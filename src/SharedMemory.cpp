#include "SharedMemory.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <mutex>

static std::mutex mtxShmLocal_;

// ============================================================================
// SharedFlightCache
// ----------------------------------------------------------------------------
// MEMORIA COMPARTIDA POSIX: usa shm_open() para crear/abrir un objeto de
// memoria compartida con nombre (/aerolinea_vuelos_cache), lo mapea a
// direcciones virtuales del proceso con mmap(), y expone lectura/escritura
// de una cadena JSON con el catalogo de vuelos activos. Cualquier otro
// proceso (por ejemplo un futuro proceso de reportes) que abra el mismo
// nombre de memoria compartida puede leer el mismo contenido sin volver a
// tocar disco.
// ============================================================================
SharedFlightCache::SharedFlightCache() : mapPtr_(nullptr), fd_(-1) {
    fd_ = shm_open(NOMBRE_SHM, O_CREAT | O_RDWR, 0666);
    if (fd_ != -1) {
        ftruncate(fd_, TAMANIO);
        mapPtr_ = mmap(nullptr, TAMANIO, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (mapPtr_ == MAP_FAILED) {
            mapPtr_ = nullptr;
        }
    }
}

SharedFlightCache::~SharedFlightCache() {
    if (mapPtr_) munmap(mapPtr_, TAMANIO);
    if (fd_ != -1) close(fd_);
}

SharedFlightCache& SharedFlightCache::getInstance() {
    static SharedFlightCache instancia;
    return instancia;
}

bool SharedFlightCache::actualizar(const std::string& jsonVuelos) {
    std::lock_guard<std::mutex> lk(mtxShmLocal_);
    if (!mapPtr_) return false;
    if (jsonVuelos.size() >= TAMANIO) return false; // no cabe en el segmento
    memset(mapPtr_, 0, TAMANIO);
    memcpy(mapPtr_, jsonVuelos.c_str(), jsonVuelos.size());
    return true;
}

std::string SharedFlightCache::leer() {
    std::lock_guard<std::mutex> lk(mtxShmLocal_);
    if (!mapPtr_) return "";
    return std::string(static_cast<char*>(mapPtr_));
}
