#ifndef INTERRUPTTABLE_H
#define INTERRUPTTABLE_H

#include <functional>
#include <map>
#include <string>
#include <atomic>
#include <queue>
#include <mutex>

// ============================================================================
// InterruptTable
// ----------------------------------------------------------------------------
// Simula una TABLA DE VECTORES DE INTERRUPCION (Interrupt Vector Table): un
// mapa que asocia un codigo de evento entero con su RUTINA DE SERVICIO DE
// INTERRUPCION (ISR) correspondiente. Los "eventos" de este sistema son,
// por ejemplo: EVT_VUELO_MODIFICADO, EVT_VENDEDOR_REVOCADO, EVT_ERROR_IO,
// EVT_SHUTDOWN, etc.
//
// Ademas gestiona:
//  - INTERRUPCIONES ASINCRONAS / SEÑALES POSIX: SIGINT y SIGTERM se capturan
//    para apagar el servidor de forma segura (graceful shutdown), volcando
//    los buffers pendientes a los archivos .txt antes de terminar.
//  - INTERRUPCIONES ENMASCARABLES / NO ENMASCARABLES: durante fases criticas
//    de escritura en los .txt se bloquean temporalmente señales secundarias
//    (SIGUSR1) con sigprocmask, mientras que los fallos criticos (SIGSEGV,
//    tratados aqui como "no enmascarables" a nivel logico de aplicacion)
//    siempre se atienden de inmediato.
//  - PRIORIDAD Y SERVICIO DE INTERRUPCIONES: una cola de eventos con
//    prioridad, donde los eventos administrativos criticos (ej. revocar un
//    vendedor) se procesan ANTES que los eventos de baja prioridad (ej.
//    peticiones de lectura en cola).
// ============================================================================

enum CodigoEvento {
    EVT_VUELO_MODIFICADO   = 1,
    EVT_VENDEDOR_REVOCADO  = 2,   // Prioridad ALTA (preemptive)
    EVT_RESERVA_CREADA     = 3,
    EVT_RESERVA_CANCELADA  = 4,
    EVT_ERROR_IO           = 5,   // Interrupcion sincrona (excepcion de parsing)
    EVT_SHUTDOWN           = 6    // Interrupcion asincrona (SIGINT/SIGTERM)
};

struct EventoPrioritario {
    int prioridad;      // 0 = maxima prioridad (administrativa/critica)
    int codigo;
    std::string detalle;
    bool operator<(const EventoPrioritario& otro) const {
        // priority_queue es max-heap; invertimos para que MENOR numero = MAYOR prioridad
        return prioridad > otro.prioridad;
    }
};

class InterruptTable {
public:
    static InterruptTable& getInstance();

    // Registra una ISR (rutina de servicio) para un codigo de evento dado.
    void registrarISR(int codigoEvento, std::function<void(const std::string&)> isr);

    // "Dispara" el evento: lo encola con su prioridad y lo despacha.
    // Los eventos de prioridad 0 (administrativos) se atienden de forma
    // apropiativa (preemptive), antes que cualquier evento pendiente de
    // prioridad mayor (numero mas alto).
    void dispararEvento(int codigoEvento, const std::string& detalle, int prioridad = 5);

    // Instala los manejadores de señales POSIX SIGINT/SIGTERM para apagado
    // seguro, y SIGUSR1 como señal secundaria enmascarable de ejemplo.
    void instalarManejadoresDeSenales();

    // Bloquea (mascara) temporalmente señales secundarias no criticas
    // durante una fase critica de I/O sobre los archivos .txt.
    void enmascararSenalesSecundarias();
    void restaurarSenales();

    static std::atomic<bool>& banderaApagado(); // true cuando llego SIGINT/SIGTERM

private:
    InterruptTable() = default;
    std::map<int, std::function<void(const std::string&)>> tablaISR_;
    std::mutex mtxTabla_;
    std::priority_queue<EventoPrioritario> colaEventos_;
    std::mutex mtxCola_;
};

#endif // INTERRUPTTABLE_H
