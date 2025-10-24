#include "rest_api.h"
#include <iostream>

int main() {
    try {
        // Crear instancia del REST API en puerto 8080
        RestAPI api(8080);
        
        // Iniciar servidor
        api.start();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
}