#ifndef OPENAI_CLIENT_H
#define OPENAI_CLIENT_H

#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class OpenAIClient {
private:
    std::string apiKey;
    std::string model;
    
public:
    OpenAIClient(const std::string& key);
    
    std::string generateFeedback(const std::string& code,
                                 int testsPassed,
                                 int testsFailed,
                                 const std::string& compilationError,
                                 const std::string& runtimeError);
    
private:
    std::string makeRequest(const json& requestBody);
};

#endif