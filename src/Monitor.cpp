#include "Monitor.h"
#include <chrono>

FlightSeatManager& FlightSeatManager::getInstance() {
    static FlightSeatManager instancia;
    return instancia;
}

void FlightSeatManager::inicializarVuelo(const std::string& idVuelo, int asientosDisponibles) {
    std::lock_guard<std::mutex> lk(mtx_);
    asientosPorVuelo_[idVuelo] = asientosDisponibles;
}

// --------------------------------------------------------------------------
// reservarAsiento: MONITOR con variable de condicion. Si no hay asientos
// disponibles, el hilo se duerme (cv_.wait_for) hasta que otro hilo llame a
// liberarAsiento() (por una cancelacion) y haga notify_all(), o hasta que
// se agote el timeout.
// --------------------------------------------------------------------------
bool FlightSeatManager::reservarAsiento(const std::string& idVuelo, int timeoutMs) {
    std::unique_lock<std::mutex> lk(mtx_);
    auto haySitio = [&]() {
        auto it = asientosPorVuelo_.find(idVuelo);
        return it != asientosPorVuelo_.end() && it->second > 0;
    };

    if (!haySitio()) {
        bool listo = cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs), haySitio);
        if (!listo) return false; // timeout: no se liberaron asientos a tiempo
    }

    asientosPorVuelo_[idVuelo]--;
    return true;
}

void FlightSeatManager::liberarAsiento(const std::string& idVuelo) {
    std::lock_guard<std::mutex> lk(mtx_);
    asientosPorVuelo_[idVuelo]++;
    cv_.notify_all(); // Despierta a los hilos esperando asiento en este vuelo
}

int FlightSeatManager::consultarDisponibles(const std::string& idVuelo) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = asientosPorVuelo_.find(idVuelo);
    return it != asientosPorVuelo_.end() ? it->second : -1;
}
