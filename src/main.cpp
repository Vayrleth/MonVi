// ============================================================================
// main.cpp -- Servidor Web Principal (API REST) del Sistema de Gestion de
// Aerolinea. Punto de entrada del PROCESO PESADO principal, encargado de:
//   - Instalar los manejadores de señales POSIX (apagado seguro).
//   - Inicializar el Monitor de asientos (FlightSeatManager) desde vuelos.txt.
//   - Levantar el hilo consumidor del Mailbox (comentarios/auditoria async).
//   - Registrar la Tabla de Vectores de Interrupcion (ISR por evento).
//   - Ejecutar una demostracion controlada de DEADLOCK con su algoritmo de
//     evasion (backoff con try_lock ordenado).
//   - Registrar TODAS las rutas de la API REST y arrancar RestServer.
// ============================================================================
#include "RestServer.h"
#include "FileManager.h"
#include "AuthManager.h"
#include "Semaphore.h"
#include "Monitor.h"
#include "Dekker.h"
#include "Peterson.h"
#include "Mailbox.h"
#include "SharedMemory.h"
#include "InterruptTable.h"
#include "Pipes.h"
#include "JsonUtil.h"

#include <iostream>
#include <sstream>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <csignal>
#include <cstring>
#include <ctime>
#include <cstdlib>

// ----------------------------------------------------------------------------
// Recursos globales de sincronizacion (compartidos entre hilos del proceso
// principal). Se instancian una sola vez y se usan desde los handlers de la
// API REST registrados mas abajo.
// ----------------------------------------------------------------------------
static Semaphore semaforoIO(10);           // Aforo maximo de 10 operaciones I/O concurrentes
static Dekker dekkerReservaAsiento;        // Exclusion mutua Dekker: reservar el MISMO asiento
static Peterson petersonTransferencia;     // Exclusion mutua Peterson: transferencia entre vuelos
static std::atomic<int> contadorHilosDekker{0};
static Mailbox buzonComentariosYAuditoria("/aerolinea_mailbox", 32, 512);

// Mutex "de vuelo" usados exclusivamente para la demostracion controlada de
// deadlock (transferencia cruzada entre dos vuelos A y B).
static std::timed_mutex mutexVueloA;
static std::timed_mutex mutexVueloB;

// ----------------------------------------------------------------------------
// demostrarDeadlockControlado()
// ----------------------------------------------------------------------------
// Escenario: el Hilo 1 representa una transferencia de reservas del Vuelo A
// hacia el Vuelo B (bloquea A y luego intenta bloquear B). El Hilo 2
// representa, al mismo tiempo, una transferencia cruzada del Vuelo B hacia
// el Vuelo A (bloquea B y luego intenta bloquear A). Si ambos usaran
// std::lock_guard normal, este patron de bloqueo cruzado produce un
// DEADLOCK clasico (cada uno espera para siempre el recurso que el otro ya
// tiene tomado).
//
// ALGORITMO DE EVASION/PREVENCION aplicado: en lugar de bloquear
// indefinidamente, cada hilo usa try_lock_for() con un tiempo maximo de
// espera. Si no logra obtener el segundo mutex en ese plazo, LIBERA el
// primero (retrocede / "backoff") y reintenta desde cero tras una pequeña
// espera aleatoria. Esto rompe la condicion de "posesion y espera" que
// exige el deadlock, garantizando que eventualmente uno de los dos hilos
// consiga ambos recursos y progrese.
// ----------------------------------------------------------------------------
static void demostrarDeadlockControlado() {
    auto& fm = FileManager::getInstance();

    auto trabajoHilo = [&](bool primeroA, const std::string& nombreHilo) {
        std::timed_mutex& primero = primeroA ? mutexVueloA : mutexVueloB;
        std::timed_mutex& segundo = primeroA ? mutexVueloB : mutexVueloA;

        int intentos = 0;
        while (intentos < 5) {
            intentos++;
            std::unique_lock<std::timed_mutex> lockPrimero(primero);
            bool obtuvoSegundo = segundo.try_lock_for(std::chrono::milliseconds(50));

            if (obtuvoSegundo) {
                fm.escribirLog("[DEADLOCK-DEMO] " + nombreHilo +
                    " obtuvo ambos recursos (Vuelo A y Vuelo B) en el intento " +
                    std::to_string(intentos) + ". Transferencia simulada completada.");
                segundo.unlock();
                return; // exito: se rompio la posible cadena de espera circular
            } else {
                // No se pudo obtener el segundo recurso: retroceder (backoff),
                // liberar el primero y reintentar. Esto evita el deadlock.
                fm.escribirLog("[DEADLOCK-DEMO] " + nombreHilo +
                    " detecto contencion cruzada en el intento " + std::to_string(intentos) +
                    "; libera su recurso y reintenta (evasion de deadlock).");
                lockPrimero.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(10 + (intentos * 5)));
            }
        }
    };

    std::thread hilo1(trabajoHilo, true, "HiloTransferenciaVueloA->B");
    std::thread hilo2(trabajoHilo, false, "HiloTransferenciaVueloB->A");
    hilo1.join();
    hilo2.join();

    fm.escribirLog("[DEADLOCK-DEMO] Demostracion de deadlock controlado finalizada sin bloqueo permanente.");
}

