#include "RestServer.h"
#include "InterruptTable.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>

RestServer::RestServer(int puerto) : puerto_(puerto), serverFd_(-1), corriendo_(false) {}

RestServer::~RestServer() {
    if (serverFd_ != -1) close(serverFd_);
}

void RestServer::registrarRuta(const std::string& metodo, const std::string& ruta, RutaHandler handler) {
    rutas_[metodo + " " + ruta] = handler;
}

void RestServer::detener() {
    corriendo_ = false;
    if (serverFd_ != -1) {
        shutdown(serverFd_, SHUT_RDWR);
        close(serverFd_);
        serverFd_ = -1;
    }
}

// ---------------------------------------------------------------------------
// iniciar(): crea el socket TCP/IP (SOCKETS), hace bind() en el puerto
// indicado y entra en un bucle accept() -> por cada conexion nueva, crea un
// std::thread independiente que la atiende (CONCURRENCIA / PROCESOS
// LIGEROS), permitiendo servir a multiples clientes web simultaneamente.
// ---------------------------------------------------------------------------
void RestServer::iniciar() {
    serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd_ < 0) {
        std::cerr << "[RestServer] Error creando el socket." << std::endl;
        return;
    }

    int opt = 1;
    setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in direccion{};
    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = INADDR_ANY;
    direccion.sin_port = htons(puerto_);

    if (bind(serverFd_, (struct sockaddr*)&direccion, sizeof(direccion)) < 0) {
        std::cerr << "[RestServer] Error en bind() puerto " << puerto_ << std::endl;
        return;
    }

    if (listen(serverFd_, 128) < 0) {
        std::cerr << "[RestServer] Error en listen()." << std::endl;
        return;
    }

    corriendo_ = true;
    std::cout << "[RestServer] Servidor escuchando en http://0.0.0.0:" << puerto_ << std::endl;

    std::vector<std::thread> hilosClientes;

    while (corriendo_ && !InterruptTable::banderaApagado().load()) {
        sockaddr_in clienteAddr{};
        socklen_t clienteLen = sizeof(clienteAddr);
        int clienteFd = accept(serverFd_, (struct sockaddr*)&clienteAddr, &clienteLen);

        if (clienteFd < 0) {
            if (!corriendo_) break; // socket cerrado por detener()
            continue;
        }

        // PROCESO LIGERO (thread) dedicado a esta peticion HTTP
        hilosClientes.emplace_back([this, clienteFd]() {
            this->atenderCliente(clienteFd);
        });
        hilosClientes.back().detach();
    }

    std::cout << "[RestServer] Apagado seguro completado." << std::endl;
}

