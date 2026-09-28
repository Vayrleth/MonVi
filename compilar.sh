#!/bin/bash
# Script de compilacion directo con g++ (alternativa a CMake).
# Debe ejecutarse desde la raiz del proyecto.
set -e

echo "Compilando aerolinea_server..."
g++ -std=c++17 -O2 -Iinclude -pthread -o aerolinea_server \
    src/main.cpp \
    src/RestServer.cpp \
    src/AuthManager.cpp \
    src/FileManager.cpp \
    src/Pipes.cpp \
    src/SharedMemory.cpp \
    src/Mailbox.cpp \
    src/Dekker.cpp \
    src/Peterson.cpp \
    src/Semaphore.cpp \
    src/Monitor.cpp \
    src/InterruptTable.cpp \
    src/JsonUtil.cpp \
    -lrt

echo "Compilando process_reports..."
g++ -std=c++17 -O2 -Iinclude -o process_reports src/Process_Reports.cpp

echo ""
echo "Compilacion exitosa."
echo "Ejecuta ./aerolinea_server desde esta misma carpeta para iniciar el servidor."
