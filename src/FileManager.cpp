#include "FileManager.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iostream>

FileManager::FileManager() : dataDir_("data/") {}

FileManager& FileManager::getInstance() {
    static FileManager instancia;
    return instancia;
}

// ---------------------------------------------------------------------------
// Utilidad interna: parsea un archivo CSV simple (campos separados por
// comas, una fila por linea) SIN sostener ningun mutex -- el mutex
// correspondiente debe adquirirse en el metodo publico que llama a esta
// funcion.
// ---------------------------------------------------------------------------
std::vector<FileManager::Row> FileManager::leerArchivo(const std::string& ruta) {
    std::vector<Row> filas;
    std::ifstream archivo(ruta);
    std::string linea;
    while (std::getline(archivo, linea)) {
        if (linea.empty()) continue;
        Row fila;
        std::stringstream ss(linea);
        std::string campo;
        while (std::getline(ss, campo, ',')) {
            fila.push_back(campo);
        }
        filas.push_back(fila);
    }
    return filas;
}

void FileManager::escribirArchivoCompleto(const std::string& ruta, const std::vector<Row>& filas) {
    std::ofstream archivo(ruta, std::ios::trunc);
    for (const auto& fila : filas) {
        for (size_t i = 0; i < fila.size(); ++i) {
            archivo << fila[i];
            if (i + 1 < fila.size()) archivo << ",";
        }
        archivo << "\n";
    }
}

void FileManager::anexarLinea(const std::string& ruta, const std::string& linea, std::shared_mutex& mtx) {
    std::unique_lock<std::shared_mutex> lk(mtx); // REGION CRITICA: escritura exclusiva
    std::ofstream archivo(ruta, std::ios::app);
    archivo << linea << "\n";
}

static std::string timestampActual() {
    auto ahora = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(ahora);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return std::string(buffer);
}

// ============================ USUARIOS ====================================

bool FileManager::validarLogin(const std::string& usuario, const std::string& pass, std::string& rolOut) {
    std::shared_lock<std::shared_mutex> lk(mtxUsuarios_); // LECTURA compartida
    auto filas = leerArchivo(dataDir_ + "usuarios.txt");
    for (auto& fila : filas) {
        if (fila.size() >= 4 && fila[0] == usuario && fila[1] == pass && fila[3] == "ACTIVO") {
            rolOut = fila[2];
            return true;
        }
    }
    return false;
}

bool FileManager::existeUsuario(const std::string& usuario) {
    std::shared_lock<std::shared_mutex> lk(mtxUsuarios_);
    auto filas = leerArchivo(dataDir_ + "usuarios.txt");
    for (auto& fila : filas) {
        if (!fila.empty() && fila[0] == usuario) return true;
    }
    return false;
}

bool FileManager::registrarCliente(const std::string& usuario, const std::string& pass) {
    if (existeUsuario(usuario)) return false;
    std::string linea = usuario + "," + pass + ",CLIENTE,ACTIVO";
    anexarLinea(dataDir_ + "usuarios.txt", linea, mtxUsuarios_);
    return true;
}

// ============================ VUELOS =======================================

std::vector<FileManager::Row> FileManager::leerVuelos() {
    std::shared_lock<std::shared_mutex> lk(mtxVuelos_);
    return leerArchivo(dataDir_ + "vuelos.txt");
}

// REGION CRITICA: actualiza el numero de asientos disponibles de un vuelo.
// delta negativo = reservar (resta), delta positivo = cancelar (suma).
bool FileManager::actualizarAsientos(const std::string& idVuelo, int delta) {
    std::unique_lock<std::shared_mutex> lk(mtxVuelos_); // ESCRITURA exclusiva total
    auto filas = leerArchivo(dataDir_ + "vuelos.txt");
    bool encontrado = false;
    for (auto& fila : filas) {
        if (fila.size() >= 6 && fila[0] == idVuelo) {
            // Columnas de vuelos.txt: id,origen,destino,asientos,precio,estado
            int asientos = std::stoi(fila[3]);
            asientos += delta;
            if (asientos < 0) return false; // no hay disponibilidad
            fila[3] = std::to_string(asientos);
            encontrado = true;
            break;
        }
    }
    if (!encontrado) return false;
    escribirArchivoCompleto(dataDir_ + "vuelos.txt", filas);
    return true;
}

bool FileManager::crearOEditarVuelo(const Row& vuelo) {
    std::unique_lock<std::shared_mutex> lk(mtxVuelos_);
    auto filas = leerArchivo(dataDir_ + "vuelos.txt");
    bool actualizado = false;
    for (auto& fila : filas) {
        if (!fila.empty() && !vuelo.empty() && fila[0] == vuelo[0]) {
            fila = vuelo;
            actualizado = true;
            break;
        }
    }
    if (!actualizado) filas.push_back(vuelo);
    escribirArchivoCompleto(dataDir_ + "vuelos.txt", filas);
    return true;
}

// ============================ RESERVACIONES ================================

std::vector<FileManager::Row> FileManager::leerReservaciones() {
    std::shared_lock<std::shared_mutex> lk(mtxReservaciones_);
    return leerArchivo(dataDir_ + "reservaciones.txt");
}

bool FileManager::crearReserva(const Row& reserva) {
    std::unique_lock<std::shared_mutex> lk(mtxReservaciones_);
    std::ofstream archivo(dataDir_ + "reservaciones.txt", std::ios::app);
    for (size_t i = 0; i < reserva.size(); ++i) {
        archivo << reserva[i];
        if (i + 1 < reserva.size()) archivo << ",";
    }
    archivo << "\n";
    return true;
}

