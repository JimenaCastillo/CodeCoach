#ifndef SOLUTION_ANALYZER_H
#define SOLUTION_ANALYZER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Estados posibles del análisis de código
enum class AnalysisStatus {
    OK,         // Análisis exitoso del código del usuario
    NoUserCode  // Solo se detectó código de plantilla/harness
};

//Contiene el análisis de complejidad algorítmica
struct ComplexityAnalysis {
    std::string timeComplexity;     // Ej: "O(n)", "O(n²)", "O(log n)"
    std::string spaceComplexity;    // Ej: "O(1)", "O(n)"
    std::string confidence;         // Ej: "high", "medium", "low"
    std::string explanation;        // Explicación legible del análisis
};

// Representa un patrón algorítmico detectado
struct AlgorithmPattern {
    std::string patternName;        // Nombre del patrón detectado
    std::string confidence;         // Nivel de confianza en la detección
    std::string description;        // Descripción de por qué se detectó
};

// Señales detectadas dentro de un loop
struct LoopSignals {
    int depth = 0;                              // Profundidad de anidamiento
    bool hasMultiplicativeUpdate = false;       // Tiene operaciones *= o /=
    bool hasModulo = false;                     // Usa operador módulo %
    bool hasDivision = false;                   // Tiene divisiones
    bool hasPower = false;                      // Usa pow() o exponenciación
    bool isLogarithmic = false;                 // Actualización logarítmica (i/=2)
    bool hasTwoPointers = false;                // Variables left/right o slow/fast
    bool hasOppositePointerMovement = false;    // Punteros se mueven en direcciones opuestas
    bool hasPointerStopConditions = false;      // Condición de parada típica de two pointers
    bool hasBinarySearchPattern = false;        // Patrón left/mid/right de búsqueda binaria
    bool hasSorting = false;                    // Llama a sort() dentro del loop
    bool hasMemoization = false;                // Usa estructuras de memoización
    bool hasHashStructure = false;              // Usa hash map/set
};

// Puntaje interno para determinar patrones detectados
struct PatternScore {
    std::string name;
    std::string confidence;
    std::string description;
    double score = 0.0;
};

// Puntaje interno para determinar complejidad
struct ComplexityScore {
    std::string timeComplexity;
    std::string spaceComplexity;
    std::string confidence;
    std::string explanation;
    double score = 0.0;
};

// Contexto completo del análisis de código
struct AnalysisContext {
    AnalysisStatus status = AnalysisStatus::NoUserCode;     // Estado del análisis
    std::string rawCode;                                    // Código original extraído
    std::string cleanedCode;                                // Código sin comentarios/strings
    std::vector<LoopSignals> loopSignals;                   // Señales de todos los loops   
    std::unordered_map<std::string, double> patternScores;  // Puntajes de patrones
};

// Resultado completo del análisis de una solución
struct CodeAnalysis {
    std::string status;                      // "OK" o "NoUserCode"
    ComplexityAnalysis complexity;           // Análisis de complejidad
    std::vector<AlgorithmPattern> patterns;  // Patrones detectados
    std::vector<std::string> dataStructures; // Estructuras de datos usadas
    std::vector<std::string> suggestions;    // Sugerencias de optimización
};

//Analizador principal de soluciones de código
class SolutionAnalyzer {
private:
    std::string openAIKey;                                          // API Key para OpenAI
    std::unordered_map<std::string, std::string> cleanCache;        // Cache de código limpio
    AnalysisStatus lastSnippetStatus = AnalysisStatus::NoUserCode;  // Estado del último snippet

public:
    SolutionAnalyzer(const std::string& apiKey); // Constructor del analizador
    
    // Analiza una solución completa
    CodeAnalysis analyze(const std::string& code, 
                        const std::string& problemDescription,
                        int testsPassed,
                        int testsFailed);

