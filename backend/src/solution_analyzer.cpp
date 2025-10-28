#include "solution_analyzer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <unistd.h>

SolutionAnalyzer::SolutionAnalyzer(const std::string& apiKey) 
    : openAIKey(apiKey) {
}

int SolutionAnalyzer::countNestedLoops(const std::string& code) {
    int maxNesting = 0;
    int currentNesting = 0;
    
    // Buscar for, while, do-while
    std::regex loopRegex(R"(\b(for|while)\s*\()");
    
    std::istringstream iss(code);
    std::string line;
    
    while (std::getline(iss, line)) {
        // Contar aperturas de loops
        auto words_begin = std::sregex_iterator(line.begin(), line.end(), loopRegex);
        auto words_end = std::sregex_iterator();
        currentNesting += std::distance(words_begin, words_end);
        
        // Contar llaves de cierre
        for (char c : line) {
            if (c == '{') currentNesting++;
            if (c == '}') currentNesting--;
        }
        
        maxNesting = std::max(maxNesting, currentNesting);
    }
    
    return maxNesting;
}

bool SolutionAnalyzer::containsRecursion(const std::string& code) {
    // Buscar patrones de recursión (función que se llama a sí misma)
    std::regex funcRegex(R"(\w+\s+(\w+)\s*\([^)]*\)\s*\{)");
    std::smatch match;
    
    std::string codeStr = code;
    std::vector<std::string> functionNames;
    
    // Extraer nombres de funciones
    while (std::regex_search(codeStr, match, funcRegex)) {
        functionNames.push_back(match[1]);
        codeStr = match.suffix();
    }
    
    // Buscar si alguna función se llama a sí misma
    for (const auto& funcName : functionNames) {
        std::regex callRegex(funcName + R"(\s*\()");
        if (std::regex_search(code, callRegex)) {
            return true;
        }
    }
    
    return false;
}

bool SolutionAnalyzer::containsBinarySearch(const std::string& code) {
    // Patrones típicos de búsqueda binaria
    return (code.find("left") != std::string::npos && 
            code.find("right") != std::string::npos &&
            code.find("mid") != std::string::npos) ||
           code.find("binary_search") != std::string::npos ||
           code.find("lower_bound") != std::string::npos;
}

bool SolutionAnalyzer::containsSorting(const std::string& code) {
    return code.find("sort") != std::string::npos ||
           code.find("std::sort") != std::string::npos ||
           code.find("qsort") != std::string::npos;
}

std::vector<std::string> SolutionAnalyzer::detectDataStructures(const std::string& code) {
    std::vector<std::string> dataStructures;
    
    // Detectar estructuras comunes
    if (code.find("vector") != std::string::npos) 
        dataStructures.push_back("Vector/Array Dinámico");
    
    if (code.find("map") != std::string::npos || 
        code.find("unordered_map") != std::string::npos)
        dataStructures.push_back("Hash Map");
    
    if (code.find("set") != std::string::npos || 
        code.find("unordered_set") != std::string::npos)
        dataStructures.push_back("Set");
    
    if (code.find("stack") != std::string::npos)
        dataStructures.push_back("Stack");
    
    if (code.find("queue") != std::string::npos)
        dataStructures.push_back("Queue");
    
    if (code.find("priority_queue") != std::string::npos)
        dataStructures.push_back("Priority Queue/Heap");
    
    if (code.find("deque") != std::string::npos)
        dataStructures.push_back("Deque");
    
    if (code.find("list") != std::string::npos)
        dataStructures.push_back("Lista Enlazada");
    
    // Detectar arrays estáticos
    std::regex arrayRegex(R"(\w+\s+\w+\s*\[\s*\d+\s*\])");
    if (std::regex_search(code, arrayRegex))
        dataStructures.push_back("Array Estático");
    
    return dataStructures;
}