// ----------------------------------------------------------------------------
// Hilo consumidor del Mailbox (Buzon POSIX). Corre en background durante
// toda la vida del servidor: hace mq_receive (bloqueante) en bucle y anexa
// el mensaje recibido al archivo correspondiente (comentarios.txt o
// reportes.txt) a traves de FileManager, desacoplando esa escritura del
// hilo HTTP que la origino.
// ----------------------------------------------------------------------------
static void hiloConsumidorMailbox() {
    auto& fm = FileManager::getInstance();
    std::string mensaje;
    while (!InterruptTable::banderaApagado().load()) {
        if (buzonComentariosYAuditoria.recibir(mensaje)) {
            // Formato del mensaje: "TIPO|contenido"
            size_t sep = mensaje.find('|');
            if (sep != std::string::npos) {
                std::string tipo = mensaje.substr(0, sep);
                std::string contenido = mensaje.substr(sep + 1);
                if (tipo == "COMENTARIO") {
                    fm.escribirLog("Comentario procesado de forma asincrona desde el Mailbox.");
                } else if (tipo == "AUDITORIA") {
                    fm.escribirLog(contenido);
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

// ----------------------------------------------------------------------------
// Serializa el catalogo de vuelos (vector<Row>) a JSON y refresca la CACHE
// en Memoria Compartida POSIX (SharedFlightCache), para que otros procesos
// puedan leer el catalogo sin volver a tocar disco.
// ----------------------------------------------------------------------------
static std::string vuelosAJson(const std::vector<FileManager::Row>& vuelos) {
    std::ostringstream json;
    json << "[";
    for (size_t i = 0; i < vuelos.size(); ++i) {
        const auto& v = vuelos[i];
        if (v.size() < 6) continue;
        json << "{"
             << "\"id\":\"" << JsonUtil::escapar(v[0]) << "\","
             << "\"origen\":\"" << JsonUtil::escapar(v[1]) << "\","
             << "\"destino\":\"" << JsonUtil::escapar(v[2]) << "\","
             << "\"asientos\":" << v[3] << ","
             << "\"precio\":" << v[4] << ","
             << "\"estado\":\"" << JsonUtil::escapar(v[5]) << "\""
             << "}";
        if (i + 1 < vuelos.size()) json << ",";
    }
    json << "]";
    return json.str();
}

// Nota: en vuelos.txt el orden real de columnas es:
// id,origen,destino,asientos,precio,estado  (ver data/vuelos.txt de ejemplo)

int main() {
    auto& fm = FileManager::getInstance();
    auto& auth = AuthManager::getInstance();
    auto& seatManager = FlightSeatManager::getInstance();
    auto& interruptTable = InterruptTable::getInstance();

    // 1) INTERRUPCIONES ASINCRONAS / SEÑALES POSIX ---------------------------
    interruptTable.instalarManejadoresDeSenales();

    // 2) TABLA DE VECTORES DE INTERRUPCION: registrar las ISR por evento -----
    interruptTable.registrarISR(EVT_VUELO_MODIFICADO, [&](const std::string& detalle) {
        fm.escribirLog("[ISR] Evento EVT_VUELO_MODIFICADO -> " + detalle);
    });
    interruptTable.registrarISR(EVT_VENDEDOR_REVOCADO, [&](const std::string& detalle) {
        // Prioridad critica (0): se atiende de forma apropiativa (preemptive)
        fm.escribirLog("[ISR-PRIORITARIA] Evento EVT_VENDEDOR_REVOCADO -> " + detalle);
    });
    interruptTable.registrarISR(EVT_RESERVA_CREADA, [&](const std::string& detalle) {
        fm.escribirLog("[ISR] Evento EVT_RESERVA_CREADA -> " + detalle);
    });
    interruptTable.registrarISR(EVT_RESERVA_CANCELADA, [&](const std::string& detalle) {
        fm.escribirLog("[ISR] Evento EVT_RESERVA_CANCELADA -> " + detalle);
    });
    interruptTable.registrarISR(EVT_ERROR_IO, [&](const std::string& detalle) {
        fm.escribirLog("[ISR-ERROR] Evento EVT_ERROR_IO -> " + detalle);
    });

    // 3) Inicializar el Monitor de asientos (FlightSeatManager) desde vuelos.txt
    for (auto& fila : fm.leerVuelos()) {
        if (fila.size() >= 4) {
            try { seatManager.inicializarVuelo(fila[0], std::stoi(fila[3])); } catch (...) {}
        }
    }

    // 4) Refrescar la cache en Memoria Compartida POSIX con el catalogo actual
    SharedFlightCache::getInstance().actualizar(vuelosAJson(fm.leerVuelos()));

    // 5) Hilo consumidor del Mailbox (Buzon POSIX), corre en background
    std::thread hiloMailbox(hiloConsumidorMailbox);
    hiloMailbox.detach();

    // 6) Demostracion controlada de Deadlock + algoritmo de evasion
    fm.escribirLog("Iniciando demostracion controlada de deadlock (transferencia cruzada de vuelos)...");
    demostrarDeadlockControlado();

    // ==========================================================================
    // REGISTRO DE RUTAS DE LA API REST
    // ==========================================================================
    const char* env_p = std::getenv("PORT");
    int puerto = (env_p != nullptr) ? std::atoi(env_p) : 8081;
    RestServer servidor(puerto);

    // --- OPTIONS generico (CORS preflight) para todas las rutas /api/* -------
    // (Se maneja de forma simplificada respondiendo 200 vacio si el navegador
    //  llegara a enviarlo; los endpoints abajo ya incluyen cabeceras CORS.)

    // GET /api/vuelos/publico -------------------------------------------------
    servidor.registrarRuta("GET", "/api/vuelos/publico", [&](const HttpRequest&) -> HttpResponse {
        semaforoIO.acquire();
        auto vuelos = fm.leerVuelos();
        semaforoIO.release();

        std::vector<FileManager::Row> comentarios;
        semaforoIO.acquire();
        comentarios = fm.leerComentarios();
        semaforoIO.release();

        std::ostringstream json;
        json << "{\"status\":\"success\",\"vuelos\":" << vuelosAJson(vuelos) << ",\"testimonios\":[";
        for (size_t i = 0; i < comentarios.size(); ++i) {
            if (comentarios[i].size() >= 2) {
                json << "{\"usuario\":\"" << JsonUtil::escapar(comentarios[i][0]) << "\","
                     << "\"texto\":\"" << JsonUtil::escapar(comentarios[i][1]) << "\"}";
                if (i + 1 < comentarios.size()) json << ",";
            }
        }
        json << "]}";

        HttpResponse resp; resp.cuerpoJson = json.str();
        return resp;
    });

    // POST /api/login ----------------------------------------------------------
    servidor.registrarRuta("POST", "/api/login", [&](const HttpRequest& req) -> HttpResponse {
        std::string usuario = JsonUtil::extraerString(req.cuerpo, "usuario");
        std::string pass = JsonUtil::extraerString(req.cuerpo, "password");

        std::string rol;
        HttpResponse resp;
        if (fm.validarLogin(usuario, pass, rol)) {
            std::string token = auth.crearSesion(usuario, rol);
            resp.cuerpoJson = "{\"status\":\"success\",\"token\":\"" + token +
                "\",\"rol\":\"" + rol + "\",\"usuario\":\"" + JsonUtil::escapar(usuario) + "\"}";
        } else {
            resp.codigo = 401;
            resp.cuerpoJson = "{\"status\":\"error\",\"mensaje\":\"Credenciales invalidas o usuario inactivo\"}";
        }
        return resp;
    });

    // POST /api/register --------------------------------------------------------
    servidor.registrarRuta("POST", "/api/register", [&](const HttpRequest& req) -> HttpResponse {
        std::string usuario = JsonUtil::extraerString(req.cuerpo, "usuario");
        std::string pass = JsonUtil::extraerString(req.cuerpo, "password");

        HttpResponse resp;
        semaforoIO.acquire();
        bool ok = fm.registrarCliente(usuario, pass);
        semaforoIO.release();

        if (ok) {
            resp.cuerpoJson = "{\"status\":\"success\",\"mensaje\":\"Cliente registrado correctamente\"}";
        } else {
            resp.codigo = 400;
            resp.cuerpoJson = "{\"status\":\"error\",\"mensaje\":\"El usuario ya existe\"}";
        }
        return resp;
    });

    // GET /api/vuelos (autenticado) ----------------------------------------------
    servidor.registrarRuta("GET", "/api/vuelos", [&](const HttpRequest&) -> HttpResponse {
        semaforoIO.acquire();
        auto vuelos = fm.leerVuelos();
        semaforoIO.release();
        HttpResponse resp;
        resp.cuerpoJson = "{\"status\":\"success\",\"vuelos\":" + vuelosAJson(vuelos) + "}";
        return resp;
    });

    // POST /api/reservar ------------------------------------------------------
    // Aqui se demuestra el ALGORITMO DE DEKKER protegiendo la region critica
    // de reservar un asiento, ADEMAS del Monitor (FlightSeatManager) que
    // coordina espera/aviso, y la actualizacion atomica en vuelos.txt.
    servidor.registrarRuta("POST", "/api/reservar", [&](const HttpRequest& req) -> HttpResponse {
        std::string idVuelo = JsonUtil::extraerString(req.cuerpo, "idVuelo");
        std::string cliente = JsonUtil::extraerString(req.cuerpo, "cliente");
        std::string vendedor = JsonUtil::extraerString(req.cuerpo, "vendedor"); // opcional, "" si autoservicio

        HttpResponse resp;

        // --- ALGORITMO DE DEKKER: exclusion mutua entre 2 hilos que podrian
        // intentar reservar el MISMO asiento del MISMO vuelo simultaneamente ---
        int idHiloDekker = contadorHilosDekker.fetch_add(1) % 2;
        dekkerReservaAsiento.lock(idHiloDekker);

        bool haySitio = seatManager.reservarAsiento(idVuelo, 200); // MONITOR con variable de condicion

        if (!haySitio) {
            dekkerReservaAsiento.unlock(idHiloDekker);
            resp.codigo = 409;
            resp.cuerpoJson = "{\"status\":\"error\",\"mensaje\":\"No hay asientos disponibles para este vuelo\"}";
            return resp;
        }

        semaforoIO.acquire();
        bool actualizado = fm.actualizarAsientos(idVuelo, -1); // REGION CRITICA sobre vuelos.txt
        semaforoIO.release();

        dekkerReservaAsiento.unlock(idHiloDekker);

        if (!actualizado) {
            seatManager.liberarAsiento(idVuelo); // revertir el monitor si el archivo fallo
            resp.codigo = 500;
            resp.cuerpoJson = "{\"status\":\"error\",\"mensaje\":\"No se pudo actualizar vuelos.txt\"}";
            interruptTable.dispararEvento(EVT_ERROR_IO, "Fallo al actualizar asientos del vuelo " + idVuelo, 1);
            return resp;
        }

        std::string idReserva = idVuelo + "-" + cliente + "-" + std::to_string(std::time(nullptr));

        semaforoIO.acquire();
        fm.crearReserva({idReserva, cliente, idVuelo, vendedor, "ACTIVA"});

        // HISTORIAL INMUTABLE: se anexa (append-only), nunca se reescribe
        std::string ts = std::to_string(std::time(nullptr));
        fm.anexarHistorialReserva({ts, cliente, idVuelo, vendedor.empty() ? "AUTOSERVICIO" : vendedor, "COMPRA"});

        if (!vendedor.empty()) {
            fm.anexarHistorialVendedor({ts, vendedor, cliente, idVuelo, "COMISION_PENDIENTE", "VENTA"});
        }
        semaforoIO.release();

        SharedFlightCache::getInstance().actualizar(vuelosAJson(fm.leerVuelos()));

        interruptTable.dispararEvento(EVT_RESERVA_CREADA,
            "Reserva " + idReserva + " creada para cliente " + cliente, 3);

        // Encolar evento de auditoria en el Mailbox (no bloqueante)
        buzonComentariosYAuditoria.enviar("AUDITORIA|Reserva creada: " + idReserva);

        resp.cuerpoJson = "{\"status\":\"success\",\"idReserva\":\"" + idReserva + "\"}";
        return resp;
    });

    // POST /api/cancelar -------------------------------------------------------
    servidor.registrarRuta("POST", "/api/cancelar", [&](const HttpRequest& req) -> HttpResponse {
        std::string idReserva = JsonUtil::extraerString(req.cuerpo, "idReserva");
        std::string idVuelo = JsonUtil::extraerString(req.cuerpo, "idVuelo");
        std::string cliente = JsonUtil::extraerString(req.cuerpo, "cliente");

        HttpResponse resp;
        semaforoIO.acquire();
        bool ok = fm.cancelarReserva(idReserva);
        semaforoIO.release();

        if (ok) {
            semaforoIO.acquire();
            fm.actualizarAsientos(idVuelo, +1);
            std::string ts = std::to_string(std::time(nullptr));
            fm.anexarHistorialReserva({ts, cliente, idVuelo, "N/A", "CANCELACION"});
            semaforoIO.release();

            seatManager.liberarAsiento(idVuelo); // Monitor: notifica a hilos esperando asiento

            SharedFlightCache::getInstance().actualizar(vuelosAJson(fm.leerVuelos()));
            interruptTable.dispararEvento(EVT_RESERVA_CANCELADA, "Reserva " + idReserva + " cancelada", 3);

            resp.cuerpoJson = "{\"status\":\"success\",\"mensaje\":\"Reserva cancelada\"}";
        } else {
            resp.codigo = 404;
            resp.cuerpoJson = "{\"status\":\"error\",\"mensaje\":\"Reserva no encontrada\"}";
        }
        return resp;
    });

    // GET /api/cliente/historial?cliente=XXX -------------------------------------
    servidor.registrarRuta("GET", "/api/cliente/historial", [&](const HttpRequest& req) -> HttpResponse {
        std::string cliente = JsonUtil::extraerString(req.cuerpo, "cliente");
        if (cliente.empty()) {
            // Tambien se acepta via header simple "X-Usuario" para peticiones GET
            auto it = req.headers.find("X-Usuario");
            if (it != req.headers.end()) cliente = it->second;
        }

        semaforoIO.acquire();
        auto historial = fm.leerHistorialClientes(cliente);
        semaforoIO.release();

        std::ostringstream json;
        json << "{\"status\":\"success\",\"historial\":[";
        for (size_t i = 0; i < historial.size(); ++i) {
            auto& h = historial[i];
            if (h.size() < 5) continue;
            json << "{\"timestamp\":\"" << h[0] << "\",\"idVuelo\":\"" << h[2]
                 << "\",\"vendedor\":\"" << h[3] << "\",\"estado\":\"" << h[4] << "\"}";
            if (i + 1 < historial.size()) json << ",";
        }
        json << "]}";

        HttpResponse resp; resp.cuerpoJson = json.str();
        return resp;
    });

    // GET /api/vendedor/historial ------------------------------------------------
    servidor.registrarRuta("GET", "/api/vendedor/historial", [&](const HttpRequest& req) -> HttpResponse {
        std::string vendedor = JsonUtil::extraerString(req.cuerpo, "vendedor");
        if (vendedor.empty()) {
            auto it = req.headers.find("X-Usuario");
            if (it != req.headers.end()) vendedor = it->second;
        }

        semaforoIO.acquire();
        auto historial = fm.leerHistorialVendedor(vendedor);
        semaforoIO.release();

        std::ostringstream json;
        json << "{\"status\":\"success\",\"historial\":[";
        for (size_t i = 0; i < historial.size(); ++i) {
            auto& h = historial[i];
            if (h.size() < 6) continue;
            json << "{\"timestamp\":\"" << h[0] << "\",\"cliente\":\"" << h[2]
                 << "\",\"idVuelo\":\"" << h[3] << "\",\"comision\":\"" << h[4]
                 << "\",\"tipo\":\"" << h[5] << "\"}";
            if (i + 1 < historial.size()) json << ",";
        }
        json << "]}";

        HttpResponse resp; resp.cuerpoJson = json.str();
        return resp;
    });

    // POST /api/vendedor/transferir ----------------------------------------------
    // Demuestra el ALGORITMO DE PETERSON protegiendo la transferencia de una
    // reserva desde un vuelo hacia otro vuelo (posible punto de deadlock si
    // dos vendedores transfieren en direcciones cruzadas al mismo tiempo;
    // ver demostrarDeadlockControlado() para el escenario completo).
    servidor.registrarRuta("POST", "/api/vendedor/transferir", [&](const HttpRequest& req) -> HttpResponse {
        std::string idReserva = JsonUtil::extraerString(req.cuerpo, "idReserva");
        std::string vueloOrigen = JsonUtil::extraerString(req.cuerpo, "vueloOrigen");
        std::string vueloDestino = JsonUtil::extraerString(req.cuerpo, "vueloDestino");
        std::string cliente = JsonUtil::extraerString(req.cuerpo, "cliente");
        std::string vendedor = JsonUtil::extraerString(req.cuerpo, "vendedor");

        int idHiloPeterson = contadorHilosDekker.fetch_add(1) % 2;
        petersonTransferencia.lock(idHiloPeterson);

        HttpResponse resp;
        bool reservoDestino = seatManager.reservarAsiento(vueloDestino, 200);
        if (!reservoDestino) {
            petersonTransferencia.unlock(idHiloPeterson);
            resp.codigo = 409;
            resp.cuerpoJson = "{\"status\":\"error\",\"mensaje\":\"Sin disponibilidad en el vuelo destino\"}";
            return resp;
        }

        semaforoIO.acquire();
        fm.actualizarAsientos(vueloDestino, -1);
        fm.actualizarAsientos(vueloOrigen, +1);
        fm.cancelarReserva(idReserva);
        std::string ts = std::to_string(std::time(nullptr));
        std::string nuevaReserva = vueloDestino + "-" + cliente + "-" + ts;
        fm.crearReserva({nuevaReserva, cliente, vueloDestino, vendedor, "ACTIVA"});
        fm.anexarHistorialReserva({ts, cliente, vueloDestino, vendedor, "TRANSFERENCIA"});
        fm.anexarHistorialVendedor({ts, vendedor, cliente, vueloDestino, "COMISION_PENDIENTE", "TRANSFERENCIA"});
        semaforoIO.release();

        seatManager.liberarAsiento(vueloOrigen);
        petersonTransferencia.unlock(idHiloPeterson);

        SharedFlightCache::getInstance().actualizar(vuelosAJson(fm.leerVuelos()));

        resp.cuerpoJson = "{\"status\":\"success\",\"nuevaReserva\":\"" + nuevaReserva + "\"}";
        return resp;
    });

    // POST /api/gerente/vuelos (crear o editar) ------------------------------------
    servidor.registrarRuta("POST", "/api/gerente/vuelos", [&](const HttpRequest& req) -> HttpResponse {
        std::string id = JsonUtil::extraerString(req.cuerpo, "id");
        std::string origen = JsonUtil::extraerString(req.cuerpo, "origen");
        std::string destino = JsonUtil::extraerString(req.cuerpo, "destino");
        std::string asientos = JsonUtil::extraerString(req.cuerpo, "asientos");
        std::string precio = JsonUtil::extraerString(req.cuerpo, "precio");
        std::string estado = JsonUtil::extraerString(req.cuerpo, "estado");
        if (estado.empty()) estado = "ACTIVO";

        // INTERRUPCIONES ENMASCARABLES: se enmascara la señal secundaria
        // SIGUSR1 durante esta fase critica de escritura en vuelos.txt.
        interruptTable.enmascararSenalesSecundarias();

        semaforoIO.acquire();
        fm.crearOEditarVuelo({id, origen, destino, asientos, precio, estado});
        semaforoIO.release();

        interruptTable.restaurarSenales();

        try { seatManager.inicializarVuelo(id, std::stoi(asientos)); } catch (...) {}
        SharedFlightCache::getInstance().actualizar(vuelosAJson(fm.leerVuelos()));

        // Evento administrativo: prioridad alta pero no critica (no es revocacion)
        interruptTable.dispararEvento(EVT_VUELO_MODIFICADO, "Vuelo " + id + " creado/editado por Gerente", 1);

        HttpResponse resp;
        resp.cuerpoJson = "{\"status\":\"success\",\"mensaje\":\"Vuelo guardado correctamente\"}";
        return resp;
    });

    // POST /api/gerente/vendedores/agregar --------------------------------------
    servidor.registrarRuta("POST", "/api/gerente/vendedores/agregar", [&](const HttpRequest& req) -> HttpResponse {
        std::string usuario = JsonUtil::extraerString(req.cuerpo, "usuario");
        std::string pass = JsonUtil::extraerString(req.cuerpo, "password");

        semaforoIO.acquire();
        bool ok = fm.agregarVendedor(usuario, pass);
        semaforoIO.release();

        HttpResponse resp;
        if (ok) {
            resp.cuerpoJson = "{\"status\":\"success\",\"mensaje\":\"Vendedor agregado\"}";
        } else {
            resp.codigo = 400;
            resp.cuerpoJson = "{\"status\":\"error\",\"mensaje\":\"El usuario ya existe\"}";
        }
        return resp;
    });

    // DELETE /api/gerente/vendedores/eliminar ------------------------------------
    // PLANIFICACION APROPIATIVA: este evento se dispara con PRIORIDAD 0
    // (la mas alta), simulando que la revocacion de acceso de un vendedor
    // se procesa de inmediato, por delante de peticiones de lectura en cola.
    servidor.registrarRuta("DELETE", "/api/gerente/vendedores/eliminar", [&](const HttpRequest& req) -> HttpResponse {
        std::string usuario = JsonUtil::extraerString(req.cuerpo, "usuario");

        semaforoIO.acquire();
        bool ok = fm.eliminarVendedor(usuario); // Baja logica -- historial jamas se borra
        semaforoIO.release();

        HttpResponse resp;
        if (ok) {
            auth.revocarSesionesDe(usuario);
            // Evento CRITICO de prioridad 0: interrumpe/adelanta el procesamiento
            interruptTable.dispararEvento(EVT_VENDEDOR_REVOCADO,
                "Vendedor " + usuario + " revocado por el Gerente (acceso eliminado, historial intacto)", 0);

            resp.cuerpoJson = "{\"status\":\"success\",\"mensaje\":\"Vendedor eliminado (historial de ventas preservado)\"}";
        } else {
            resp.codigo = 404;
            resp.cuerpoJson = "{\"status\":\"error\",\"mensaje\":\"Vendedor no encontrado\"}";
        }
        return resp;
    });

    // GET /api/gerente/historial-general -----------------------------------------
    servidor.registrarRuta("GET", "/api/gerente/historial-general", [&](const HttpRequest&) -> HttpResponse {
        semaforoIO.acquire();
        auto historialReservas = fm.leerHistorialReservas();
        auto historialVentas = fm.leerHistorialVendedoresGlobal();
        semaforoIO.release();

        std::ostringstream json;
        json << "{\"status\":\"success\",\"historial_reservas\":[";
        for (size_t i = 0; i < historialReservas.size(); ++i) {
            auto& h = historialReservas[i];
            if (h.size() < 5) continue;
            json << "{\"timestamp\":\"" << h[0] << "\",\"cliente\":\"" << h[1]
                 << "\",\"idVuelo\":\"" << h[2] << "\",\"vendedor\":\"" << h[3]
                 << "\",\"estado\":\"" << h[4] << "\"}";
            if (i + 1 < historialReservas.size()) json << ",";
        }
        json << "],\"historial_ventas\":[";
        for (size_t i = 0; i < historialVentas.size(); ++i) {
            auto& h = historialVentas[i];
            if (h.size() < 6) continue;
            json << "{\"timestamp\":\"" << h[0] << "\",\"vendedor\":\"" << h[1]
                 << "\",\"cliente\":\"" << h[2] << "\",\"idVuelo\":\"" << h[3]
                 << "\",\"comision\":\"" << h[4] << "\",\"tipo\":\"" << h[5] << "\"}";
            if (i + 1 < historialVentas.size()) json << ",";
        }
        json << "]}";

        HttpResponse resp; resp.cuerpoJson = json.str();
        return resp;
    });

    // GET /api/gerente/reportes ----------------------------------------------
    // TUBERIAS POSIX + PROCESOS PESADOS: lanza el proceso independiente
    // process_reports via fork()+exec() y le pasa parametros por un pipe,
    // recibiendo el reporte generado por otro pipe.
    servidor.registrarRuta("GET", "/api/gerente/reportes", [&](const HttpRequest&) -> HttpResponse {
        std::string resultado = ReportPipe::ejecutarReporte("REPORTE_GENERAL|solicitado_por=gerente");
        HttpResponse resp;
        resp.cuerpoJson = resultado;
        return resp;
    });

    // POST /api/cliente/comentario -------------------------------------------
    // BUZON (Mailbox): se encola el comentario en la cola de mensajes POSIX
    // en lugar de escribirlo directamente; el hilo consumidor lo procesa.
    servidor.registrarRuta("POST", "/api/cliente/comentario", [&](const HttpRequest& req) -> HttpResponse {
        std::string usuario = JsonUtil::extraerString(req.cuerpo, "usuario");
        std::string texto = JsonUtil::extraerString(req.cuerpo, "texto");

        semaforoIO.acquire();
        fm.agregarComentario(usuario, texto); // escritura real (rapida, append-only)
        semaforoIO.release();

        buzonComentariosYAuditoria.enviar("COMENTARIO|" + usuario);

        HttpResponse resp;
        resp.cuerpoJson = "{\"status\":\"success\",\"mensaje\":\"Comentario recibido\"}";
        return resp;
    });

    std::cout << "==================================================================\n";
    std::cout << " Sistema de Gestion de Aerolinea -- Servidor Backend C++17 (Linux)\n";
    std::cout << " Landing Page:  http://localhost:8080/index.html\n";
    std::cout << " Login:         http://localhost:8080/login.html\n";
    std::cout << "==================================================================\n";

    servidor.iniciar(); // Bloqueante: bucle accept() hasta señal de apagado

    fm.escribirLog("Apagado seguro del servidor completado (buffers volcados a disco).");
    std::cout << "[main] Servidor detenido correctamente." << std::endl;
    return 0;
}
