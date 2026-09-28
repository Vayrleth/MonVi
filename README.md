# SkyLinea — Sistema de Gestion de Aerolinea

Backend en **C++17** (Linux/POSIX) + Frontend web (HTML/CSS/JS) que usa
archivos `.txt` como unica base de datos. Proyecto academico centrado en
demostrar 23 conceptos de Sistemas Operativos de forma explicita y
funcional dentro de una aplicacion real.

## Requisitos

- Linux (probado sobre Ubuntu 24.04 / g++ 13)
- g++ con soporte de C++17
- `make` (opcional, para el Makefile de conveniencia) o `cmake` >= 3.10

> Nota: en el entorno donde se genero este proyecto no habia `cmake`
> instalado ni acceso a internet, por lo que la compilacion se validó con
> `g++` directamente (ver `compilar.sh`). El `CMakeLists.txt` incluido es
> completamente funcional si `cmake` esta disponible en tu maquina.

## Compilar

### Opcion A: script directo con g++ (siempre funciona)

```bash
chmod +x compilar.sh
./compilar.sh
```

Esto genera dos binarios en la raiz del proyecto:
- `aerolinea_server`  -> Servidor Backend / Proceso Principal (API REST)
- `process_reports`   -> Proceso Pesado Independiente de Reportes

### Opcion B: CMake

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

Los binarios se colocan automaticamente en la raiz del proyecto (junto a
`data/` y `public/`) porque `Pipes.cpp` invoca `./process_reports` con ruta
relativa desde el directorio de ejecucion.

## Ejecutar

Desde la **raiz del proyecto** (donde estan `data/`, `public/`,
`aerolinea_server` y `process_reports`):

```bash
./aerolinea_server
```

El servidor queda escuchando en `http://localhost:8080`. Abre en tu
navegador:

- Landing page: `http://localhost:8080/index.html`
- Login/registro: `http://localhost:8080/login.html`

Para apagar el servidor de forma segura: `Ctrl+C` (SIGINT) o `kill <pid>`
(SIGTERM). Ambas señales estan capturadas para volcar buffers pendientes a
los `.txt` antes de terminar.

## Usuarios de prueba (data/usuarios.txt)

| Usuario   | Contraseña | Rol       |
|-----------|------------|-----------|
| gerente1  | admin123   | GERENTE   |
| vend1     | vend123    | VENDEDOR  |
| cliente1  | cli123     | CLIENTE   |

Tambien puedes registrar nuevos clientes desde `login.html`.

## Estructura del proyecto

```
aerolinea_project/
├── CMakeLists.txt
├── compilar.sh              # Script alterno de compilacion con g++
├── README.md
├── data/                    # Base de datos en archivos .txt
├── public/                  # Frontend (HTML, CSS, JS) servido por el propio backend
├── include/                 # Cabeceras C++ (.h con guardas #ifndef)
└── src/                     # Implementaciones C++ (.cpp)
```

## Mapa de conceptos de Sistemas Operativos -> Codigo