std::vector<AlgorithmPattern> SolutionAnalyzer::detectPatterns(const std::string& code) {
    std::vector<AlgorithmPattern> patterns;
    
    // Two Pointers
    if ((code.find("left") != std::string::npos && code.find("right") != std::string::npos) ||
        (code.find("i") != std::string::npos && code.find("j") != std::string::npos)) {
        patterns.push_back({
            "Two Pointers",
            "medium",
            "Usa dos punteros para recorrer la estructura"
        });
    }
    
    // Sliding Window
    if (code.find("window") != std::string::npos ||
        (code.find("left") != std::string::npos && 
         code.find("right") != std::string::npos &&
         code.find("while") != std::string::npos)) {
        patterns.push_back({
            "Sliding Window",
            "medium",
            "Ventana deslizante para optimizar búsquedas"
        });
    }
    
    // Binary Search
    if (containsBinarySearch(code)) {
        patterns.push_back({
            "Binary Search",
            "high",
            "Búsqueda binaria en espacio ordenado"
        });
    }
    
    // Recursión/Divide and Conquer
    if (containsRecursion(code)) {
        patterns.push_back({
            "Recursión/Divide and Conquer",
            "high",
            "Solución recursiva dividiendo el problema"
        });
    }
    
    // Dynamic Programming (heurística básica)
    if (code.find("dp") != std::string::npos || 
        code.find("memo") != std::string::npos ||
        (code.find("vector<vector<") != std::string::npos && containsRecursion(code))) {
        patterns.push_back({
            "Dynamic Programming",
            "medium",
            "Programación dinámica con memoización"
        });
    }
    
    // Greedy
    if (containsSorting(code) && code.find("for") != std::string::npos) {
        patterns.push_back({
            "Greedy Algorithm",
            "low",
            "Posible enfoque greedy (ordenar + iterar)"
        });
    }
    
    // Backtracking
    if (containsRecursion(code) && 
        (code.find("push_back") != std::string::npos && code.find("pop_back") != std::string::npos)) {
        patterns.push_back({
            "Backtracking",
            "medium",
            "Exploración exhaustiva con retroceso"
        });
    }
    
    return patterns;
}

ComplexityAnalysis SolutionAnalyzer::estimateComplexity(const std::string& code) {
    ComplexityAnalysis analysis;
    
    int nestedLoops = countNestedLoops(code);
    bool hasRecursion = containsRecursion(code);
    bool hasBinarySearch = containsBinarySearch(code);
    bool hasSorting = containsSorting(code);
    
    // Estimar complejidad temporal
    if (hasSorting) {
        analysis.timeComplexity = "O(n log n)";
        analysis.explanation = "Usa ordenamiento (típicamente O(n log n))";
        analysis.confidence = "high";
    } else if (hasBinarySearch) {
        analysis.timeComplexity = "O(log n)";
        analysis.explanation = "Búsqueda binaria sobre espacio ordenado";
        analysis.confidence = "high";
    } else if (nestedLoops >= 3) {
        analysis.timeComplexity = "O(n³) o mayor";
        analysis.explanation = "Tiene 3 o más loops anidados";
        analysis.confidence = "high";
    } else if (nestedLoops == 2) {
        analysis.timeComplexity = "O(n²)";
        analysis.explanation = "Tiene 2 loops anidados";
        analysis.confidence = "high";
    } else if (nestedLoops == 1) {
        analysis.timeComplexity = "O(n)";
        analysis.explanation = "Recorre la entrada linealmente";
        analysis.confidence = "high";
    } else if (hasRecursion) {
        analysis.timeComplexity = "O(2ⁿ) o O(n!)";
        analysis.explanation = "Recursión sin memoización (exponencial posible)";
        analysis.confidence = "medium";
    } else {
        analysis.timeComplexity = "O(1)";
        analysis.explanation = "Operaciones constantes";
        analysis.confidence = "medium";
    }
    
    // Estimar complejidad espacial
    std::vector<std::string> ds = detectDataStructures(code);
    
    if (ds.size() > 0) {
        bool hasDynamicStructure = false;
        for (const auto& structure : ds) {
            if (structure.find("Vector") != std::string::npos ||
                structure.find("Map") != std::string::npos ||
                structure.find("Set") != std::string::npos) {
                hasDynamicStructure = true;
                break;
            }
        }
        
        if (hasDynamicStructure) {
            analysis.spaceComplexity = "O(n)";
        } else {
            analysis.spaceComplexity = "O(1)";
        }
    } else {
        analysis.spaceComplexity = "O(1)";
    }
    
    return analysis;
}

