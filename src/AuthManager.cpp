#include "AuthManager.h"
#include <random>
#include <sstream>
#include <mutex>

AuthManager& AuthManager::getInstance() {
    static AuthManager instancia;
    return instancia;
}

std::string AuthManager::generarTokenAleatorio() {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::ostringstream oss;
    for (int i = 0; i < 4; ++i) {
        oss << std::hex << rng();
    }
    return oss.str();
}

std::string AuthManager::crearSesion(const std::string& usuario, const std::string& rol) {
    std::string token = generarTokenAleatorio();
    std::unique_lock<std::shared_mutex> lk(mtx_); // ESCRITURA exclusiva (region critica)
    sesiones_[token] = Sesion{usuario, rol};
    return token;
}

bool AuthManager::validarToken(const std::string& token, std::string& usuarioOut, std::string& rolOut) {
    std::shared_lock<std::shared_mutex> lk(mtx_); // LECTURA compartida (varios hilos en paralelo)
    auto it = sesiones_.find(token);
    if (it == sesiones_.end()) return false;
    usuarioOut = it->second.usuario;
    rolOut = it->second.rol;
    return true;
}

void AuthManager::cerrarSesion(const std::string& token) {
    std::unique_lock<std::shared_mutex> lk(mtx_);
    sesiones_.erase(token);
}

void AuthManager::revocarSesionesDe(const std::string& usuario) {
    std::unique_lock<std::shared_mutex> lk(mtx_);
    for (auto it = sesiones_.begin(); it != sesiones_.end(); ) {
        if (it->second.usuario == usuario) {
            it = sesiones_.erase(it);
        } else {
            ++it;
        }
    }
}
