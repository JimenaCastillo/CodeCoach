#include "rest_api.h"
#include <httplib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include "openai_client.h"
#include "motor_eval.h"

RestAPI::RestAPI(int port) : port(port) {
    baseURL = "mongodb+srv://kgarro3_db_user:OQQkcyW2wqw2kWzV@clustercodecoach.cz5islf.mongodb.net/";
}

std::string RestAPI::loadEnvVariable(const std::string& key) {
    std::ifstream envFile(".env");
    std::string line;
    
    if (!envFile.is_open()) {
        std::cerr << "Error: No se pudo abrir .env" << std::endl;
        return "";
    }
    
    while (std::getline(envFile, line)) {
        if (line.find(key) == 0) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                return line.substr(pos + 1);
            }
        }
    }
    
    return "";
}

void RestAPI::start() {
    httplib::Server svr;
    
    // Endpoint: GET /api/problems - Obtener lista de problemas
    svr.Get("/api/problems", [this](const httplib::Request&, httplib::Response& res) {
        json response = json::array();
        
        // Problemas de ejemplo
        response.push_back({
            {"id", "1"},
            {"title", "Two Sum"},
            {"difficulty", "Easy"},
            {"category", "Array"},
            {"description", "Dado un array de números, encuentra dos que sumen a un target"}
        });
        
        response.push_back({
            {"id", "2"},
            {"title", "Reverse String"},
            {"difficulty", "Easy"},
            {"category", "String"},
            {"description", "Reversa una cadena de texto"}
        });
        
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");
        res.set_content(response.dump(), "application/json");
    });
    
    // Endpoint: GET /api/problems/:id - Obtener problema específico
    svr.Get("/api/problems/(\\d+)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        
        json problem = {
            {"id", id},
            {"title", "Problem " + id},
            {"description", "Problem description"},
            {"testCases", json::array()}
        };
        
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");
        res.set_content(problem.dump(), "application/json");
    });
    
    // Endpoint: POST /api/problems - Crear nuevo problema
    svr.Post("/api/problems", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            
            json response = {
                {"success", true},
                {"message", "Problema creado exitosamente"},
                {"problem", body}
            };
            
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Content-Type", "application/json");
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            json error = {
                {"success", false},
                {"error", e.what()}
            };
            
            res.set_header("Content-Type", "application/json");
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        }
    });
    
    // Endpoint: OPTIONS (para CORS preflight)
    svr.Options("/api/.*", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    });
    
    // Endpoint: POST /api/feedback - Obtener feedback de OpenAI
    svr.Post("/api/feedback", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string code = body["code"];
            int testsPassed = body["testsPassed"];
            int testsFailed = body["testsFailed"];
            std::string compilationError = body.value("compilationError", "");
            std::string runtimeError = body.value("runtimeError", "");
            
            // Obtener API Key del .env
            std::ifstream envFile(".env");
            std::string apiKey;
            std::string line;
            while (std::getline(envFile, line)) {
                if (line.find("OPENAI_API_KEY=") == 0) {
                    apiKey = line.substr(15);
                    break;
                }
            }
            envFile.close();
            
            if (apiKey.empty()) {
                json error = {
                    {"success", false},
                    {"error", "API Key de OpenAI no configurada"}
                };
                res.set_header("Content-Type", "application/json");
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }
            
            // Generar feedback
            OpenAIClient openai(apiKey);
            std::string feedback = openai.generateFeedback(
                code, testsPassed, testsFailed, 
                compilationError, runtimeError
            );
            
            json response = {
                {"success", true},
                {"feedback", feedback}
            };
            
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Content-Type", "application/json");
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            json error = {
                {"success", false},
                {"error", e.what()}
            };
            
            res.set_header("Content-Type", "application/json");
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        }
    });
    
    // Health check
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Content-Type", "application/json");
        res.set_content("{\"status\": \"ok\"}", "application/json");
    });
    
    // Endpoint: POST /api/execute - Ejecutar código
    svr.Post("/api/execute", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string code = body["code"];
            auto testCasesJson = body["testCases"];
            
            // Convertir JSON a vector de TestCase
            std::vector<TestCase> testCases;
            for (const auto& tc : testCasesJson) {
                TestCase test;
                test.input = tc["input"].get<std::string>();
                test.expectedOutput = tc["expectedOutput"].get<std::string>();
                testCases.push_back(test);
            }
            
            // Ejecutar con MotorEval
            MotorEval evaluator(5);
            EvaluationResult result = evaluator.evaluate(code, testCases);
            
            // Construir respuesta
            json response = {
                {"success", result.success},
                {"testsPassed", result.testsPassed},
                {"testsFailed", result.testsFailed},
                {"executionTime", result.executionTime},
                {"compilationError", result.compilationError},
                {"runtimeError", result.runtimeError},
                {"testOutputs", result.testOutputs},
                {"testResults", result.testResults}
            };
            
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Content-Type", "application/json");
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            json error = {
                {"success", false},
                {"error", e.what()}
            };
            
            res.set_header("Content-Type", "application/json");
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        }
    });
    
    std::cout << "🚀 CodeCoach Backend iniciando en puerto " << port << std::endl;
    std::cout << "📍 http://localhost:" << port << std::endl;
    std::cout << "✅ Health check: http://localhost:" << port << "/health" << std::endl;
    
    svr.listen("localhost", port);
}