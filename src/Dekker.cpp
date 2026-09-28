#include "Dekker.h"
#include <thread>

Dekker::Dekker() : turn_(0) {
    flag_[0] = false;
    flag_[1] = false;
}

// --------------------------------------------------------------------------
// lock(id): implementacion textbook del algoritmo de Dekker.
// El hilo 'id' anuncia su intencion de entrar (flag_[id]=true) y, mientras
// el otro hilo tambien quiera entrar, cede el turno segun la variable
// 'turn_'. Esto garantiza exclusion mutua SIN usar mutex del sistema
// operativo, solo con atomics y espera activa (busy-wait).
// --------------------------------------------------------------------------
void Dekker::lock(int id) {
    int otro = 1 - id;
    flag_[id] = true;
    while (flag_[otro]) {
        if (turn_ != id) {
            flag_[id] = false;
            while (turn_ != id) {
                std::this_thread::yield();
            }
            flag_[id] = true;
        }
    }
    // Región crítica protegida a partir de aquí (ej: reservar el mismo asiento)
}

void Dekker::unlock(int id) {
    turn_ = 1 - id;
    flag_[id] = false;
}
