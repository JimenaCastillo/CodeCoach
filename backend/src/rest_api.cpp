#include "rest_api.h"
#include "mongodb_manager.h"
#include "solution_analyzer.h"
#include <httplib.h>
#include <iostream>
#include <fstream>
#include <vector>
#include "openai_client.h"
#include "motor_eval.h"

RestAPI::RestAPI(int port) : port(port) {
    baseURL = "mongodb+srv://kgarro3_db_user:OQQkcyW2wqw2kWzV@clustercodecoach.cz5islf.mongodb.net/";
}

std::string RestAPI::loadEnvVariable(const std::string& key) {
    // Buscar .env en múltiples ubicaciones posibles
    std::vector<std::string> possiblePaths = {
        ".env",                                    // Directorio actual
        "../.env",                                 // Un nivel arriba
        "../../.env",                              // Dos niveles arriba
        "../../backend/.env"                       // En backend/
    };
    
    std::ifstream envFile;
    std::string envPath;
    
    // Intentar abrir .env desde diferentes ubicaciones
    for (const auto& path : possiblePaths) {
        envFile.open(path);
        if (envFile.is_open()) {
            envPath = path;
            break;
        }
    }
    
    if (!envFile.is_open()) {
        std::cerr << "Error: No se pudo abrir .env" << std::endl;
        return "";
    }
    
    std::string line;
    
    while (std::getline(envFile, line)) {
        if (line.find(key) == 0) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                envFile.close();
                return line.substr(pos + 1);
            }
        }
    }
    
    envFile.close();
    return "";
}