bool FileManager::cancelarReserva(const std::string& idReserva) {
    std::unique_lock<std::shared_mutex> lk(mtxReservaciones_);
    auto filas = leerArchivo(dataDir_ + "reservaciones.txt");
    std::vector<Row> restantes;
    bool encontrada = false;
    for (auto& fila : filas) {
        if (!fila.empty() && fila[0] == idReserva) {
            encontrada = true;
            continue; // se elimina de reservaciones activas
        }
        restantes.push_back(fila);
    }
    if (encontrada) escribirArchivoCompleto(dataDir_ + "reservaciones.txt", restantes);
    return encontrada;
}

// -------- HISTORIAL DE RESERVAS: INMUTABLE (solo append, nunca se borra) ---
void FileManager::anexarHistorialReserva(const Row& registro) {
    std::string linea;
    for (size_t i = 0; i < registro.size(); ++i) {
        linea += registro[i];
        if (i + 1 < registro.size()) linea += ",";
    }
    // timestampActual() ya viene incluido por el caller como primer/segundo campo
    anexarLinea(dataDir_ + "historial_reservas.txt", linea, mtxHistorialReservas_);
}

std::vector<FileManager::Row> FileManager::leerHistorialReservas() {
    std::shared_lock<std::shared_mutex> lk(mtxHistorialReservas_);
    return leerArchivo(dataDir_ + "historial_reservas.txt");
}

std::vector<FileManager::Row> FileManager::leerHistorialClientes(const std::string& cliente) {
    std::shared_lock<std::shared_mutex> lk(mtxHistorialReservas_);
    auto todas = leerArchivo(dataDir_ + "historial_reservas.txt");
    std::vector<Row> resultado;
    // Formato de fila: timestamp,idCliente,idVuelo,idVendedor,estado
    for (auto& fila : todas) {
        if (fila.size() >= 2 && fila[1] == cliente) resultado.push_back(fila);
    }
    return resultado;
}

// ============================ VENDEDORES ===================================

bool FileManager::agregarVendedor(const std::string& usuario, const std::string& pass) {
    if (existeUsuario(usuario)) return false;
    anexarLinea(dataDir_ + "usuarios.txt", usuario + "," + pass + ",VENDEDOR,ACTIVO", mtxUsuarios_);
    anexarLinea(dataDir_ + "vendedores.txt", usuario + ",ACTIVO", mtxVendedores_);
    return true;
}

// --------------------------------------------------------------------------
// eliminarVendedor: BAJA LOGICA. Se marca INACTIVO en usuarios.txt (revoca
// acceso) y en vendedores.txt, pero JAMAS se toca historial_vendedores.txt:
// esa es la garantia de INMUTABILIDAD del historial de ventas exigida por
// el enunciado, incluso despues de que el vendedor ya no tiene cuenta.
// --------------------------------------------------------------------------
bool FileManager::eliminarVendedor(const std::string& usuario) {
    {
        std::unique_lock<std::shared_mutex> lk(mtxUsuarios_);
        auto filas = leerArchivo(dataDir_ + "usuarios.txt");
        bool encontrado = false;
        for (auto& fila : filas) {
            if (fila.size() >= 4 && fila[0] == usuario && fila[2] == "VENDEDOR") {
                fila[3] = "INACTIVO";
                encontrado = true;
            }
        }
        if (!encontrado) return false;
        escribirArchivoCompleto(dataDir_ + "usuarios.txt", filas);
    }
    {
        std::unique_lock<std::shared_mutex> lk(mtxVendedores_);
        auto filas = leerArchivo(dataDir_ + "vendedores.txt");
        for (auto& fila : filas) {
            if (!fila.empty() && fila[0] == usuario) fila[1] = "INACTIVO";
        }
        escribirArchivoCompleto(dataDir_ + "vendedores.txt", filas);
    }
    return true;
    // historial_vendedores.txt NUNCA se modifica aqui -> INMUTABLE
}

void FileManager::anexarHistorialVendedor(const Row& registro) {
    std::string linea;
    for (size_t i = 0; i < registro.size(); ++i) {
        linea += registro[i];
        if (i + 1 < registro.size()) linea += ",";
    }
    anexarLinea(dataDir_ + "historial_vendedores.txt", linea, mtxHistorialVendedores_);
}

std::vector<FileManager::Row> FileManager::leerHistorialVendedor(const std::string& vendedor) {
    std::shared_lock<std::shared_mutex> lk(mtxHistorialVendedores_);
    auto todas = leerArchivo(dataDir_ + "historial_vendedores.txt");
    std::vector<Row> resultado;
    // Formato de fila: timestamp,idVendedor,idCliente,idVuelo,comision,estado
    for (auto& fila : todas) {
        if (fila.size() >= 2 && fila[1] == vendedor) resultado.push_back(fila);
    }
    return resultado;
}

std::vector<FileManager::Row> FileManager::leerHistorialVendedoresGlobal() {
    std::shared_lock<std::shared_mutex> lk(mtxHistorialVendedores_);
    return leerArchivo(dataDir_ + "historial_vendedores.txt");
}

// ============================ COMENTARIOS ===================================

void FileManager::agregarComentario(const std::string& usuario, const std::string& texto) {
    // Se reemplazan comas por espacios para no romper el formato CSV simple
    std::string textoLimpio = texto;
    for (auto& c : textoLimpio) if (c == ',') c = ';';
    anexarLinea(dataDir_ + "comentarios.txt", usuario + "," + textoLimpio, mtxComentarios_);
}

std::vector<FileManager::Row> FileManager::leerComentarios() {
    std::shared_lock<std::shared_mutex> lk(mtxComentarios_);
    return leerArchivo(dataDir_ + "comentarios.txt");
}

// ============================ LOGS / REPORTES ================================

void FileManager::escribirLog(const std::string& linea) {
    anexarLinea(dataDir_ + "reportes.txt", timestampActual() + " | " + linea, mtxLogs_);
}
