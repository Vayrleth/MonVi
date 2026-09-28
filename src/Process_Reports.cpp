// ============================================================================
// Process_Reports.cpp
// ----------------------------------------------------------------------------
// PROCESO PESADO INDEPENDIENTE (Heavyweight Process). Se compila como un
// binario separado ("process_reports") y es lanzado por el Servidor
// Principal mediante fork()+exec() (ver src/Pipes.cpp).
//
// Este proceso NO comparte memoria con el servidor principal (exclusion
// mutua entre procesos): la unica via de comunicacion es a traves de los
// PIPES POSIX heredados como stdin/stdout.
//
// Protocolo de MENSAJES (simple, basado en texto):
//   El padre escribe por stdin lineas con el formato:
//       ACCION|param1|param2|...
//   y este proceso responde por stdout un JSON con el resultado.
// ============================================================================
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>

static std::vector<std::string> dividir(const std::string& linea, char sep) {
    std::vector<std::string> partes;
    std::stringstream ss(linea);
    std::string parte;
    while (std::getline(ss, parte, sep)) partes.push_back(parte);
    return partes;
}

static int contarLineas(const std::string& ruta) {
    std::ifstream archivo(ruta);
    int contador = 0;
    std::string linea;
    while (std::getline(archivo, linea)) {
        if (!linea.empty()) contador++;
    }
    return contador;
}

int main() {
    // Leer TODO lo que el padre envio por el pipe (stdin) hasta EOF
    std::ostringstream entradaCompleta;
    std::string linea;
    while (std::getline(std::cin, linea)) {
        entradaCompleta << linea << "\n";
    }

    std::string payload = entradaCompleta.str();
    std::vector<std::string> partes = dividir(payload, '|');
    std::string accion = partes.empty() ? "GENERAL" : partes[0];

    // Genera metricas simples a partir de los archivos .txt (base de datos)
    int totalVuelos      = contarLineas("data/vuelos.txt");
    int totalReservasHist = contarLineas("data/historial_reservas.txt");
    int totalVentasVend   = contarLineas("data/historial_vendedores.txt");
    int totalUsuarios     = contarLineas("data/usuarios.txt");
    int totalComentarios  = contarLineas("data/comentarios.txt");

    std::ostringstream json;
    json << "{"
         << "\"status\":\"success\","
         << "\"generado_por\":\"process_reports (proceso independiente via fork+exec)\","
         << "\"accion_recibida\":\"" << accion << "\","
         << "\"reporte\":{"
         << "\"total_vuelos\":" << totalVuelos << ","
         << "\"total_reservas_historicas\":" << totalReservasHist << ","
         << "\"total_ventas_registradas\":" << totalVentasVend << ","
         << "\"total_usuarios\":" << totalUsuarios << ","
         << "\"total_comentarios\":" << totalComentarios
         << "}"
         << "}";

    // La respuesta se escribe en stdout, que el padre lee del otro extremo del pipe
    std::cout << json.str() << std::endl;
    return 0;
}
