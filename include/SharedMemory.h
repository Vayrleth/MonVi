#ifndef SHAREDMEMORY_H
#define SHAREDMEMORY_H

#include <string>
#include <cstddef>

// ============================================================================
// SharedMemory (Memoria Compartida POSIX: shm_open / mmap)
// ----------------------------------------------------------------------------
// Mantiene una CACHE en memoria compartida entre procesos con el contenido
// serializado (JSON) del catalogo de vuelos activos, cargado desde
// vuelos.txt. Esto permite que, en un futuro, otros procesos (por ejemplo
// el proceso de reportes) puedan leer el catalogo de vuelos sin tener que
// volver a parsear el archivo .txt, acelerando la consulta.
// ============================================================================
class SharedFlightCache {
public:
    static SharedFlightCache& getInstance();

    // Escribe (actualiza) el contenido cacheado en la memoria compartida.
    bool actualizar(const std::string& jsonVuelos);

    // Lee el contenido actualmente cacheado.
    std::string leer();

    ~SharedFlightCache();

private:
    SharedFlightCache();
    SharedFlightCache(const SharedFlightCache&) = delete;
    SharedFlightCache& operator=(const SharedFlightCache&) = delete;

    void* mapPtr_;
    int fd_;
    static constexpr size_t TAMANIO = 65536; // 64 KB
    static constexpr const char* NOMBRE_SHM = "/aerolinea_vuelos_cache";
};

#endif // SHAREDMEMORY_H
