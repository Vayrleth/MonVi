#include "InterruptTable.h"
#include <signal.h>
#include <cstdio>
#include <iostream>

// --------------------------------------------------------------------------
// Bandera global atomica que se activa desde el manejador de señal
// (async-signal-safe: solo se toca un std::atomic<bool>, nada mas pesado)
// cuando llega SIGINT o SIGTERM. El bucle principal del servidor (main.cpp)
// consulta esta bandera para iniciar el apagado seguro.
// --------------------------------------------------------------------------
static std::atomic<bool> g_banderaApagado{false};

std::atomic<bool>& InterruptTable::banderaApagado() {
    return g_banderaApagado;
}

// Manejador real de señales (nivel C, requerido por sigaction/signal).
// INTERRUPCION ASINCRONA: se dispara fuera del flujo normal del programa,
// en cualquier punto de ejecucion, cuando el SO recibe SIGINT/SIGTERM.
static void manejadorApagadoSeguro(int senal) {
    (void)senal;
    g_banderaApagado.store(true);
}

// Señal secundaria de ejemplo (SIGUSR1): representa un evento de baja
// prioridad que puede ser ENMASCARADO (bloqueado temporalmente) durante
// una fase critica de I/O sobre los .txt.
static void manejadorSenalSecundaria(int senal) {
    (void)senal;
    std::cerr << "[InterruptTable] Señal secundaria SIGUSR1 recibida (evento de baja prioridad)." << std::endl;
}

InterruptTable& InterruptTable::getInstance() {
    static InterruptTable instancia;
    return instancia;
}

void InterruptTable::registrarISR(int codigoEvento, std::function<void(const std::string&)> isr) {
    std::lock_guard<std::mutex> lk(mtxTabla_);
    tablaISR_[codigoEvento] = isr; // Tabla de Vectores de Interrupcion (mapa codigo -> ISR)
}

// --------------------------------------------------------------------------
// dispararEvento: encola el evento con su prioridad y lo despacha de
// inmediato invocando la ISR registrada para ese codigo.
//
// PLANIFICACION APROPIATIVA / PRIORIDAD DE INTERRUPCIONES: los eventos con
// prioridad 0 (ej. EVT_VENDEDOR_REVOCADO disparado por el Gerente) se
// procesan de inmediato, "adelantandose" logicamente a cualquier peticion
// de lectura de baja prioridad que estuviera en cola de procesamiento.
// --------------------------------------------------------------------------
void InterruptTable::dispararEvento(int codigoEvento, const std::string& detalle, int prioridad) {
    {
        std::lock_guard<std::mutex> lk(mtxCola_);
        colaEventos_.push({prioridad, codigoEvento, detalle});
    }

    // Despachar todos los eventos pendientes en orden de prioridad
    while (true) {
        EventoPrioritario evento;
        {
            std::lock_guard<std::mutex> lk(mtxCola_);
            if (colaEventos_.empty()) break;
            evento = colaEventos_.top();
            colaEventos_.pop();
        }

        std::function<void(const std::string&)> isr;
        {
            std::lock_guard<std::mutex> lk(mtxTabla_);
            auto it = tablaISR_.find(evento.codigo);
            if (it != tablaISR_.end()) isr = it->second;
        }

        if (isr) {
            isr(evento.detalle); // Ejecuta la Rutina de Servicio de Interrupcion (ISR)
        }
    }
}

void InterruptTable::instalarManejadoresDeSenales() {
    struct sigaction accion;
    accion.sa_handler = manejadorApagadoSeguro;
    sigemptyset(&accion.sa_mask);
    accion.sa_flags = 0;

    sigaction(SIGINT, &accion, nullptr);
    sigaction(SIGTERM, &accion, nullptr);

    struct sigaction accionSecundaria;
    accionSecundaria.sa_handler = manejadorSenalSecundaria;
    sigemptyset(&accionSecundaria.sa_mask);
    accionSecundaria.sa_flags = 0;
    sigaction(SIGUSR1, &accionSecundaria, nullptr);

    std::cout << "[InterruptTable] Manejadores de señales instalados (SIGINT, SIGTERM, SIGUSR1)." << std::endl;
}

// --------------------------------------------------------------------------
// enmascararSenalesSecundarias: usa sigprocmask para BLOQUEAR SIGUSR1
// durante una fase critica (ej. escritura de historial_reservas.txt),
// evitando que la ISR de esa señal secundaria interrumpa la escritura.
// SIGINT/SIGTERM/SIGSEGV se dejan siempre desenmascaradas: son tratadas
// como interrupciones "no enmascarables" a nivel de esta aplicacion,
// porque deben poder apagar el sistema o reportar fallos criticos de
// inmediato incluso durante una region critica de I/O.
// --------------------------------------------------------------------------
void InterruptTable::enmascararSenalesSecundarias() {
    sigset_t conjunto;
    sigemptyset(&conjunto);
    sigaddset(&conjunto, SIGUSR1);
    sigprocmask(SIG_BLOCK, &conjunto, nullptr);
}

void InterruptTable::restaurarSenales() {
    sigset_t conjunto;
    sigemptyset(&conjunto);
    sigaddset(&conjunto, SIGUSR1);
    sigprocmask(SIG_UNBLOCK, &conjunto, nullptr);
}
