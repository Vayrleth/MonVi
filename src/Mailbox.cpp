#include "Mailbox.h"
#include <fcntl.h>
#include <cstring>
#include <vector>

// ============================================================================
// Mailbox
// ----------------------------------------------------------------------------
// Envoltura sobre las colas de mensajes POSIX (mq_open/mq_send/mq_receive).
// Se usa para desacoplar la escritura en disco (comentarios.txt, logs de
// auditoria) del hilo que atiende la peticion HTTP: el hilo HTTP solo hace
// enviar() (encola el mensaje y regresa de inmediato) y un hilo consumidor
// dedicado, corriendo en background en main.cpp, hace recibir() en bucle y
// delega la escritura real a FileManager.
// ============================================================================
Mailbox::Mailbox(const std::string& nombreCola, long maxMensajes, long tamanioMax)
    : nombre_(nombreCola), tamanioMax_(tamanioMax) {
    struct mq_attr atributos;
    atributos.mq_flags = 0;
    atributos.mq_maxmsg = maxMensajes;
    atributos.mq_msgsize = tamanioMax;
    atributos.mq_curmsgs = 0;

    // O_CREAT: crea la cola si no existe; permisos 0666
    cola_ = mq_open(nombre_.c_str(), O_CREAT | O_RDWR, 0666, &atributos);
}

Mailbox::~Mailbox() {
    if (cola_ != (mqd_t)-1) {
        mq_close(cola_);
    }
}

bool Mailbox::enviar(const std::string& mensaje) {
    if (cola_ == (mqd_t)-1) return false;
    std::string msg = mensaje;
    if ((long)msg.size() > tamanioMax_) {
        msg = msg.substr(0, tamanioMax_);
    }
    // Prioridad 0 por defecto: FIFO simple entre mensajes de comentarios/logs
    return mq_send(cola_, msg.c_str(), msg.size(), 0) == 0;
}

bool Mailbox::recibir(std::string& mensajeOut, unsigned int prioridad) {
    if (cola_ == (mqd_t)-1) return false;
    std::vector<char> buffer(tamanioMax_);
    unsigned int prio = prioridad;
    ssize_t bytesLeidos = mq_receive(cola_, buffer.data(), tamanioMax_, &prio);
    if (bytesLeidos < 0) return false;
    mensajeOut.assign(buffer.data(), bytesLeidos);
    return true;
}
