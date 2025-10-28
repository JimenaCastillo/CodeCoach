#ifndef SOLUTION_ANALYZER_H
#define SOLUTION_ANALYZER_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct ComplexityAnalysis {
    std::string timeComplexity;      // "O(n)", "O(n^2)", "O(log n)", etc.
    std::string spaceComplexity;     // "O(1)", "O(n)", etc.
    std::string confidence;          // "high", "medium", "low"
    std::string explanation;         // Explicación de por qué
};

struct AlgorithmPattern {
    std::string patternName;         // "Two Pointers", "Sliding Window", etc.
    std::string confidence;          // "high", "medium", "low"
    std::string description;         // Descripción breve
};

struct CodeAnalysis {
    ComplexityAnalysis complexity;
    std::vector<AlgorithmPattern> patterns;
    std::vector<std::string> dataStructures;  // "array", "hashmap", "stack", etc.
    std::vector<std::string> suggestions;     // Sugerencias de optimización
};

class SolutionAnalyzer {
private:
    std::string openAIKey;
    
public:
    SolutionAnalyzer(const std::string& apiKey);
    
    // Análisis principal
    CodeAnalysis analyze(const std::string& code, 
                        const std::string& problemDescription,
                        int testsPassed,
                        int testsFailed);
    
    // Análisis estático (sin LLM)
    ComplexityAnalysis estimateComplexity(const std::string& code);
    std::vector<std::string> detectDataStructures(const std::string& code);
    std::vector<AlgorithmPattern> detectPatterns(const std::string& code);
    
private:
    // Helpers
    std::string makeLLMRequest(const std::string& prompt);
    int countNestedLoops(const std::string& code);
    bool containsRecursion(const std::string& code);
    bool containsBinarySearch(const std::string& code);
    bool containsSorting(const std::string& code);
};

#endif