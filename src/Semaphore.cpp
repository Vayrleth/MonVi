#include "Semaphore.h"

// --------------------------------------------------------------------------
// Semaforo POSIX (sem_init con pshared=0: compartido entre hilos del mismo
// proceso). Se usa para limitar el AFORO MAXIMO de hilos que pueden estar
// haciendo I/O simultanea sobre los archivos .txt.
// --------------------------------------------------------------------------
Semaphore::Semaphore(unsigned int valorInicial) {
    sem_init(&sem_, 0, valorInicial);
}

Semaphore::~Semaphore() {
    sem_destroy(&sem_);
}

void Semaphore::acquire() {
    sem_wait(&sem_); // Decrementa; bloquea si el contador llega a 0
}

void Semaphore::release() {
    sem_post(&sem_); // Incrementa; despierta a un hilo en espera si lo hay
}
