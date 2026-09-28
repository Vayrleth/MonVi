#ifndef DEKKER_H
#define DEKKER_H

#include <atomic>

// ============================================================================
// Algoritmo de Dekker
// ----------------------------------------------------------------------------
// Exclusion mutua CLASICA entre exactamente 2 hilos sin usar mutex del SO,
// solo con variables atomicas compartidas (flag[2] y turn). Se utiliza en
// este proyecto para arbitrar el caso de que DOS HILOS intenten reservar el
// MISMO ASIENTO del MISMO VUELO al mismo tiempo (condicion de carrera
// clasica de "double booking").
//
// id debe ser 0 o 1 (identifica cual de los dos hilos entra).
// ============================================================================
class Dekker {
public:
    Dekker();

    void lock(int id);   // id: 0 o 1
    void unlock(int id);

private:
    std::atomic<bool> flag_[2];
    std::atomic<int> turn_;
};

#endif // DEKKER_H
