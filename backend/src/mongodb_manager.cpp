#include "../include/mongodb_manager.h"
#include "../include/motor_eval.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <unistd.h>

// Implementación de Problem
json Problem::toJson() const {
    json testCasesJson = json::array();
    for (const auto& tc : testCases) {
        testCasesJson.push_back({
            {"input", tc.input},
            {"expectedOutput", tc.expectedOutput}
        });
    }
    
    return {
        {"id", id},
        {"title", title},
        {"description", description},
        {"difficulty", difficulty},
        {"category", category},
        {"testCases", testCasesJson},
        {"solutionTemplate", solutionTemplate},
        {"hints", hints}
    };
}

Problem Problem::fromJson(const json& j) {
    Problem p;
    p.id = j.value("id", "");
    p.title = j.value("title", "");
    p.description = j.value("description", "");
    p.difficulty = j.value("difficulty", "");
    p.category = j.value("category", "");
    p.solutionTemplate = j.value("solutionTemplate", "");
    p.hints = j.value("hints", "");
    
    if (j.contains("testCases") && j["testCases"].is_array()) {
        for (const auto& tc : j["testCases"]) {
            TestCase testCase;
            testCase.input = tc.value("input", "");
            testCase.expectedOutput = tc.value("expectedOutput", "");
            p.testCases.push_back(testCase);
        }
    }
    
    return p;
}

// Implementación de MongoDBManager
MongoDBManager::MongoDBManager(const std::string& uri) 
    : connectionURI(uri), 
      databaseName("codecoach"),
      collectionName("problems") {
}

MongoDBManager::~MongoDBManager() {
}

std::string MongoDBManager::escapeJsonString(const std::string& str) {
    std::string escaped;
    for (char c : str) {
        switch (c) {
            case '"':  escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:   escaped += c; break;
        }
    }
    return escaped;
}

std::string MongoDBManager::executeMongoCommand(const std::string& command) {
    // Crear archivo temporal con el comando
    std::string tempFile = "/tmp/mongo_cmd_" + std::to_string(getpid()) + ".js";
    std::ofstream file(tempFile);
    file << command;
    file.close();
    
    // Ejecutar mongosh con el comando
    std::string cmd = "mongosh \"" + connectionURI + "\" --quiet --eval \"$(cat " + tempFile + ")\" 2>&1";
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        remove(tempFile.c_str());
        return "{\"error\": \"No se pudo conectar a MongoDB\"}";
    }
    
    std::string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    pclose(pipe);
    
    // Limpiar archivo temporal
    remove(tempFile.c_str());
    
    return output;
}

json MongoDBManager::parseMongoResponse(const std::string& response) {
    try {
        // Verificar si hay errores de conexión antes de parsear
        if (response.find("MongoServerSelectionError") != std::string::npos ||
            response.find("Network Access List") != std::string::npos ||
            response.find("authentication failed") != std::string::npos ||
            response.find("SSL") != std::string::npos) {
            // Retornar error estructurado
            return json{{"error", "Error de conexión a MongoDB. Verifica la Network Access List en MongoDB Atlas."}};
        }
        
        // Limpiar la respuesta (remover líneas de log de mongosh)
        std::string cleaned;
        std::istringstream iss(response);
        std::string line;
        bool jsonStarted = false;
        
        while (std::getline(iss, line)) {
            // Buscar inicio de JSON
            if (line.find('{') != std::string::npos || 
                line.find('[') != std::string::npos) {
                jsonStarted = true;
            }
            
            if (jsonStarted) {
                cleaned += line + "\n";
            }
        }
        
        if (cleaned.empty()) {
            return json::object();
        }
        
        return json::parse(cleaned);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing MongoDB response: " << e.what() << std::endl;
        std::cerr << "Response was: " << response << std::endl;
        return json::object();
    }
}

bool MongoDBManager::testConnection() {
    std::string command = 
        "db = db.getSiblingDB('" + databaseName + "'); "
        "print(EJSON.stringify({success: true, database: db.getName()}));";
    
    std::string response = executeMongoCommand(command);
    
    // Verificar si hay errores de conexión en la respuesta
    if (response.find("MongoServerSelectionError") != std::string::npos ||
        response.find("Network Access List") != std::string::npos ||
        response.find("authentication failed") != std::string::npos) {
        std::cerr << "Error de conexión a MongoDB: " << response << std::endl;
        return false;
    }
    
    json result = parseMongoResponse(response);
    
    return result.value("success", false);
}

