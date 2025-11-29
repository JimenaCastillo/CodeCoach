#ifndef REST_API_H
#define REST_API_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Servidor REST API del backend
class RestAPI {
private:
    int port;               // Puerto del servidor (default: 8080)
    std::string baseURL;    // URL base de MongoDB
    std::string apiKey;     // API key de OpenAI   

public:
    RestAPI(int port); // Constructor del REST API
    void start(); // Inicia el servidor REST API
    
    // Métodos públicos para endpoints (pueden ser llamados externamente)
    json getProblems();
    json getProblem(const std::string& id);
    json createProblem(const json& problemData);
    
private:
    // Helpers
    std::string loadEnvVariable(const std::string& key); // Carga variable de entorno desde .env
};

#endif