std::string SolutionAnalyzer::makeLLMRequest(const std::string& prompt) {
    // Similar a OpenAIClient pero enfocado en análisis
    std::string requestFile = "/tmp/analyzer_request_" + std::to_string(getpid()) + ".json";
    
    json requestBody = {
        {"model", "gpt-3.5-turbo"},
        {"messages", json::array({
            {
                {"role", "system"},
                {"content", "Eres un experto en análisis de algoritmos y complejidad computacional."}
            },
            {
                {"role", "user"},
                {"content", prompt}
            }
        })},
        {"temperature", 0.3},
        {"max_tokens", 300}
    };
    
    std::ofstream file(requestFile);
    file << requestBody.dump();
    file.close();
    
    std::string curlCmd = "curl --max-time 10 -s https://api.openai.com/v1/chat/completions "
        "-H 'Content-Type: application/json' "
        "-H 'Authorization: Bearer " + openAIKey + "' "
        "-d @" + requestFile + " 2>&1";
    
    FILE* pipe = popen(curlCmd.c_str(), "r");
    if (!pipe) {
        remove(requestFile.c_str());
        return "";
    }
    
    std::string response;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        response += buffer;
    }
    pclose(pipe);
    
    remove(requestFile.c_str());
    
    try {
        auto responseJson = json::parse(response);
        if (responseJson.contains("choices") && responseJson["choices"].size() > 0) {
            return responseJson["choices"][0]["message"]["content"];
        }
    } catch (...) {
        return "";
    }
    
    return "";
}

CodeAnalysis SolutionAnalyzer::analyze(const std::string& code,
                                       const std::string& problemDescription,
                                       int testsPassed,
                                       int testsFailed) {
    CodeAnalysis analysis;
    
    // 1. Análisis estático
    analysis.complexity = estimateComplexity(code);
    analysis.dataStructures = detectDataStructures(code);
    analysis.patterns = detectPatterns(code);
    
    // 2. Análisis con LLM (opcional, más preciso)
    if (!openAIKey.empty()) {
        std::string truncatedCode = code.substr(0, std::min<size_t>(800, code.size()));
        
        std::string prompt = 
            "Analiza este código C++ y proporciona:\n"
            "1. Complejidad temporal (Big O)\n"
            "2. Complejidad espacial\n"
            "3. Patrón algorítmico usado\n"
            "4. Una sugerencia de optimización (si aplica)\n\n"
            "Problema: " + problemDescription + "\n\n"
            "Código:\n```cpp\n" + truncatedCode + "\n```\n\n"
            "Tests pasados: " + std::to_string(testsPassed) + "\n"
            "Tests fallidos: " + std::to_string(testsFailed) + "\n\n"
            "Responde en formato conciso (máximo 5 líneas).";
        
        std::string llmResponse = makeLLMRequest(prompt);
        
        if (!llmResponse.empty()) {
            // El LLM puede proporcionar análisis más refinado
            // Por ahora usamos el estático + guardamos respuesta LLM como sugerencia
            analysis.suggestions.push_back(llmResponse);
        }
    }
    
    // 3. Sugerencias basadas en análisis estático
    if (analysis.complexity.timeComplexity.find("n²") != std::string::npos ||
        analysis.complexity.timeComplexity.find("n³") != std::string::npos) {
        analysis.suggestions.push_back(
            "💡 La complejidad es cuadrática o mayor. Considera usar estructuras como "
            "hash maps o algoritmos más eficientes."
        );
    }
    
    if (analysis.dataStructures.empty()) {
        analysis.suggestions.push_back(
            "💡 No usas estructuras de datos auxiliares. Considera si un hash map "
            "o vector podría optimizar tu solución."
        );
    }
    
    if (testsFailed > 0 && analysis.patterns.empty()) {
        analysis.suggestions.push_back(
            "💡 No se detectó un patrón algorítmico claro. Revisa si el problema "
            "requiere two pointers, sliding window, o búsqueda binaria."
        );
    }
    
    return analysis;
}