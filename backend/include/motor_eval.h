#ifndef MOTOR_EVAL_H
#define MOTOR_EVAL_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct TestCase {
    std::string input;
    std::string expectedOutput;
};

struct EvaluationResult {
    bool success;
    int testsPassed;
    int testsFailed;
    std::string compilationError;
    std::string runtimeError;
    double executionTime;
    std::vector<std::string> testOutputs;
    std::vector<bool> testResults;
};

class MotorEval {
private:
    std::string tempDir;
    int timeoutSeconds;
    
public:
    MotorEval(int timeout = 5);
    ~MotorEval();
    
    EvaluationResult evaluate(const std::string& userCode, 
                             const std::vector<TestCase>& testCases);
    
private:
    std::string compileCpp(const std::string& code);
    std::string executeProgram(const std::string& execPath, 
                              const std::string& input);
    std::string trimOutput(const std::string& output);
    int executeWithTimeout(const std::string& command, 
                          std::string& output);
};

#endif