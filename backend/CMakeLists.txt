cmake_minimum_required(VERSION 3.10)
project(CodeCoach)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencias
find_package(mongocxx REQUIRED)
find_package(bsoncxx REQUIRED)

# Incluir headers
include_directories(include)

# Archivos fuente
set(SOURCES
    src/main.cpp
    src/rest_api.cpp
    src/motor_eval.cpp
    src/mongodb_manager.cpp
    src/openai_client.cpp
)

# Crear ejecutable
add_executable(codecoach ${SOURCES})

# Linkear librerías
target_link_libraries(codecoach 
    mongocxx 
    bsoncxx
)
