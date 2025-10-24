#include "motor_eval.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <unistd.h>
#include <chrono>

MotorEval::MotorEval(int timeout) : timeoutSeconds(timeout) {
    tempDir = "/tmp/codecoach_temp_";
    tempDir += std::to_string(getpid());
    system(("mkdir -p " + tempDir).c_str());
}

MotorEval::~MotorEval() {
    system(("rm -rf " + tempDir).c_str());
}

std::string MotorEval::trimOutput(const std::string& output) {
    size_t start = output.find_first_not_of(" \n\r\t");
    size_t end = output.find_last_not_of(" \n\r\t");
    
    if (start == std::string::npos) return "";
    return output.substr(start, end - start + 1);
}

std::string MotorEval::compileCpp(const std::string& code) {
    std::string codeFile = tempDir + "/code.cpp";
    std::string execFile = tempDir + "/code";
    
    std::ofstream file(codeFile);
    if (!file.is_open()) {
        return "Error: No se pudo crear archivo temporal";
    }
    file << code;
    file.close();
    
    std::string compileCmd = "g++ -o " + execFile + " " + codeFile + " 2>&1";
    FILE* pipe = popen(compileCmd.c_str(), "r");
    
    std::string error;
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        error += buffer;
    }
    pclose(pipe);
    
    if (!error.empty()) {
        return error;
    }
    
    return "";
}

std::string MotorEval::executeProgram(const std::string& execPath, 
                                      const std::string& input) {
    std::string output;
    
    std::string cmd = "echo '" + input + "' | " + execPath + " 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    
    if (!pipe) return "Error al ejecutar programa";
    
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    pclose(pipe);
    
    return output;
}

int MotorEval::executeWithTimeout(const std::string& command, 
                                  std::string& output) {
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return -1;
    
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    return pclose(pipe) >> 8;
}

EvaluationResult MotorEval::evaluate(const std::string& userCode, 
                                     const std::vector<TestCase>& testCases) {
    EvaluationResult result;
    result.testsPassed = 0;
    result.testsFailed = 0;
    result.success = false;
    result.executionTime = 0.0;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Compilar
    std::string compileError = compileCpp(userCode);
    if (!compileError.empty()) {
        result.compilationError = compileError;
        result.testsFailed = testCases.size();
        return result;
    }
    
    // Ejecutar test cases
    std::string execPath = tempDir + "/code";
    
    for (const auto& testCase : testCases) {
        std::string output = executeProgram(execPath, testCase.input);
        output = trimOutput(output);
        
        std::string expected = trimOutput(testCase.expectedOutput);
        
        result.testOutputs.push_back(output);
        
        bool passed = (output == expected);
        result.testResults.push_back(passed);
        
        if (passed) {
            result.testsPassed++;
        } else {
            result.testsFailed++;
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTime = std::chrono::duration<double>(endTime - startTime).count();
    
    result.success = (result.testsFailed == 0);
    
    return result;
}