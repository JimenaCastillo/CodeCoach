cmake_minimum_required(VERSION 3.10)
project(CodeCoach)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Flags de compilación
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# Incluir directorios
include_directories(include)
include_directories(third_party/cpp-httplib)

# Archivos fuente
set(SOURCES
    src/main.cpp
    src/rest_api.cpp
    src/motor_eval.cpp
    src/mongodb_manager.cpp
    src/openai_client.cpp
    src/solution_analyzer.cpp
)   

# Crear ejecutable
add_executable(codecoach ${SOURCES})

# Linkear librerías
target_link_libraries(codecoach curl pthread)

# Configuración de build
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Mensajes informativos
message(STATUS "Proyecto: ${PROJECT_NAME}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Output Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")