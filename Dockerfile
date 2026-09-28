# 1. Imagen base con entorno Linux e instaladores de C++ y CMake
FROM ubuntu:22.04

# 2. Evitar preguntas interactivas durante la instalación
ENV DEBIAN_FRONTEND=noninteractive

# 3. Instalar compiladores C++, CMake y utilidades de compilación
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++ \
    && rm -rf /var/lib/apt/lists/*

# 4. Crear carpeta de trabajo en el servidor remoto
WORKDIR /app

# 5. Copiar todo tu código fuente dentro del contenedor
COPY . .

# 6. Crear la carpeta de compilación y compilar el proyecto
RUN mkdir -p build && cd build && cmake .. && make

# 7. Exponer el puerto donde escucha tu servidor C++
EXPOSE 8080

# 8. Comando para iniciar el ejecutable
CMD ["./build/bin/aerolinea_server"]
