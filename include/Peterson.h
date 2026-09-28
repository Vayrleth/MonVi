#ifndef PETERSON_H
#define PETERSON_H

#include <atomic>

// ============================================================================
// Algoritmo de Peterson
// ----------------------------------------------------------------------------
// Alternativa mas simple que Dekker para exclusion mutua entre 2 hilos.
// Se utiliza en este proyecto para proteger la SECCION CRITICA de
// TRANSFERENCIA DE RESERVAS entre dos vuelos (vendedor.html), que es
// precisamente el escenario donde se demuestra el DEADLOCK controlado
// (dos transferencias cruzadas que se bloquean mutuamente).
// ============================================================================
class Peterson {
public:
    Peterson();

    void lock(int id);   // id: 0 o 1
    void unlock(int id);

private:
    std::atomic<bool> flag_[2];
    std::atomic<int> turn_;
};

#endif // PETERSON_H
