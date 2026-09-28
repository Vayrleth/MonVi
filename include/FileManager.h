#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <string>
#include <vector>
#include <shared_mutex>
#include <mutex>
#include <map>

// ============================================================================
// FileManager
// ----------------------------------------------------------------------------
// Encapsula TODO el acceso a los archivos .txt que actuan como base de datos
// del sistema. Cada archivo tiene asociado un std::shared_mutex propio, de
// forma que las lecturas (varios hilos) puedan ocurrir en paralelo, pero toda
// escritura se realice en EXCLUSION MUTUA total (REGION CRITICA).
//
// Concepto de SO aplicado: "Regiones Criticas" + "Primitivas de Exclusion
// Mutua" -> ver los bloques std::unique_lock / std::shared_lock en el .cpp.
//
// Los historiales (historial_reservas.txt y historial_vendedores.txt) son
// INMUTABLES: solo se permite anexar (append) registros nuevos, jamas se
// reescriben ni se borran lineas existentes, incluso cuando un vendedor es
// dado de baja logica.
// ============================================================================
class FileManager {
public:
    static FileManager& getInstance();

    // Registro genérico de una fila de datos (vector de campos separados por coma)
    using Row = std::vector<std::string>;

    // --- USUARIOS ---
    bool validarLogin(const std::string& usuario, const std::string& pass, std::string& rolOut);
    bool registrarCliente(const std::string& usuario, const std::string& pass);
    bool existeUsuario(const std::string& usuario);

    // --- VUELOS ---
    std::vector<Row> leerVuelos();
    bool actualizarAsientos(const std::string& idVuelo, int delta); // REGION CRITICA
    bool crearOEditarVuelo(const Row& vuelo);

    // --- RESERVACIONES / HISTORIAL (INMUTABLE) ---
    bool crearReserva(const Row& reserva);
    bool cancelarReserva(const std::string& idReserva);
    std::vector<Row> leerReservaciones();
    void anexarHistorialReserva(const Row& registro);      // APPEND-ONLY
    std::vector<Row> leerHistorialReservas();
    std::vector<Row> leerHistorialClientes(const std::string& cliente);

    // --- VENDEDORES / HISTORIAL VENDEDOR (INMUTABLE) ---
    bool agregarVendedor(const std::string& usuario, const std::string& pass);
    bool eliminarVendedor(const std::string& usuario); // baja logica: NO borra historial
    void anexarHistorialVendedor(const Row& registro);
    std::vector<Row> leerHistorialVendedor(const std::string& vendedor);
    std::vector<Row> leerHistorialVendedoresGlobal();

    // --- COMENTARIOS ---
    void agregarComentario(const std::string& usuario, const std::string& texto);
    std::vector<Row> leerComentarios();

    // --- LOGS / REPORTES ---
    void escribirLog(const std::string& linea);

private:
    FileManager();
    FileManager(const FileManager&) = delete;
    FileManager& operator=(const FileManager&) = delete;

    std::vector<Row> leerArchivo(const std::string& ruta);
    void escribirArchivoCompleto(const std::string& ruta, const std::vector<Row>& filas);
    void anexarLinea(const std::string& ruta, const std::string& linea, std::shared_mutex& mtx);

    std::string dataDir_;

    // Un mutex por archivo -> permite paralelismo entre archivos distintos
    // y consistencia (region critica) dentro del mismo archivo.
    std::shared_mutex mtxUsuarios_;
    std::shared_mutex mtxVuelos_;
    std::shared_mutex mtxReservaciones_;
    std::shared_mutex mtxHistorialReservas_;
    std::shared_mutex mtxVendedores_;
    std::shared_mutex mtxHistorialVendedores_;
    std::shared_mutex mtxComentarios_;
    std::shared_mutex mtxLogs_;
};

#endif // FILEMANAGER_H
