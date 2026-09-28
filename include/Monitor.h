#ifndef MONITOR_H
#define MONITOR_H

#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <string>

// ============================================================================
// FlightSeatManager (Monitor)
// ----------------------------------------------------------------------------
// Implementa el patron de MONITOR: una clase que encapsula un mutex interno
// y una variable de condicion, exponiendo unicamente metodos de alto nivel
// que garantizan que el acceso al estado interno (asientos disponibles por
// vuelo) siempre ocurra en exclusion mutua.
//
// Los hilos que intentan reservar un asiento cuando NO hay disponibilidad
// pueden esperar (wait) en la variable de condicion hasta que otro hilo
// libere un asiento (por cancelacion) y haga notify().
// ============================================================================
class FlightSeatManager {
public:
    static FlightSeatManager& getInstance();

    void inicializarVuelo(const std::string& idVuelo, int asientosDisponibles);

    // Intenta reservar un asiento. Si no hay, espera hasta 'timeoutMs' ms
    // a que se libere alguno (notify por cancelacion). Retorna false si
    // se agota el tiempo de espera.
    bool reservarAsiento(const std::string& idVuelo, int timeoutMs = 200);

    // Libera (devuelve) un asiento -- por cancelacion -- y notifica a
    // los hilos que pudieran estar esperando (wait) por ese vuelo.
    void liberarAsiento(const std::string& idVuelo);

    int consultarDisponibles(const std::string& idVuelo);

private:
    FlightSeatManager() = default;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::unordered_map<std::string, int> asientosPorVuelo_;
};

#endif // MONITOR_H
