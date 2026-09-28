#include "Pipes.h"
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <cstdio>
#include <vector>

// ============================================================================
// ReportPipe::ejecutarReporte
// ----------------------------------------------------------------------------
// Demuestra 4 conceptos de SO simultaneamente:
//   1) PROCESOS PESADOS (fork): se crea un proceso hijo completamente
//      independiente (memoria propia) distinto al Servidor Principal.
//   2) exec(): el proceso hijo reemplaza su imagen por el binario
//      "process_reports", es decir, EXCLUSION MUTUA ENTRE PROCESOS: el
//      hijo ya no comparte memoria con el padre, solo se comunican por
//      los pipes.
//   3) TUBERIAS POSIX (pipe()): se crean dos pipes, uno para enviar el
//      payload (metricas/parametros del reporte) del padre al hijo
//      (stdin del hijo) y otro para recibir la salida generada por el
//      hijo (stdout del hijo) de vuelta al padre.
//   4) MENSAJES: el 'payload' viaja como un mensaje de texto estructurado
//      (protocolo simple: líneas separadas por '\n').
// ============================================================================
std::string ReportPipe::ejecutarReporte(const std::string& payload) {
    int pipeEntrada[2]; // padre -> hijo (stdin del hijo)
    int pipeSalida[2];  // hijo -> padre (stdout del hijo)

    if (pipe(pipeEntrada) == -1 || pipe(pipeSalida) == -1) {
        return "{\"status\":\"error\",\"mensaje\":\"No se pudo crear el pipe\"}";
    }

    pid_t pid = fork(); // PROCESO PESADO independiente

    if (pid < 0) {
        return "{\"status\":\"error\",\"mensaje\":\"fork() fallo\"}";
    }

    if (pid == 0) {
        // ---------------- PROCESO HIJO ----------------
        close(pipeEntrada[1]); // el hijo no escribe en su propia entrada
        close(pipeSalida[0]);  // el hijo no lee su propia salida

        dup2(pipeEntrada[0], STDIN_FILENO);
        dup2(pipeSalida[1], STDOUT_FILENO);

        close(pipeEntrada[0]);
        close(pipeSalida[1]);

        // exec(): reemplaza la imagen del proceso por process_reports
        execlp("./process_reports", "process_reports", nullptr);

        // Si execlp falla, terminamos el hijo para no duplicar el padre
        _exit(127);
    }

    // ---------------- PROCESO PADRE (Servidor Principal) ----------------
    close(pipeEntrada[0]); // el padre no lee de la entrada del hijo
    close(pipeSalida[1]);  // el padre no escribe en la salida del hijo

    // Enviar el payload (mensaje) por el pipe de entrada del hijo
    write(pipeEntrada[1], payload.c_str(), payload.size());
    close(pipeEntrada[1]); // EOF para el hijo -> sabe que ya recibio todo

    // Leer la respuesta completa que el hijo escribe en su stdout
    std::string resultado;
    char buffer[4096];
    ssize_t leidos;
    while ((leidos = read(pipeSalida[0], buffer, sizeof(buffer))) > 0) {
        resultado.append(buffer, leidos);
    }
    close(pipeSalida[0]);

    int estado;
    waitpid(pid, &estado, 0); // esperar a que el hijo termine (evita zombies)

    if (resultado.empty()) {
        resultado = "{\"status\":\"error\",\"mensaje\":\"El proceso de reportes no devolvio datos\"}";
    }

    return resultado;
}
