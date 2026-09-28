#ifndef AUTHMANAGER_H
#define AUTHMANAGER_H

#include <string>
#include <unordered_map>
#include <shared_mutex>

// ============================================================================
// AuthManager
// ----------------------------------------------------------------------------
// Administra las sesiones activas del sistema. Al hacer login exitoso se
// genera un token de sesion aleatorio que se asocia al usuario y su rol.
// Cada peticion protegida debe enviar ese token (header "Authorization" o
// campo "token" en el JSON) y AuthManager valida a que usuario/rol
// pertenece. Internamente protegido con std::shared_mutex (region critica
// de lectura/escritura del mapa de sesiones, muchos hilos concurrentes).
// ============================================================================
class AuthManager {
public:
    static AuthManager& getInstance();

    // Genera un token nuevo y registra la sesion. Retorna el token.
    std::string crearSesion(const std::string& usuario, const std::string& rol);

    // Retorna true si el token es valido; llena usuarioOut/rolOut.
    bool validarToken(const std::string& token, std::string& usuarioOut, std::string& rolOut);

    // Elimina una sesion (logout) o al revocar un vendedor.
    void cerrarSesion(const std::string& token);

    // Revoca TODAS las sesiones activas de un usuario dado (ej. al eliminar
    // un vendedor, el Gerente fuerza su cierre de sesion inmediato).
    void revocarSesionesDe(const std::string& usuario);

private:
    AuthManager() = default;
    struct Sesion { std::string usuario; std::string rol; };
    std::unordered_map<std::string, Sesion> sesiones_;
    std::shared_mutex mtx_;

    std::string generarTokenAleatorio();
};

#endif // AUTHMANAGER_H