int MongoDBManager::getProblemsCount() {
    std::string command = 
        "db = db.getSiblingDB('" + databaseName + "'); "
        "count = db." + collectionName + ".countDocuments(); "
        "print(EJSON.stringify({count: count}));";
    
    std::string response = executeMongoCommand(command);
    json result = parseMongoResponse(response);
    
    return result.value("count", 0);
}

bool MongoDBManager::createProblem(const Problem& problem) {
    // Validar que el problema tenga campos requeridos
    if (problem.id.empty() || problem.title.empty()) {
        std::cerr << "Error: El problema debe tener id y title" << std::endl;
        return false;
    }
    
    json problemJson = problem.toJson();
    
    std::string command = 
        "db = db.getSiblingDB('" + databaseName + "'); "
        "result = db." + collectionName + ".insertOne(" + problemJson.dump() + "); "
        "print(EJSON.stringify({success: result.acknowledged}));";
    
    std::string response = executeMongoCommand(command);
    json result = parseMongoResponse(response);
    
    return result.value("success", false);
}

Problem MongoDBManager::getProblem(const std::string& id) {
    std::string command = 
        "db = db.getSiblingDB('" + databaseName + "'); "
        "doc = db." + collectionName + ".findOne({id: '" + id + "'}); "
        "print(EJSON.stringify(doc));";
    
    std::string response = executeMongoCommand(command);
    json result = parseMongoResponse(response);
    
    if (result.is_null() || result.empty()) {
        return Problem();
    }
    
    return Problem::fromJson(result);
}

std::vector<Problem> MongoDBManager::getAllProblems() {
    std::string command = 
        "db = db.getSiblingDB('" + databaseName + "'); "
        "docs = db." + collectionName + ".find().toArray(); "
        "print(EJSON.stringify(docs));";
    
    std::string response = executeMongoCommand(command);
    json result = parseMongoResponse(response);
    
    std::vector<Problem> problems;
    
    if (result.is_array()) {
        for (const auto& doc : result) {
            problems.push_back(Problem::fromJson(doc));
        }
    }
    
    return problems;
}

std::vector<Problem> MongoDBManager::getProblemsByCategory(const std::string& category) {
    std::string command = 
        "db = db.getSiblingDB('" + databaseName + "'); "
        "docs = db." + collectionName + ".find({category: '" + category + "'}).toArray(); "
        "print(EJSON.stringify(docs));";
    
    std::string response = executeMongoCommand(command);
    json result = parseMongoResponse(response);
    
    std::vector<Problem> problems;
    
    if (result.is_array()) {
        for (const auto& doc : result) {
            problems.push_back(Problem::fromJson(doc));
        }
    }
    
    return problems;
}

std::vector<Problem> MongoDBManager::getProblemsByDifficulty(const std::string& difficulty) {
    std::string command = 
        "db = db.getSiblingDB('" + databaseName + "'); "
        "docs = db." + collectionName + ".find({difficulty: '" + difficulty + "'}).toArray(); "
        "print(EJSON.stringify(docs));";
    
    std::string response = executeMongoCommand(command);
    json result = parseMongoResponse(response);
    
    std::vector<Problem> problems;
    
    if (result.is_array()) {
        for (const auto& doc : result) {
            problems.push_back(Problem::fromJson(doc));
        }
    }
    
    return problems;
}

bool MongoDBManager::updateProblem(const std::string& id, const Problem& problem) {
    json problemJson = problem.toJson();
    
    std::string command = 
        "db = db.getSiblingDB('" + databaseName + "'); "
        "result = db." + collectionName + ".updateOne("
        "{id: '" + id + "'}, "
        "{$set: " + problemJson.dump() + "}); "
        "print(EJSON.stringify({success: result.modifiedCount > 0}));";
    
    std::string response = executeMongoCommand(command);
    json result = parseMongoResponse(response);
    
    return result.value("success", false);
}

bool MongoDBManager::deleteProblem(const std::string& id) {
    std::string command = 
        "db = db.getSiblingDB('" + databaseName + "'); "
        "result = db." + collectionName + ".deleteOne({id: '" + id + "'}); "
        "print(EJSON.stringify({success: result.deletedCount > 0}));";
    
    std::string response = executeMongoCommand(command);
    json result = parseMongoResponse(response);
    
    return result.value("success", false);
}