void RestServer::atenderCliente(int clienteFd) {
    char buffer[8192];
    std::string crudo;

    ssize_t leidos = read(clienteFd, buffer, sizeof(buffer) - 1);
    if (leidos > 0) {
        buffer[leidos] = '\0';
        crudo.assign(buffer, leidos);

        // Si el header indica Content-Length mayor a lo ya leido, seguimos leyendo
        size_t posCL = crudo.find("Content-Length:");
        if (posCL != std::string::npos) {
            size_t inicio = posCL + strlen("Content-Length:");
            size_t fin = crudo.find("\r\n", inicio);
            int contentLength = 0;
            try { contentLength = std::stoi(crudo.substr(inicio, fin - inicio)); } catch (...) {}

            size_t posCuerpo = crudo.find("\r\n\r\n");
            if (posCuerpo != std::string::npos) {
                size_t cuerpoActual = crudo.size() - (posCuerpo + 4);
                while ((int)cuerpoActual < contentLength) {
                    ssize_t masLeidos = read(clienteFd, buffer, sizeof(buffer) - 1);
                    if (masLeidos <= 0) break;
                    crudo.append(buffer, masLeidos);
                    cuerpoActual += masLeidos;
                }
            }
        }
    }

    if (!crudo.empty()) {
        HttpRequest req = parsearPeticion(crudo);
        HttpResponse resp;

        // Archivos estaticos del frontend (todo lo que no empiece con /api/)
        if (req.ruta.rfind("/api/", 0) != 0) {
            std::string contenido = servirArchivoEstatico(req.ruta);
            if (!contenido.empty()) {
                std::string tipoMime = "text/plain";
                if (req.ruta.size() >= 5 && req.ruta.substr(req.ruta.size()-5) == ".html") tipoMime = "text/html";
                else if (req.ruta.size() >= 4 && req.ruta.substr(req.ruta.size()-4) == ".css") tipoMime = "text/css";
                else if (req.ruta.size() >= 3 && req.ruta.substr(req.ruta.size()-3) == ".js") tipoMime = "application/javascript";
                else if (req.ruta.size() >= 4 && req.ruta.substr(req.ruta.size()-4) == ".jpg") tipoMime = "image/jpeg";
                else if (req.ruta.size() >= 4 && req.ruta.substr(req.ruta.size()-4) == ".png") tipoMime = "image/png";

                std::string encabezado = "HTTP/1.1 200 OK\r\nContent-Type: " + tipoMime + "\r\nContent-Length: " +
                    std::to_string(contenido.size()) + "\r\nConnection: close\r\n\r\n";
                write(clienteFd, encabezado.c_str(), encabezado.size());
                write(clienteFd, contenido.c_str(), contenido.size());
                close(clienteFd);
                return;
            }
        }

        auto it = rutas_.find(req.metodo + " " + req.ruta);
        if (it != rutas_.end()) {
            try {
                resp = it->second(req);
            } catch (const std::exception& e) {
                // INTERRUPCION SINCRONA (excepcion) capturada: nunca tumba el hilo/servidor
                resp.codigo = 500;
                resp.cuerpoJson = std::string("{\"status\":\"error\",\"mensaje\":\"Excepcion interna: ") + e.what() + "\"}";
            }
        } else {
            resp.codigo = 404;
            resp.cuerpoJson = "{\"status\":\"error\",\"mensaje\":\"Ruta no encontrada\"}";
        }

        std::string respuestaCompleta = construirRespuesta(resp);
        write(clienteFd, respuestaCompleta.c_str(), respuestaCompleta.size());
    }

    close(clienteFd);
}

HttpRequest RestServer::parsearPeticion(const std::string& crudo) {
    HttpRequest req;
    std::istringstream stream(crudo);
    std::string lineaInicial;
    std::getline(stream, lineaInicial);

    std::istringstream lineaSS(lineaInicial);
    std::string rutaCompleta;
    lineaSS >> req.metodo >> rutaCompleta;
    // Descartar query string si existe (?param=valor)
    size_t signo = rutaCompleta.find('?');
    req.ruta = (signo == std::string::npos) ? rutaCompleta : rutaCompleta.substr(0, signo);

    std::string linea;
    while (std::getline(stream, linea) && linea != "\r" && !linea.empty()) {
        size_t sep = linea.find(':');
        if (sep != std::string::npos) {
            std::string clave = linea.substr(0, sep);
            std::string valor = linea.substr(sep + 1);
            if (!valor.empty() && valor.back() == '\r') valor.pop_back();
            if (!valor.empty() && valor.front() == ' ') valor.erase(0, 1);
            req.headers[clave] = valor;
        }
    }

    size_t posCuerpo = crudo.find("\r\n\r\n");
    if (posCuerpo != std::string::npos) {
        req.cuerpo = crudo.substr(posCuerpo + 4);
    }

    return req;
}

std::string RestServer::construirRespuesta(const HttpResponse& resp) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << resp.codigo << " " << (resp.codigo == 200 ? "OK" : "ERROR") << "\r\n";
    oss << "Content-Type: application/json\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "Access-Control-Allow-Headers: *\r\n";
    oss << "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n";
    oss << "Content-Length: " << resp.cuerpoJson.size() << "\r\n";
    oss << "Connection: close\r\n\r\n";
    oss << resp.cuerpoJson;
    return oss.str();
}

std::string RestServer::servirArchivoEstatico(const std::string& rutaPedida) {
    std::string ruta = rutaPedida;
    if (ruta == "/") ruta = "/index.html";

    std::string rutaFisica = "public" + ruta;
    std::ifstream archivo(rutaFisica, std::ios::binary);
    if (!archivo.is_open()) return "";

    std::ostringstream oss;
    oss << archivo.rdbuf();
    return oss.str();
}
