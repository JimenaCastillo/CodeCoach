#include "motor_eval.h"
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <chrono>
#include <sys/wait.h>
#include <sys/stat.h>

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

// Escapar caracteres especiales para shell
std::string escapeShellArg(const std::string& arg) {
    std::string escaped;
    for (char c : arg) {
        if (c == '\'' || c == '\\' || c == '"' || c == '$' || c == '`') {
            escaped += '\\';
        }
        escaped += c;
    }
    return escaped;
}

std::string MotorEval::compileCpp(const std::string& code) {
    // Validación de tamaño
    if (code.size() > 50000) { // 50KB máximo
        return "Error: Código demasiado largo (máximo 50KB)";
    }
    
    std::string codeFile = tempDir + "/code.cpp";
    std::string execFile = tempDir + "/code";
    
    std::ofstream file(codeFile);
    if (!file.is_open()) {
        return "Error: No se pudo crear archivo temporal";
    }
    file << code;
    file.close();
    
    // Compilar con flags de seguridad
    std::string compileCmd = "g++ -std=c++17 -Wall -Wextra -O2 -o " + 
                            execFile + " " + codeFile + " 2>&1";
    FILE* pipe = popen(compileCmd.c_str(), "r");
    
    std::string error;
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        error += buffer;
        // Limitar longitud del error
        if (error.size() > 2000) {
            error += "\n... (error truncado)";
            break;
        }
    }
    pclose(pipe);
    
    if (!error.empty()) {
        return error;
    }
    
    return "";
}

std::string MotorEval::executeProgram(const std::string& execPath, 
                                      const std::string& input) {
    // Validar que el ejecutable existe
    struct stat buffer;
    if (stat(execPath.c_str(), &buffer) != 0) {
        return "Error: El ejecutable no existe: " + execPath;
    }
    
    // Validar permisos de ejecución
    if ((buffer.st_mode & S_IXUSR) == 0) {
        return "Error: El ejecutable no tiene permisos de ejecución";
    }
    
    std::string output;
    std::string inputFile = tempDir + "/input.txt";
    std::string outputFile = tempDir + "/output.txt";
    
    // Escribir input en archivo (más seguro que echo)
    std::ofstream inFile(inputFile);
    inFile << input;
    inFile.close();
    
    // Ejecutar con timeout usando timeout command de Linux
    std::string cmd = "timeout " + std::to_string(timeoutSeconds) + 
                     "s " + execPath + " < " + inputFile + 
                     " > " + outputFile + " 2>&1";
    
    int exitCode = system(cmd.c_str());
    
    // Verificar si hubo timeout (exit code 124)
    if (WEXITSTATUS(exitCode) == 124) {
        return "TIMEOUT: El programa excedió el límite de " + 
               std::to_string(timeoutSeconds) + " segundos";
    }
    
    // Leer output
    std::ifstream outFile(outputFile);
    if (outFile.is_open()) {
        std::string line;
        while (std::getline(outFile, line)) {
            output += line + "\n";
            // Limitar tamaño de output
            if (output.size() > 10000) {
                output += "\n... (output truncado)";
                break;
            }
        }
        outFile.close();
    }
    
    // Limpiar archivos temporales
    remove(inputFile.c_str());
    remove(outputFile.c_str());
    
    return output;
}

int MotorEval::executeWithTimeout(const std::string& command, 
                                  std::string& output) {
    std::string cmd = "timeout " + std::to_string(timeoutSeconds) + "s " + command;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return -1;
    
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    int status = pclose(pipe);
    return WEXITSTATUS(status);
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
    
    // Validar que el ejecutable existe antes de ejecutar
    struct stat buffer;
    if (stat(execPath.c_str(), &buffer) != 0) {
        result.success = false;
        result.compilationError = "Error: El ejecutable no se generó correctamente";
        result.testsFailed = testCases.size();
        return result;
    }
    
    // Validar permisos de ejecución
    if ((buffer.st_mode & S_IXUSR) == 0) {
        result.success = false;
        result.compilationError = "Error: El ejecutable no tiene permisos de ejecución";
        result.testsFailed = testCases.size();
        return result;
    }
    
    for (const auto& testCase : testCases) {
        std::string output = executeProgram(execPath, testCase.input);
        
        // Verificar si hubo timeout
        if (output.find("TIMEOUT:") == 0) {
            result.runtimeError = output;
            result.testsFailed = testCases.size();
            break;
        }
        
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
    
    result.success = (result.testsFailed == 0 && result.runtimeError.empty());
    
    return result;
}