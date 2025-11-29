#include "motor_eval.h"
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <chrono>
#include <sys/wait.h>
#include <sys/stat.h>

// Constructor del Motor de Evaluación
MotorEval::MotorEval(int timeout) : timeoutSeconds(timeout) {
    // Construir path del directorio temporal
    tempDir = "/tmp/codecoach_temp_";
    tempDir += std::to_string(getpid()); // getpid() retorna el Process ID actual

    // Crear directorio temporal (mkdir -p crea directorios padre si no existen)
    system(("mkdir -p " + tempDir).c_str());
}

// Destructor - Limpia recursos
MotorEval::~MotorEval() {
    system(("rm -rf " + tempDir).c_str());
}

// Limpia espacios en blanco al inicio y final de un string
std::string MotorEval::trimOutput(const std::string& output) {
    size_t start = output.find_first_not_of(" \n\r\t"); // Buscar primer carácter que no sea espacio/tab/newline
    size_t end = output.find_last_not_of(" \n\r\t"); // Buscar último carácter que no sea espacio/tab/newline
    
    if (start == std::string::npos) return ""; // Si todo el string es whitespace, retornar vacío
    return output.substr(start, end - start + 1); // Extraer substring desde start hasta end (inclusive)
}

// Escapar caracteres especiales para shell
std::string escapeShellArg(const std::string& arg) {
    std::string escaped;
    for (char c : arg) {
        // Si es un carácter peligroso, agregar backslash antes
        if (c == '\'' || c == '\\' || c == '"' || c == '$' || c == '`') {
            escaped += '\\';
        }
        escaped += c;
    }
    return escaped;
}

// Compila código C++ a ejecutable
std::string MotorEval::compileCpp(const std::string& code) {
    // Validación de tamaño
    if (code.size() > 50000) { // 50KB máximo
        return "Error: Código demasiado largo (máximo 50KB)";
    }
    
    // Construir paths de archivos temporales
    std::string codeFile = tempDir + "/code.cpp"; // Archivo fuente
    std::string execFile = tempDir + "/code"; // Ejecutable
    
    // PASO 1: Escribir código a archivo temporal
    std::ofstream file(codeFile);
    if (!file.is_open()) {
        return "Error: No se pudo crear archivo temporal";
    }
    file << code;
    file.close();
    
    // PASO 2: Construir comando de compilación
    // 2>&1 redirige stderr a stdout para capturar errores
    std::string compileCmd = "g++ -std=c++17 -Wall -Wextra -O2 -o " + 
                            execFile + " " + codeFile + " 2>&1";

    // PASO 3: Ejecutar compilador                        
    FILE* pipe = popen(compileCmd.c_str(), "r");
    
    // PASO 4: Capturar output del compilador
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
    
    // PASO 5: Retornar resultado
    // Si error está vacío, la compilación fue exitosa
    // Si error tiene contenido, hubo un problema
    if (!error.empty()) {
        return error;
    }
    
    return ""; // Éxito (string vacío)
}

// Ejecuta programa compilado con una entrada específica
std::string MotorEval::executeProgram(const std::string& execPath, 
                                      const std::string& input) {
    // Validar que el ejecutable existe
    struct stat buffer;
    if (stat(execPath.c_str(), &buffer) != 0) {
        return "Error: El ejecutable no existe: " + execPath;
    }
    
    // Validar permisos de ejecución
    // S_IXUSR es el bit de permiso de ejecución para el usuario
    if ((buffer.st_mode & S_IXUSR) == 0) {
        return "Error: El ejecutable no tiene permisos de ejecución";
    }
    
    std::string output;
    std::string inputFile = tempDir + "/input.txt";
    std::string outputFile = tempDir + "/output.txt";
    
    // PASO 1: Escribir input a archivo temporal
    // Esto es más seguro que usar echo con pipes
    std::ofstream inFile(inputFile);
    inFile << input;
    inFile.close();
    
    // PASO 2: Construir comando con timeout
    std::string cmd = "timeout " + std::to_string(timeoutSeconds) + 
                     "s " + execPath + " < " + inputFile + 
                     " > " + outputFile + " 2>&1";
    
    // PASO 3: Ejecutar comando                 
    int exitCode = system(cmd.c_str());
    
    // PASO 4: Verificar si hubo timeout
    // El comando timeout retorna exit code 124 cuando se excede el tiempo
    if (WEXITSTATUS(exitCode) == 124) {
        return "TIMEOUT: El programa excedió el límite de " + 
               std::to_string(timeoutSeconds) + " segundos";
    }
    
    // PASO 5: Leer output del archivo
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
    
    // PASO 6: Limpiar archivos temporales
    remove(inputFile.c_str());
    remove(outputFile.c_str());
    
    return output;
}

// Ejecuta comando del sistema con timeout
int MotorEval::executeWithTimeout(const std::string& command, 
                                  std::string& output) {
    // Agregar timeout al comando                                
    std::string cmd = "timeout " + std::to_string(timeoutSeconds) + "s " + command;

    // Abrir pipe para leer output
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return -1;
    
    // Leer output del comando
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    // Cerrar pipe y obtener exit status
    int status = pclose(pipe);
    return WEXITSTATUS(status);
}

// Evalúa código del usuario contra casos de prueba
EvaluationResult MotorEval::evaluate(const std::string& userCode, 
                                     const std::vector<TestCase>& testCases) {
    // INICIALIZACIÓN: Crear estructura de resultado                                    
    EvaluationResult result;
    result.testsPassed = 0;
    result.testsFailed = 0;
    result.success = false;
    result.executionTime = 0.0;
    
    // MÉTRICA: Comenzar a medir tiempo total
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // PASO 1: COMPILACIÓN
    std::string compileError = compileCpp(userCode);
    if (!compileError.empty()) {
        result.compilationError = compileError;
        result.testsFailed = testCases.size(); // Todos los tests fallan
        return result;
    }
    
    // PASO 2: PREPARAR EJECUCIÓN
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
    
    // PASO 3: EJECUTAR CADA TEST CASE
    for (const auto& testCase : testCases) {
        // Ejecutar programa con el input de este test case
        std::string output = executeProgram(execPath, testCase.input);
        
        // Verificar si hubo timeout
        if (output.find("TIMEOUT:") == 0) {
            result.runtimeError = output;
            result.testsFailed = testCases.size();
            break; // Detener ejecución si hay timeout
        }
        
        // Limpiar whitespace de output real y esperado
        output = trimOutput(output);
        std::string expected = trimOutput(testCase.expectedOutput);
        
        // Guardar output (para mostrar al usuario)
        result.testOutputs.push_back(output);
        
        // Comparar output con expected
        bool passed = (output == expected);
        result.testResults.push_back(passed);
        
        // Actualizar contadores
        if (passed) {
            result.testsPassed++;
        } else {
            result.testsFailed++;
        }
    }
    
    // PASO 4: CALCULAR TIEMPO TOTAL
    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTime = std::chrono::duration<double>(endTime - startTime).count();
    
    // PASO 5: DETERMINAR ÉXITO GENERAL
    result.success = (result.testsFailed == 0 && result.runtimeError.empty());
    
    return result;
}