#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <semaphore.h>
#include <string>

// ============================================================================
// Semaphore (envoltura sobre sem_t de POSIX <semaphore.h>)
// ----------------------------------------------------------------------------
// Se utiliza para limitar el AFORO MAXIMO de conexiones concurrentes que
// pueden estar realizando operaciones de I/O sobre los archivos .txt al
// mismo tiempo (evita saturar el disco / exceso de hilos abriendo el mismo
// archivo). Cada peticion HTTP que requiere leer/escribir en disco debe
// primero adquirir (wait) el semaforo y liberarlo (signal) al terminar.
// ============================================================================
class Semaphore {
public:
    explicit Semaphore(unsigned int valorInicial);
    ~Semaphore();

    void acquire(); // sem_wait
    void release(); // sem_post

private:
    sem_t sem_;
};

#endif // SEMAPHORE_H
