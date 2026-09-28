cmake_minimum_required(VERSION 3.10)
project(SistemaAerolinea CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Incluir directorio de encabezados
include_directories(${CMAKE_SOURCE_DIR}/include)

# ---------------------------------------------------------------------------
# Bibliotecas del sistema requeridas en Linux:
#   pthread -> std::thread, std::mutex, std::condition_variable, etc.
#   rt      -> mq_open/mq_send/mq_receive (colas POSIX) y shm_open (memoria compartida)
# ---------------------------------------------------------------------------
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Redireccionar salida de binarios a build/bin/
# ---------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
# Target 1: aerolinea_server (Servidor Backend / Proceso Principal)
# ---------------------------------------------------------------------------
add_executable(aerolinea_server
    src/main.cpp
    src/RestServer.cpp
    src/AuthManager.cpp
    src/FileManager.cpp
    src/Pipes.cpp
    src/SharedMemory.cpp
    src/Mailbox.cpp
    src/Dekker.cpp
    src/Peterson.cpp
    src/Semaphore.cpp
    src/Monitor.cpp
    src/InterruptTable.cpp
    src/JsonUtil.cpp
)

target_link_libraries(aerolinea_server PRIVATE Threads::Threads rt)

# ---------------------------------------------------------------------------
# Target 2: process_reports (Proceso Pesado Independiente de Reportes)
# ---------------------------------------------------------------------------
add_executable(process_reports
    src/Process_Reports.cpp
)

target_link_libraries(process_reports PRIVATE Threads::Threads rt)

# ---------------------------------------------------------------------------
# Copiar frontend (public/) y Enlazar Base de Datos (data/) en build/bin/
# ---------------------------------------------------------------------------
add_custom_target(preparar_entorno ALL
    # 1. Copiar los archivos web de la carpeta public/
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/public ${CMAKE_BINARY_DIR}/bin/public
    
    # 2. Crear enlace simbólico para data/ (evita duplicar o perder escrituras)
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_SOURCE_DIR}/data ${CMAKE_BINARY_DIR}/bin/data
    
    COMMENT "Sincronizando la interfaz web (public/) y creando enlace simbólico a la base de datos (data/)"
)
