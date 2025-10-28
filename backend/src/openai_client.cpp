#include "openai_client.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <unistd.h>

OpenAIClient::OpenAIClient(const std::string& key) 
    : apiKey(key), model("gpt-3.5-turbo") {
    
    // Validar API Key
    if (apiKey.empty() || apiKey.find("sk-") != 0) {
        throw std::runtime_error("API Key de OpenAI inválida o no configurada");
    }
}

std::string OpenAIClient::makeRequest(const json& requestBody) {
    // Guardar request en archivo temporal
    std::string requestFile = "/tmp/openai_request_" + std::to_string(getpid()) + ".json";
    std::ofstream file(requestFile);
    file << requestBody.dump();
    file.close();
    
    // Construir comando curl con timeout de 10 segundos
    std::string curlCmd = "curl --max-time 10 -s https://api.openai.com/v1/chat/completions "
        "-H 'Content-Type: application/json' "
        "-H 'Authorization: Bearer " + apiKey + "' "
        "-d @" + requestFile + " 2>&1";
    
    // Ejecutar curl y capturar respuesta
    FILE* pipe = popen(curlCmd.c_str(), "r");
    if (!pipe) {
        system(("rm -f " + requestFile).c_str());
        return "{\"error\": {\"message\": \"No se pudo conectar a OpenAI\"}}";
    }
    
    std::string response;
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        response += buffer;
    }
    int exitCode = pclose(pipe);
    
    // Limpiar archivo temporal
    system(("rm -f " + requestFile).c_str());
    
    // Verificar si hubo timeout o error
    if (exitCode != 0) {
        return "{\"error\": {\"message\": \"Timeout o error al conectar con OpenAI\"}}";
    }
    
    if (response.empty()) {
        return "{\"error\": {\"message\": \"Respuesta vacía de OpenAI\"}}";
    }
    
    return response;
}

std::string OpenAIClient::generateFeedback(const std::string& code,
                                          int testsPassed,
                                          int testsFailed,
                                          const std::string& compilationError,
                                          const std::string& runtimeError) {
    
    // Truncar código si es muy largo
    std::string truncatedCode = code;
    if (code.size() > 1000) {
        truncatedCode = code.substr(0, 1000) + "\n... (código truncado)";
    }
    
    // Construir prompt
    std::string prompt = 
        "Eres un coach de programación experto. Analiza este código C++:\n\n"
        "```cpp\n" + truncatedCode + "\n```\n\n"
        "Resultados:\n"
        "- Tests pasados: " + std::to_string(testsPassed) + "\n"
        "- Tests fallidos: " + std::to_string(testsFailed) + "\n";
    
    if (!compilationError.empty()) {
        std::string truncatedError = compilationError.substr(0, 
            std::min<size_t>(500, compilationError.size()));
        prompt += "- Error de compilación: " + truncatedError + "\n";
    }
    
    if (!runtimeError.empty()) {
        std::string truncatedError = runtimeError.substr(0, 
            std::min<size_t>(200, runtimeError.size()));
        prompt += "- Error en tiempo de ejecución: " + truncatedError + "\n";
    }
    
    prompt += "\nProporciona:\n"
        "1. Análisis breve del problema (1-2 líneas)\n"
        "2. Una pista específica para arreglarlo (sin dar la solución completa)\n"
        "3. Sugerencia de dónde debuggear\n\n"
        "Responde de forma concisa (máximo 4 líneas). Usa un tono motivador.";
    
    // Crear request JSON
    json requestBody = {
        {"model", model},
        {"messages", json::array({
            {
                {"role", "system"},
                {"content", "Eres un coach de programación paciente y motivador. "
                           "Das pistas útiles sin revelar la solución completa."}
            },
            {
                {"role", "user"},
                {"content", prompt}
            }
        })},
        {"temperature", 0.7},
        {"max_tokens", 200}
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
            std::string errorMsg = responseJson["error"]["message"];
            
            // Mensajes de error amigables
            if (errorMsg.find("rate limit") != std::string::npos) {
                return "El servicio de feedback está temporalmente ocupado. "
                       "Intenta nuevamente en unos segundos.";
            } else if (errorMsg.find("invalid") != std::string::npos || 
                      errorMsg.find("authentication") != std::string::npos) {
                return "Error de configuración del sistema. Contacta al administrador.";
            } else {
                return "No se pudo obtener feedback en este momento. Error: " + errorMsg;
            }
        }
    } catch (const std::exception& e) {
        return "Error al procesar respuesta del servicio de feedback. "
               "Por favor intenta nuevamente.";
    }

    return "No se pudo obtener feedback en este momento.";
}