#include "openai_client.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>

OpenAIClient::OpenAIClient(const std::string& key) 
    : apiKey(key), model("gpt-3.5-turbo") {
}

std::string OpenAIClient::makeRequest(const json& requestBody) {
    // Guardar request en archivo temporal
    std::string requestFile = "/tmp/openai_request.json";
    std::ofstream file(requestFile);
    file << requestBody.dump();
    file.close();
    
    // Construir comando curl
    std::string curlCmd = "curl -s https://api.openai.com/v1/chat/completions "
        "-H 'Content-Type: application/json' "
        "-H 'Authorization: Bearer " + apiKey + "' "
        "-d @" + requestFile;
    
    // Ejecutar curl y capturar respuesta
    FILE* pipe = popen(curlCmd.c_str(), "r");
    if (!pipe) {
        return "{\"error\": \"No se pudo conectar a OpenAI\"}";
    }
    
    std::string response;
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        response += buffer;
    }
    pclose(pipe);
    
    // Limpiar archivo temporal
    system(("rm -f " + requestFile).c_str());
    
    return response;
}

std::string OpenAIClient::generateFeedback(const std::string& code,
                                          int testsPassed,
                                          int testsFailed,
                                          const std::string& compilationError,
                                          const std::string& runtimeError) {
    
    // Construir prompt
    std::string prompt = 
        "Eres un coach de programación experto. Analiza este código C++:\n\n"
        "```cpp\n" + code + "\n```\n\n"
        "Resultados:\n"
        "- Tests pasados: " + std::to_string(testsPassed) + "\n"
        "- Tests fallidos: " + std::to_string(testsFailed) + "\n";
    
    if (!compilationError.empty()) {
        prompt += "- Error de compilación: " + compilationError + "\n";
    }
    
    if (!runtimeError.empty()) {
        prompt += "- Error en tiempo de ejecución: " + runtimeError + "\n";
    }
    
    prompt += "\nProporciona:\n"
        "1. Análisis breve del problema (1-2 líneas)\n"
        "2. Una pista específica para arreglarlo (sin dar la solución)\n"
        "3. Sugerencia de dónde debuggear\n\n"
        "Responde de forma concisa (máximo 3 líneas).";
    
    // Crear request JSON
    json requestBody = {
        {"model", model},
        {"messages", json::array({
            {
                {"role", "user"},
                {"content", prompt}
            }
        })},
        {"temperature", 0.7},
        {"max_tokens", 150}
    };
    
    // Hacer request a OpenAI
    std::string response = makeRequest(requestBody);
    
    try {
        auto responseJson = json::parse(response);
        
        // Extraer mensaje
        if (responseJson.contains("choices") && responseJson["choices"].size() > 0) {
            std::string message = responseJson["choices"][0]["message"]["content"];
            return message;
        } else if (responseJson.contains("error")) {
            std::string error = responseJson["error"]["message"];
            return "Error de OpenAI: " + error;
        }
    } catch (const std::exception& e) {
        return "Error al procesar respuesta de OpenAI";
    }
    
    return "No se pudo obtener feedback";
}