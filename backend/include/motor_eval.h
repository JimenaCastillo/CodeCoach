#ifndef MOTOR_EVAL_H
#define MOTOR_EVAL_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;


// Representa un caso de prueba
struct TestCase {
    std::string input;           // Entrada que se pasa al programa
    std::string expectedOutput;  // Salida esperada
};

// Resultado completo de la evaluación de código
struct EvaluationResult {
    bool success;                           // true si todos los tests pasaron
    int testsPassed;                        // Número de tests exitosos
    int testsFailed;                        // Número de tests fallidos
    std::string compilationError;           // Error de compilación (si existe)
    std::string runtimeError;               // Error en tiempo de ejecución (si existe)
    double executionTime;                   // Tiempo total de ejecución en segundos
    std::vector<std::string> testOutputs;   // Output de cada test
    std::vector<bool> testResults;          // true/false para cada test  
};

// Motor de evaluación de código C++
class MotorEval {
private:
    std::string tempDir; // Directorio temporal para compilación/ejecución
    int timeoutSeconds;  // Límite de tiempo por test case
    
public:
    MotorEval(int timeout = 5); // Constructor del motor de evaluación
    ~MotorEval(); // Destructor - limpia archivos temporales
    
    EvaluationResult evaluate(const std::string& userCode, 
                             const std::vector<TestCase>& testCases); // Evalúa código del usuario contra casos de prueba
    
private:
    std::string compileCpp(const std::string& code);        // Compila código C++ a ejecutable
    std::string executeProgram(const std::string& execPath, 
                              const std::string& input);    // Ejecuta programa compilado con entrada específica
    std::string trimOutput(const std::string& output);      // Remueve espacios en blanco al inicio y final
    int executeWithTimeout(const std::string& command, 
                          std::string& output);             // Ejecuta comando con timeout
};

#endif