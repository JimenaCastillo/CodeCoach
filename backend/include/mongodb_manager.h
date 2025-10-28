#ifndef MONGODB_MANAGER_H
#define MONGODB_MANAGER_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "motor_eval.h"

using json = nlohmann::json;
struct Problem {
    std::string id;
    std::string title;
    std::string description;
    std::string difficulty;  // "Easy", "Medium", "Hard"
    std::string category;    // "Array", "String", "Dynamic Programming", etc.
    std::vector<TestCase> testCases;
    std::string solutionTemplate;
    std::string hints;
    
    // Convertir a JSON
    json toJson() const;
    
    // Crear desde JSON
    static Problem fromJson(const json& j);
};

class MongoDBManager {
private:
    std::string connectionURI;
    std::string databaseName;
    std::string collectionName;
    
public:
    MongoDBManager(const std::string& uri);
    ~MongoDBManager();
    
    // CRUD Operations
    bool createProblem(const Problem& problem);
    Problem getProblem(const std::string& id);
    std::vector<Problem> getAllProblems();
    std::vector<Problem> getProblemsByCategory(const std::string& category);
    std::vector<Problem> getProblemsByDifficulty(const std::string& difficulty);
    bool updateProblem(const std::string& id, const Problem& problem);
    bool deleteProblem(const std::string& id);
    
    // Utilidades
    bool testConnection();
    int getProblemsCount();
    
private:
    // Helpers para ejecutar comandos mongosh
    std::string executeMongoCommand(const std::string& command);
    json parseMongoResponse(const std::string& response);
    std::string escapeJsonString(const std::string& str);
};

#endif