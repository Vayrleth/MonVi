#ifndef RESTSERVER_H
#define RESTSERVER_H

#include <string>
#include <functional>
#include <map>
#include <atomic>

// ============================================================================
// RestServer
// ----------------------------------------------------------------------------
// Servidor HTTP minimalista implementado directamente sobre SOCKETS TCP/IP
// POSIX (<sys/socket.h>), sin dependencias externas. Expone la API REST
// que consume el frontend (public/js/*.js via fetch()).
//
// CONCURRENCIA / PROCESOS LIGEROS: por cada conexion aceptada (accept()) se
// lanza un std::thread independiente que atiende esa peticion HTTP
// completa (parseo, ruteo, respuesta) y termina (detached o joined en
// background), permitiendo atender MULTIPLES CLIENTES SIMULTANEAMENTE
// (Multiprogramacion / Concurrencia).
//
// PLANIFICACION APROPIATIVA: las peticiones marcadas como administrativas
// criticas (revocar vendedor, editar vuelo con bandera urgente) se
// despachan a traves de InterruptTable con prioridad 0, lo cual friega/
// posterga el procesamiento de peticiones de solo lectura en cola (ver
// main.cpp / InterruptTable).
// ============================================================================

struct HttpRequest {
    std::string metodo;   // GET, POST, DELETE, ...
    std::string ruta;     // /api/...
    std::map<std::string, std::string> headers;
    std::string cuerpo;   // body JSON crudo
};

struct HttpResponse {
    int codigo = 200;
    std::string cuerpoJson = "{}";
};

using RutaHandler = std::function<HttpResponse(const HttpRequest&)>;

class RestServer {
public:
    RestServer(int puerto);
    ~RestServer();

    // Registra un handler para metodo+ruta exacta (ej. "GET", "/api/vuelos")
    void registrarRuta(const std::string& metodo, const std::string& ruta, RutaHandler handler);

    // Inicia el servidor (bind/listen/accept loop). Bloqueante.
    void iniciar();

    // Solicita apagado seguro (usado por manejador de SIGINT/SIGTERM).
    void detener();

private:
    void atenderCliente(int clienteFd);
    HttpRequest parsearPeticion(const std::string& crudo);
    std::string construirRespuesta(const HttpResponse& resp);
    std::string servirArchivoEstatico(const std::string& ruta);

    int puerto_;
    int serverFd_;
    std::atomic<bool> corriendo_;
    std::map<std::string, RutaHandler> rutas_; // clave: "METODO RUTA"
};

#endif // RESTSERVER_H