void RestAPI::start() {
    httplib::Server svr;
    
    // Cargar variables de entorno
    std::string mongoURI = loadEnvVariable("MONGODB_URI");
    std::string openaiKey = loadEnvVariable("OPENAI_API_KEY");
    
    if (mongoURI.empty()) {
        std::cerr << "Advertencia: MONGODB_URI no configurado, usando problemas hardcodeados" << std::endl;
    }
    
    if (openaiKey.empty()) {
        std::cerr << "Advertencia: OPENAI_API_KEY no configurado" << std::endl;
    }
    
    // Crear instancias de managers
    MongoDBManager* dbManager = nullptr;
    if (!mongoURI.empty()) {
        dbManager = new MongoDBManager(mongoURI);
        if (dbManager->testConnection()) {
            std::cout << "Conectado a MongoDB" << std::endl;
            std::cout << "Problemas en BD: " << dbManager->getProblemsCount() << std::endl;
        } else {
            std::cerr << "Error: No se pudo conectar a MongoDB" << std::endl;
            delete dbManager;
            dbManager = nullptr;
        }
    }
    
    // Endpoint: GET /api/problems - Obtener lista de problemas
    svr.Get("/api/problems", [dbManager](const httplib::Request& /* req */, httplib::Response& res) {
        json response = json::array();
        
        // Intentar obtener de MongoDB
        if (dbManager) {
            try {
                std::vector<Problem> problems = dbManager->getAllProblems();
                
                for (const auto& problem : problems) {
                    response.push_back({
                        {"id", problem.id},
                        {"title", problem.title},
                        {"difficulty", problem.difficulty},
                        {"category", problem.category},
                        {"description", problem.description}
                    });
                }
            } catch (const std::exception& e) {
                std::cerr << "Error al obtener problemas de MongoDB: " << e.what() << std::endl;
                // Continuar con fallback hardcodeado
            }
        }
        
        // Si MongoDB no tiene problemas, usar hardcodeados
        if (response.empty()) {
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
        }
        
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");
        res.set_content(response.dump(), "application/json");
    });
    
    // Endpoint: GET /api/problems/:id - Obtener problema específico
    svr.Get("/api/problems/(\\w+)", [dbManager](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        json problem;
        
        // Intentar obtener de MongoDB
        if (dbManager) {
            try {
                Problem p = dbManager->getProblem(id);
                
                if (!p.id.empty()) {
                    problem = p.toJson();
                }
            } catch (const std::exception& e) {
                std::cerr << "Error al obtener problema de MongoDB: " << e.what() << std::endl;
                // Continuar con fallback hardcodeado
            }
        }
        
        // Fallback: problemas hardcodeados
        if (problem.empty()) {
            if (id == "1") {
                problem = {
                    {"id", "1"},
                    {"title", "Two Sum"},
                    {"description", "Dado un array de enteros nums y un entero target, "
                                   "retorna los índices de dos números que sumen target."},
                    {"difficulty", "Easy"},
                    {"category", "Array"},
                    {"testCases", json::array({
                        {{"input", "2 7 11 15\n9"}, {"expectedOutput", "0 1"}},
                        {{"input", "3 2 4\n6"}, {"expectedOutput", "1 2"}},
                        {{"input", "3 3\n6"}, {"expectedOutput", "0 1"}}
                    })},
                    {"solutionTemplate", "#include <iostream>\n#include <vector>\nusing namespace std;\n\n"
                                        "int main() {\n    // === BEGIN USER CODE ===\n    // Tu código aquí\n    // === END USER CODE ===\n    return 0;\n}"}
                };
            } else if (id == "2") {
                problem = {
                    {"id", "2"},
                    {"title", "Reverse String"},
                    {"description", "Dada una cadena, devuelve la cadena invertida."},
                    {"difficulty", "Easy"},
                    {"category", "String"},
                    {"testCases", json::array({
                        {{"input", "hello"}, {"expectedOutput", "olleh"}},
                        {{"input", "world"}, {"expectedOutput", "dlrow"}},
                        {{"input", "a"}, {"expectedOutput", "a"}}
                    })},
                    {"solutionTemplate", "#include <iostream>\n#include <string>\nusing namespace std;\n\n"
                                        "int main() {\n    // === BEGIN USER CODE ===\n    // Tu código aquí\n    // === END USER CODE ===\n    return 0;\n}"}
                };
            } else {
                problem = {
                    {"error", "Problema no encontrado"}
                };
                res.status = 404;
            }
        }
        
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");
        res.set_content(problem.dump(), "application/json");
    });
    
    // Endpoint: POST /api/problems - Crear nuevo problema (ADMIN)
    svr.Post("/api/problems", [dbManager](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            
            if (!dbManager) {
                json error = {
                    {"success", false},
                    {"error", "MongoDB no está configurado"}
                };
                res.set_header("Content-Type", "application/json");
                res.set_content(error.dump(), "application/json");
                res.status = 503;
                return;
            }
            
            // Crear problema desde JSON
            Problem problem = Problem::fromJson(body);
            
            // Validaciones
            if (problem.id.empty() || problem.title.empty()) {
                json error = {
                    {"success", false},
                    {"error", "ID y título son requeridos"}
                };
                res.set_header("Content-Type", "application/json");
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }
            
            bool created = dbManager->createProblem(problem);
            
            json response = {
                {"success", created},
                {"message", created ? "Problema creado exitosamente" : "Error al crear problema"},
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
    
    // Endpoint: POST /api/execute - Ejecutar código
    svr.Post("/api/execute", [openaiKey](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            
            // Validar que code existe y es string
            if (!body.contains("code") || !body["code"].is_string()) {
                json error = {
                    {"success", false},
                    {"error", "Campo 'code' es requerido y debe ser un string"}
                };
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_header("Content-Type", "application/json");
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }
            
            std::string code = body["code"];
            
            // Validar que code no esté vacío
            if (code.empty()) {
                json error = {
                    {"success", false},
                    {"error", "El código no puede estar vacío"}
                };
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_header("Content-Type", "application/json");
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }
            
            // Validar que testCases existe y es array
            if (!body.contains("testCases") || !body["testCases"].is_array()) {
                json error = {
                    {"success", false},
                    {"error", "Campo 'testCases' es requerido y debe ser un array"}
                };
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_header("Content-Type", "application/json");
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }
            
            auto testCasesJson = body["testCases"];
            
            // Validar que testCases no esté vacío
            if (testCasesJson.empty()) {
                json error = {
                    {"success", false},
                    {"error", "Debe proporcionar al menos un test case"}
                };
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_header("Content-Type", "application/json");
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }
            
            std::string problemDescription = body.value("problemDescription", "");
            
            // Validación de tamaño
            if (code.size() > 50000) {
                json error = {
                    {"success", false},
                    {"error", "Código demasiado largo (máximo 50KB)"}
                };
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_header("Content-Type", "application/json");
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }
            
            // Convertir JSON a vector de TestCase
            std::vector<TestCase> testCases;
            for (const auto& tc : testCasesJson) {
                // Validar que cada testCase tenga input y expectedOutput
                if (!tc.contains("input") || !tc.contains("expectedOutput")) {
                    json error = {
                        {"success", false},
                        {"error", "Cada test case debe tener 'input' y 'expectedOutput'"}
                    };
                    res.set_header("Access-Control-Allow-Origin", "*");
                    res.set_header("Content-Type", "application/json");
                    res.set_content(error.dump(), "application/json");
                    res.status = 400;
                    return;
                }
                
                TestCase test;
                test.input = tc["input"].get<std::string>();
                test.expectedOutput = tc["expectedOutput"].get<std::string>();
                testCases.push_back(test);
            }
            
            // Ejecutar con MotorEval
            MotorEval evaluator(5); // 5 segundos timeout
            EvaluationResult result = evaluator.evaluate(code, testCases);
            
            // Análisis de complejidad (opcional, puede fallar)
            CodeAnalysis analysis;
            try {
                if (!code.empty()) {
                    SolutionAnalyzer analyzer(openaiKey);
                    analysis = analyzer.analyze(
                        code, 
                        problemDescription,
                        result.testsPassed, 
                        result.testsFailed
                    );
                }
            } catch (const std::exception& ex) {
                // Si falla el análisis, continuar sin él
                std::cerr << "Advertencia: Error en análisis de complejidad: " << ex.what() << std::endl;
                analysis.complexity.timeComplexity = "N/A";
                analysis.complexity.spaceComplexity = "N/A";
                analysis.complexity.confidence = "low";
                analysis.complexity.explanation = "No se pudo analizar la complejidad";
                analysis.status = "error"; // CHANGE: Indica fallo en la etapa de análisis.
            }
            
            // Construir respuesta con análisis
            json response = {
                {"success", result.success},
                {"testsPassed", result.testsPassed},
                {"testsFailed", result.testsFailed},
                {"executionTime", result.executionTime},
                {"compilationError", result.compilationError},
                {"runtimeError", result.runtimeError},
                {"testOutputs", result.testOutputs},
                {"testResults", result.testResults},
                {"analysis", {
                    {"timeComplexity", analysis.complexity.timeComplexity},
                    {"spaceComplexity", analysis.complexity.spaceComplexity},
                    {"confidence", analysis.complexity.confidence},
                    {"explanation", analysis.complexity.explanation},
                    {"status", analysis.status}, // CHANGE: Propaga estado NoUserCode al frontend.
                    {"dataStructures", analysis.dataStructures},
                    {"patterns", json::array()},
                    {"suggestions", analysis.suggestions}
                }}
            };
            
            // Agregar patrones detectados
            for (const auto& pattern : analysis.patterns) {
                response["analysis"]["patterns"].push_back({
                    {"name", pattern.patternName},
                    {"confidence", pattern.confidence},
                    {"description", pattern.description}
                });
            }
            
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
    
    // Endpoint: POST /api/feedback - Obtener feedback de OpenAI
    svr.Post("/api/feedback", [openaiKey](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            
            // Validar que code existe
            if (!body.contains("code") || !body["code"].is_string()) {
                json error = {
                    {"success", false},
                    {"error", "Campo 'code' es requerido y debe ser un string"}
                };
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_header("Content-Type", "application/json");
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }
            
            std::string code = body["code"];
            
            // Validar que testsPassed existe y es número
            if (!body.contains("testsPassed") || !body["testsPassed"].is_number_integer()) {
                json error = {
                    {"success", false},
                    {"error", "Campo 'testsPassed' es requerido y debe ser un número"}
                };
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_header("Content-Type", "application/json");
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }
            
            // Validar que testsFailed existe y es número
            if (!body.contains("testsFailed") || !body["testsFailed"].is_number_integer()) {
                json error = {
                    {"success", false},
                    {"error", "Campo 'testsFailed' es requerido y debe ser un número"}
                };
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_header("Content-Type", "application/json");
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }
            
            int testsPassed = body["testsPassed"];
            int testsFailed = body["testsFailed"];
            
            // Validar que los valores sean >= 0
            if (testsPassed < 0 || testsFailed < 0) {
                json error = {
                    {"success", false},
                    {"error", "testsPassed y testsFailed deben ser >= 0"}
                };
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_header("Content-Type", "application/json");
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }
            
            std::string compilationError = body.value("compilationError", "");
            std::string runtimeError = body.value("runtimeError", "");
            
            if (openaiKey.empty()) {
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
            OpenAIClient openai(openaiKey);
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
    
    // Endpoint: OPTIONS (para CORS preflight)
    svr.Options("/api/.*", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    });
    
    // Health check
    svr.Get("/health", [dbManager](const httplib::Request&, httplib::Response& res) {
        json health = {
            {"status", "ok"},
            {"mongodb", dbManager != nullptr ? "connected" : "disconnected"}
        };
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");
        res.set_content(health.dump(), "application/json");
    });
    
    // Manejo de errores global
    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");
        json error = {
            {"error", "Error interno del servidor"},
            {"status", res.status}
        };
        res.set_content(error.dump(), "application/json");
    });
    
    std::cout << "🚀 CodeCoach Backend iniciando en puerto " << port << std::endl;
    std::cout << "📍 http://localhost:" << port << std::endl;
    std::cout << "✅ Health check: http://localhost:" << port << "/health" << std::endl;
    
    svr.listen("localhost", port);
    
    // Cleanup
    if (dbManager) {
        delete dbManager;
    }
}
