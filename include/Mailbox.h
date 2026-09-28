#ifndef MAILBOX_H
#define MAILBOX_H

#include <string>
#include <mqueue.h>

// ============================================================================
// Mailbox (Buzon de mensajes -- POSIX Message Queues, <mqueue.h>)
// ----------------------------------------------------------------------------
// Se utiliza como cola de mensajeria ASINCRONA para encolar:
//   - Comentarios de clientes (que luego se anexan a comentarios.txt)
//   - Registros de auditoria (que luego se anexan a reportes.txt / logs)
//
// El objetivo es que la peticion HTTP correspondiente (POST /api/.../comentario,
// o cualquier evento auditable) NO se bloquee esperando la escritura a disco:
// el hilo que atiende la peticion solo encola (mq_send) el mensaje y responde
// de inmediato; un hilo consumidor dedicado (ver main.cpp) hace mq_receive en
// un bucle y realiza la escritura real a traves de FileManager.
// ============================================================================
class Mailbox {
public:
    explicit Mailbox(const std::string& nombreCola, long maxMensajes = 32, long tamanioMax = 1024);
    ~Mailbox();

    bool enviar(const std::string& mensaje);           // mq_send (no bloqueante en la practica: cola con espacio)
    bool recibir(std::string& mensajeOut, unsigned int prioridad = 0); // mq_receive (bloqueante)

private:
    mqd_t cola_;
    std::string nombre_;
    long tamanioMax_;
};

#endif // MAILBOX_H
