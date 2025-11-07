#include "openai_client.h"
#include <fstream>
#include <cstdlib>
#include <unistd.h>

OpenAIClient::OpenAIClient(const std::string& key) 
    : apiKey(key), model("gpt-3.5-turbo") {
    
    // Validar API Key
    if (apiKey.empty() || apiKey.find("sk-") != 0) {
        throw std::runtime_error("API Key de OpenAI inválida o no configurada");
    }
    
    // Validar que no sea un placeholder
    if (apiKey.find("xxxxx") != std::string::npos || apiKey.length() < 20) {
        throw std::runtime_error("API Key de OpenAI no configurada correctamente. Verifica tu archivo .env");
    }
}

std::string OpenAIClient::makeRequest(const json& requestBody) {
    // Guardar request en archivo temporal
    std::string requestFile = "/tmp/openai_request_" + std::to_string(getpid()) + ".json";
    std::ofstream file(requestFile);
    file << requestBody.dump();
    file.close();
    
    // Construir comando curl con timeout de 30 segundos (aumentado para conexiones lentas)
    std::string curlCmd = "curl --max-time 30 -s --connect-timeout 10 https://api.openai.com/v1/chat/completions "
        "-H 'Content-Type: application/json' "
        "-H 'Authorization: Bearer " + apiKey + "' "
        "-d @" + requestFile + " 2>&1";
    
    // Ejecutar curl y capturar respuesta
    FILE* pipe = popen(curlCmd.c_str(), "r");
    if (!pipe) {
        system(("rm -f " + requestFile).c_str());
        return "{\"error\": {\"message\": \"No se pudo ejecutar curl. Verifica que curl esté instalado.\"}}";
    }
    
    std::string response;
    std::string errorOutput;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        response += buffer;
        // Capturar errores de curl
        std::string line(buffer);
        if (line.find("curl:") != std::string::npos || 
            line.find("error") != std::string::npos ||
            line.find("timeout") != std::string::npos) {
            errorOutput += line;
        }
    }
    int exitCode = pclose(pipe);
    
    // Limpiar archivo temporal
    system(("rm -f " + requestFile).c_str());
    
    // Verificar si hubo timeout o error
    if (exitCode != 0) {
        // Proporcionar mensaje de error más específico
        if (errorOutput.find("timeout") != std::string::npos || 
            errorOutput.find("timed out") != std::string::npos) {
            return "{\"error\": {\"message\": \"Timeout al conectar con OpenAI. La conexión tardó demasiado.\"}}";
        } else if (errorOutput.find("Could not resolve host") != std::string::npos) {
            return "{\"error\": {\"message\": \"No se pudo resolver el servidor de OpenAI. Verifica tu conexión a internet.\"}}";
        } else if (errorOutput.find("Connection refused") != std::string::npos) {
            return "{\"error\": {\"message\": \"Conexión rechazada por OpenAI. Verifica tu firewall.\"}}";
        } else {
            return "{\"error\": {\"message\": \"Error de conexión: " + errorOutput.substr(0, 100) + "\"}}";
        }
    }
    
    if (response.empty()) {
        return "{\"error\": {\"message\": \"Respuesta vacía de OpenAI. Verifica tu API key y conexión.\"}}";
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
        "IMPORTANTE: Responde EN ESPAÑOL LATINOAMERICANO. Usa un tono motivador y conciso (máximo 4 líneas).";
    
    // Crear request JSON
    json requestBody = {
        {"model", model},
        {"messages", json::array({
            {
                {"role", "system"},
                {"content", "Eres un coach de programación paciente y motivador. "
                           "Das pistas útiles sin revelar la solución completa. "
                           "Siempre responde en ESPAÑOL LATINOAMERICANO. Usa un lenguaje natural y coloquial de Latinoamérica."}
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
            } else if (errorMsg.find("quota") != std::string::npos || 
                      errorMsg.find("exceeded") != std::string::npos ||
                      errorMsg.find("billing") != std::string::npos) {
                return "Tu cuenta de OpenAI no tiene créditos disponibles o excedió su límite. "
                       "Visita https://platform.openai.com/account/billing para agregar créditos.";
            } else if (errorMsg.find("invalid") != std::string::npos || 
                      errorMsg.find("authentication") != std::string::npos ||
                      errorMsg.find("api key") != std::string::npos ||
                      errorMsg.find("Unauthorized") != std::string::npos) {
                return "Error: API Key de OpenAI inválida. Verifica tu archivo .env.";
            } else {
                return "No se pudo obtener feedback. Error: " + errorMsg;
            }
        }
    } catch (const std::exception& e) {
        return "Error al procesar respuesta del servicio de feedback. "
               "Por favor intenta nuevamente.";
    }

    return "No se pudo obtener feedback en este momento.";
}