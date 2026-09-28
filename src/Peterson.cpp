#include "Peterson.h"
#include <thread>

Peterson::Peterson() : turn_(0) {
    flag_[0] = false;
    flag_[1] = false;
}

// --------------------------------------------------------------------------
// lock(id): algoritmo de Peterson. Similar en espiritu a Dekker pero mas
// simple: el hilo anuncia interes, cede el turno al otro y espera mientras
// el otro tambien tenga interes Y sea su turno.
// Se usa aqui para proteger la transferencia de reservas entre vuelos
// (vendedor.html), escenario donde se demuestra un deadlock controlado.
// --------------------------------------------------------------------------
void Peterson::lock(int id) {
    int otro = 1 - id;
    flag_[id] = true;
    turn_ = otro;
    while (flag_[otro] && turn_ == otro) {
        std::this_thread::yield();
    }
}

void Peterson::unlock(int id) {
    flag_[id] = false;
}