| # | Concepto | Donde se implementa |
|---|----------|----------------------|
| 1 | Multiprogramacion | `src/main.cpp` (multiples subsistemas corriendo a la vez: servidor HTTP, hilo de mailbox, monitor de asientos) |
| 2 | Concurrencia | `src/RestServer.cpp` (un `std::thread` por conexion HTTP) |
| 3 | Procesos pesados | `src/Pipes.cpp` (`fork()` + `execlp()` hacia `process_reports`) |
| 4 | Procesos ligeros / hilos | `src/RestServer.cpp`, `std::thread` en `main.cpp` |
| 5 | Planificacion apropiativa | `DELETE /api/gerente/vendedores/eliminar` dispara evento de **prioridad 0** en `InterruptTable` |
| 6 | Prioridad de interrupciones | `include/InterruptTable.h` (`EventoPrioritario`, cola de prioridad) |
| 7 | Interrupciones sincronas (excepciones) | `try/catch` en `RestServer::atenderCliente` |
| 8 | Señales POSIX (asincronas) | `src/InterruptTable.cpp` (`SIGINT`, `SIGTERM`) |
| 9 | Enmascarables / no enmascarables | `enmascararSenalesSecundarias()` / `restaurarSenales()` (SIGUSR1) en `POST /api/gerente/vuelos` |
| 10 | Vectores de interrupcion | `InterruptTable::tablaISR_` (mapa codigo->ISR) |
| 11 | Regiones criticas | `FileManager.cpp` (bloques `unique_lock`/`shared_lock` sobre cada `.txt`) |
| 12 | Primitivas de exclusion mutua | `std::mutex`, `std::shared_mutex` en todo `FileManager`, `AuthManager` |
| 13 | Algoritmo de Dekker | `include/Dekker.h`, `src/Dekker.cpp`, usado en `POST /api/reservar` |
| 14 | Algoritmo de Peterson | `include/Peterson.h`, `src/Peterson.cpp`, usado en `POST /api/vendedor/transferir` |
| 15 | Exclusion mutua entre procesos | `src/Pipes.cpp` (padre e hijo sin memoria compartida, solo pipes) |
| 16 | Deadlocks (demo + evasion) | `demostrarDeadlockControlado()` en `src/main.cpp` |
| 17 | Semaforos | `include/Semaphore.h` (`sem_t`), limita aforo de I/O a 10 en `main.cpp` |
| 18 | Buzones / colas de mensajes POSIX | `include/Mailbox.h` (`mqueue.h`), usado para comentarios/auditoria |
| 19 | Monitores | `include/Monitor.h` (`FlightSeatManager`, `std::condition_variable`) |
| 20 | Tuberias POSIX | `src/Pipes.cpp` (`pipe()`) |
| 21 | Mensajes | Protocolo de texto `ACCION\|param1\|param2` entre procesos (`Pipes.cpp` / `Process_Reports.cpp`) |
| 22 | Sockets | `src/RestServer.cpp` (`socket()`, `bind()`, `listen()`, `accept()`) |
| 23 | Memoria compartida POSIX | `include/SharedMemory.h` (`shm_open` + `mmap`) |

## Endpoints de la API REST

| Metodo | Ruta | Descripcion |
|--------|------|-------------|
| GET    | `/api/vuelos/publico` | Catalogo publico + testimonios (Landing Page) |
| POST   | `/api/login` | Autenticacion (Cliente/Vendedor/Gerente) |
| POST   | `/api/register` | Registro publico de clientes |
| GET    | `/api/vuelos` | Catalogo de vuelos (autenticado) |
| POST   | `/api/reservar` | Reservar/comprar un asiento |
| POST   | `/api/cancelar` | Cancelar una reserva |
| GET    | `/api/cliente/historial` | Historial personal del cliente (header `X-Usuario`) |
| GET    | `/api/vendedor/historial` | Historial de ventas del vendedor (header `X-Usuario`) |
| POST   | `/api/vendedor/transferir` | Transferir una reserva entre vuelos |
| POST   | `/api/gerente/vuelos` | Crear o editar un vuelo |
| POST   | `/api/gerente/vendedores/agregar` | Alta de vendedor |
| DELETE | `/api/gerente/vendedores/eliminar` | Baja logica de vendedor (historial intacto) |
| GET    | `/api/gerente/historial-general` | Historial global de auditoria |
| GET    | `/api/gerente/reportes` | Reporte generado por proceso independiente (pipes) |
| POST   | `/api/cliente/comentario` | Enviar comentario (via Mailbox/cola de mensajes) |

## Notas de diseño y simplificaciones conscientes

- **Sin dependencias externas**: el servidor HTTP esta escrito directamente
  sobre sockets POSIX (sin `cpp-httplib` ni `boost`), y el parseo JSON de
  entrada usa un extractor minimo (`JsonUtil`) en vez de una libreria como
  `nlohmann::json`, para que el proyecto compile con un simple `g++` sin
  pasos adicionales de instalacion.
- **Imagen de fondo**: `public/img/aeropuerto.jpg` fue generada
  programaticamente (silueta de aeropuerto/avion al atardecer) porque el
  entorno de generacion de este proyecto no tenia acceso a internet para
  descargar una fotografia. Puedes sustituirla libremente por una foto real
  de aeropuerto/avion; el CSS ya aplica el degradado oscuro de superposicion
  sobre cualquier imagen que coloques en esa ruta.
- **Reservas activas del cliente**: el cliente ve su historial permanente
  desde el servidor (`historial_reservas.txt`), y ademas el frontend guarda
  localmente (en `localStorage`) el ID de las reservas creadas en la sesion
  actual para poder ofrecer un boton de cancelacion inmediato, ya que el
  backend no expone un endpoint separado de "reservas activas por cliente"
  fuera del alcance minimo solicitado.
- Las tablas de texto (`,` como separador) no soportan comas dentro de los
  valores; los comentarios de clientes reemplazan comas por punto y coma
  automaticamente antes de guardarse.