    ComplexityAnalysis estimateComplexity(const std::string& code);         // Estima la complejidad algorítmica del código
    std::vector<std::string> detectDataStructures(const std::string& code); // Detecta estructuras de datos utilizadas
    std::vector<AlgorithmPattern> detectPatterns(const std::string& code);  // Detecta patrones algorítmicos

private:
    // MÉTODOS DE LIMPIEZA DE CÓDIGO     
    std::string cleanCode(const std::string& code);           // Limpia el código removiendo comentarios y neutralizando strings
    const std::string& getCleaned(const std::string& code);   // Obtiene versión limpia con caché
    std::string extractRelevantCode(const std::string& code); // Extrae código relevante del usuario

    // MÉTODOS DE DETECCIÓN DE CARACTERÍSTICAS
    bool detectLogarithmicUpdate(const std::string& line) const;            // Detecta si un loop tiene actualización logarítmica
    bool detectOppositePointerMovement(const std::string& loopBody) const;  // Detecta movimiento de punteros en direcciones opuestas
    bool detectPointerStopCondition(const std::string& loopHeader) const;   // Detecta condiciones típicas de two pointers
    bool detectMultiplicativeUpdate(const std::string& loopBody) const;     // Detecta operaciones multiplicativas en el loop
    bool detectModuloUsage(const std::string& loopBody) const;              // Detecta uso del operador módulo
    bool detectPowerOperation(const std::string& loopBody) const;           // Detecta operaciones de potencia
    bool detectBinarySearchPattern(const std::string& loopBody) const;      // Detecta patrón de búsqueda binaria
    bool detectSortingInLoop(const std::string& loopBody) const;            // Detecta llamadas a sort() dentro del loop
    bool detectHashStructure(const std::string& loopBody) const;            // Detecta uso de estructuras hash
    bool detectMemoization(const std::string& code) const;                  // Detecta uso de memoización
    bool hasTwoPointerVariables(const std::string& loopBody) const;         // Detecta variables de two pointers

    // MÉTODOS DE ANÁLISIS DE LOOPS
    LoopSignals analyzeLoop(const std::string& header,
                            const std::string& body,
                            int depth) const;           // Analiza un loop individual
    void collectLoopSignals(AnalysisContext& context);  // Recolecta señales de todos los loops del código
    int countNestedLoops(AnalysisContext& context);     // Cuenta el nivel máximo de anidamiento de loops

    // MÉTODOS DE DETECCIÓN DE PATRONES
    bool containsRecursion(const std::string& cleanedCode);     // Detecta recursión en el código
    bool containsBinarySearch(const std::string& cleanedCode);  // Detecta uso de búsqueda binaria
    bool containsSorting(const std::string& cleanedCode);       // Detecta uso de sorting

    // MÉTODOS DE ANÁLISIS AVANZADO
    std::vector<std::string> detectDataStructuresFromClean(const std::string& cleanedCode); // Detecta estructuras de datos desde código limpio
    std::vector<AlgorithmPattern> detectPatternsFromClean(const std::string& cleanedCode);  // Detecta patrones desde código limpio
    std::vector<AlgorithmPattern> detectPatterns(AnalysisContext& context);                 // Detecta patrones usando contexto completo
    ComplexityAnalysis estimateComplexityFromClean(const std::string& cleanedCode);         // Estima complejidad desde código limpio
    ComplexityAnalysis estimateComplexityFromContext(AnalysisContext& context);             // Estima complejidad usando contexto completo

    // MÉTODOS DE AJUSTE DE CONFIANZA
    void adjustPatternConfidence(std::vector<AlgorithmPattern>& patterns,
                                 const AnalysisContext& context) const;     // Ajusta la confianza de los patrones detectados
    void adjustComplexityConfidence(ComplexityAnalysis& analysis,
                                    const AnalysisContext& context) const;  // Ajusta la confianza del análisis de complejidad
    
    // INTEGRACIÓN CON LLM
    std::string makeLLMRequest(const std::string& prompt);  // Hace una petición al LLM de OpenAI
};

#endif
