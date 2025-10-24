#ifndef REST_API_H
#define REST_API_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class RestAPI {
private:
    int port;
    std::string baseURL;
    std::string apiKey;

public:
    RestAPI(int port);
    void start();
    
    // Endpoints
    json getProblems();
    json getProblem(const std::string& id);
    json createProblem(const json& problemData);
    
private:
    // Helpers
    std::string loadEnvVariable(const std::string& key);
};

#endif