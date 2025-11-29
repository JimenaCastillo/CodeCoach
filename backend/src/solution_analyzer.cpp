#include "solution_analyzer.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <set>
#include <vector>
#include <unistd.h>
#include <cctype>
#include <cstdio>
#include <unordered_map>

namespace {

// Retorna lista de tokens de IO
const std::vector<std::string>& ioTokens() {
    static const std::vector<std::string> tokens = {
        "cin", "cout", "scanf", "printf", "getline", "puts", "putchar",
        "getchar", "fscanf", "fprintf", "read", "write", "ios::sync_with_stdio",
        "tie", "endl", "flush", "istream", "ostream", "fastio"
    };
    return tokens;
}

// Retorna keywords para ignorar funciones helper
const std::vector<std::string>& ignoreKeywords() {
    static const std::vector<std::string> keywords = {
        "parse", "reader", "writer", "read", "print", "input", "output", "write",
        "scan", "display", "log", "debug", "format", "helper", "util", "token",
        "split", "join", "validator", "validate", "check", "ensure", "load",
        "save", "driver", "runner", "harness", "sanitize", "bracket", "comma",
        "mock", "stub", "adapter", "manager", "service", "controller", "io",
        "fast", "stream", "buffer", "serializer"
    };
    return keywords;
}

// Remueve whitespace de un snippet
std::string trimSnippet(const std::string& text) {
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

// Verifica si una posición es límite de palabra
inline bool isWordBoundary(const std::string& text, size_t pos) {
    return pos >= text.size() || (!(std::isalnum(static_cast<unsigned char>(text[pos])) || text[pos] == '_'));
}

// Verifica si una palabra comienza en cierta posición
bool startsWithWord(const std::string& text, size_t pos, const std::string& word) {
    if (pos + word.size() > text.size()) {
        return false;
    }
    if (text.compare(pos, word.size(), word) != 0) {
        return false;
    }
    bool leftOK = (pos == 0) || isWordBoundary(text, pos - 1);
    bool rightOK = isWordBoundary(text, pos + word.size());
    return leftOK && rightOK;
}

// Salta whitespace desde una posición
size_t skipWhitespace(const std::string& text, size_t pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
        ++pos;
    }
    return pos;
}

// Encuentra el carácter de cierre correspondiente
size_t findMatching(const std::string& text, size_t start, char openChar, char closeChar) {
    if (start >= text.size() || text[start] != openChar) {
        return std::string::npos;
    }
    int depth = 1;
    for (size_t i = start + 1; i < text.size(); ++i) {
        char c = text[i];
        if (c == openChar) {
            ++depth;
        } else if (c == closeChar) {
            --depth;
            if (depth == 0) {
                return i;
            }
        }
    }
    return std::string::npos;
}

// Encuentra el final de una declaración (hasta el ';')
size_t findStatementEnd(const std::string& text, size_t start) {
    int paren = 0;
    int brace = 0;
    int bracket = 0;
    for (size_t i = start; i < text.size(); ++i) {
        char c = text[i];
        if (c == ';' && paren == 0 && brace == 0 && bracket == 0) {
            return i + 1;
        }
        if (c == '(') ++paren;
        else if (c == ')') --paren;
        else if (c == '{') ++brace;
        else if (c == '}') --brace;
        else if (c == '[') ++bracket;
        else if (c == ']') --bracket;
    }
    return text.size();
}

} // namespace

// Constructor del analizador
SolutionAnalyzer::SolutionAnalyzer(const std::string& apiKey) 
    : openAIKey(apiKey) {
}

// ========== HELPER PRIVADO: Limpiar código de comentarios y strings ==========

// Limpia código removiendo comentarios y neutralizando strings
std::string SolutionAnalyzer::cleanCode(const std::string& code) {
    std::string cleaned;
    cleaned.reserve(code.size());
    
    // Estado de la máquina
    bool inString = false;
    bool inLineComment = false;
    bool inBlockComment = false;
    bool escape = false;
    char stringChar = '\0';
    
    // Procesar carácter por carácter
    for (size_t i = 0; i < code.size(); ++i) {
        char c = code[i];
        char next = (i + 1 < code.size()) ? code[i + 1] : '\0';
        
        // ESTADO 1: Comentario de línea
        if (inLineComment) {
            cleaned += (c == '\n') ? '\n' : ' ';
            if (c == '\n') {
                inLineComment = false;
            }
            continue;
        }
        
        // ESTADO 2: Comentario de bloque
        if (inBlockComment) {
            cleaned += (c == '\n') ? '\n' : ' ';
            if (c == '*' && next == '/') {
                cleaned.back() = ' ';
                cleaned += ' ';
                ++i;
                inBlockComment = false;
            }
            continue;
        }
        
        // DETECCIÓN: Inicio de comentario de línea
        if (!inString && c == '/' && next == '/') {
            cleaned += ' ';
            cleaned += ' ';
            ++i;
            inLineComment = true;
            continue;
        }
        
        // DETECCIÓN: Inicio de comentario de bloque
        if (!inString && c == '/' && next == '*') {
            cleaned += ' ';
            cleaned += ' ';
            ++i;
            inBlockComment = true;
            continue;
        }
        
        // Agregar carácter actual
        cleaned += c;
        
        // MANEJO DE STRINGS
        if ((c == '"' || c == '\'') && !escape) {
            if (inString && c == stringChar) {
                inString = false;
            } else if (!inString) {
                inString = true;
                stringChar = c;
            }
        }
        
        // MANEJO DE ESCAPE
        if (inString && c == '\\' && !escape) {
            escape = true;
        } else {
            escape = false;
        }
    }
    
    return cleaned;
}

// Obtiene versión limpia con caché
const std::string& SolutionAnalyzer::getCleaned(const std::string& code) {
    auto it = cleanCache.find(code);
    if (it != cleanCache.end()) {
        return it->second;
    }
    
    auto inserted = cleanCache.emplace(code, std::string{});
    inserted.first->second = cleanCode(code);
    return inserted.first->second;
}

// Detecta actualización logarítmica en un loop
bool SolutionAnalyzer::detectLogarithmicUpdate(const std::string& text) const {
    static const std::regex divideAssign(R"(\b\w+\s*/=\s*(2|\w+\s*>>\s*1))");
    static const std::regex halveAssign(R"(\b\w+\s*=\s*\w+\s*/\s*2)");
    static const std::regex shiftAssign(R"(\b\w+\s*>>=\s*1)" );

    if (std::regex_search(text, divideAssign) ||
        std::regex_search(text, halveAssign) ||
        std::regex_search(text, shiftAssign)) {
        return true;
    }

    // Patrón de binary search: mid se asigna a left/right
    if (text.find("mid") != std::string::npos &&
        (text.find("left = mid") != std::string::npos ||
         text.find("right = mid") != std::string::npos)) {
        return true;
    }
    return false;
}

// Detecta movimiento de punteros en direcciones opuestas
bool SolutionAnalyzer::detectOppositePointerMovement(const std::string& loopBody) const {
    // Helper: Verifica si string contiene alguno de los tokens
    auto containsAny = [](const std::string& haystack, const std::vector<std::string>& needles) {
        for (const auto& token : needles) {
            if (haystack.find(token) != std::string::npos) {
                return true;
            }
        }
        return false;
    };

    // Patrón 1: left++ y right--
    bool incLeft = containsAny(loopBody, {"left++", "++left", "left += 1", "left +=1", "left = left + 1"});
    bool decRight = containsAny(loopBody, {"right--", "--right", "right -= 1", "right -=1", "right = right - 1"});

    // Patrón 2: slow++ y fast+=2
    bool incSlow = containsAny(loopBody, {"slow++", "++slow", "slow += 1"});
    bool incFastTwice = containsAny(loopBody, {"fast += 2", "fast +=2", "fast = fast + 2", "fast++", "++fast"});
    return (incLeft && decRight) || (incSlow && incFastTwice);
}


// Detecta condiciones típicas de two pointers
bool SolutionAnalyzer::detectPointerStopCondition(const std::string& loopHeader) const {
    static const std::vector<std::string> conditions = {
        "left < right", "left <= right", "right > left", "right >= left",
        "slow != fast", "fast != slow", "slow < fast", "fast < slow"
    };
    for (const auto& cond : conditions) {
        if (loopHeader.find(cond) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// Detecta operaciones multiplicativas en el loop
bool SolutionAnalyzer::detectMultiplicativeUpdate(const std::string& loopBody) const {
    static const std::regex multAssign(R"(\b\w+\s*=\s*\w+\s*[*\/])");
    static const std::regex mulPattern(R"(\b\w+\s*[*]\s*\w+)");
    static const std::regex divAssign(R"([/]=)" );
    return loopBody.find("*=") != std::string::npos ||
           loopBody.find("/=") != std::string::npos ||
           std::regex_search(loopBody, multAssign) ||
           std::regex_search(loopBody, mulPattern) ||
           std::regex_search(loopBody, divAssign);
}

// Detecta uso del operador módulo
bool SolutionAnalyzer::detectModuloUsage(const std::string& loopBody) const {
    return loopBody.find('%') != std::string::npos;
}

// Detecta operaciones de potencia
bool SolutionAnalyzer::detectPowerOperation(const std::string& loopBody) const {
    return loopBody.find("pow(") != std::string::npos ||
           loopBody.find("std::pow") != std::string::npos ||
           loopBody.find("**") != std::string::npos ||
           loopBody.find("^=") != std::string::npos;
}

// Detecta patrón de búsqueda binaria
bool SolutionAnalyzer::detectBinarySearchPattern(const std::string& loopText) const {
    bool hasMid = loopText.find("mid") != std::string::npos;
    bool updatesLeft = loopText.find("left = mid") != std::string::npos ||
                       loopText.find("left = mid + 1") != std::string::npos ||
                       loopText.find("left = mid+1") != std::string::npos;
    bool updatesRight = loopText.find("right = mid") != std::string::npos ||
                        loopText.find("right = mid - 1") != std::string::npos ||
                        loopText.find("right = mid-1") != std::string::npos;
    return hasMid && updatesLeft && updatesRight;
}

// Detecta llamadas a sort() dentro del loop
bool SolutionAnalyzer::detectSortingInLoop(const std::string& loopBody) const {
    return loopBody.find("sort(") != std::string::npos ||
           loopBody.find("std::sort") != std::string::npos ||
           loopBody.find("stable_sort") != std::string::npos;
}

// Detecta uso de estructuras hash
bool SolutionAnalyzer::detectHashStructure(const std::string& loopBody) const {
    static const std::vector<std::string> tokens = {
        "unordered_map", "std::unordered_map", "unordered_set", "std::unordered_set",
        "std::map", "std::set", "map<", "set<"
    };
    for (const auto& token : tokens) {
        if (loopBody.find(token) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// Detecta uso de memoización
bool SolutionAnalyzer::detectMemoization(const std::string& code) const {
    static const std::vector<std::regex> memoPatterns = {
        std::regex(R"(\bmemo\b)"),
        std::regex(R"(\bdp\b)"),
        std::regex(R"(\bcache\b)"),
        std::regex(R"(\blookup\b)"),
        std::regex(R"(\bvisited\b)")
    };
    for (const auto& pattern : memoPatterns) {
        if (std::regex_search(code, pattern)) {
            return true;
        }
    }
    return false;
}

// Detecta variables de two pointers
bool SolutionAnalyzer::hasTwoPointerVariables(const std::string& loopBody) const {
    bool hasLeftRight = loopBody.find("left") != std::string::npos &&
                        loopBody.find("right") != std::string::npos;
    bool hasSlowFast = loopBody.find("slow") != std::string::npos &&
                       loopBody.find("fast") != std::string::npos;
    return hasLeftRight || hasSlowFast;
}

// Extrae código relevante del usuario (ignora harness)
std::string SolutionAnalyzer::extractRelevantCode(const std::string& code) {
    // Trackear estado del snippet extraído
    lastSnippetStatus = AnalysisStatus::OK;
    
    if (code.empty()) {
        lastSnippetStatus = AnalysisStatus::NoUserCode;
        return {};
    }

    std::string cleaned = cleanCode(code);
    
    // Helper: Convertir a minúsculas
    auto toLower = [](const std::string& text) {
        std::string lower;
        lower.reserve(text.size());
        for (char c : text) {
            lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return lower;
    };

    // Estructura para almacenar información de funciones
    struct FunctionBlock {
        std::string name;
        size_t signaturePos = 0;
        size_t bodyStart = 0;
        size_t bodyEnd = 0;
        bool hasIO = false;
    };

    // Helper: Verifica si un rango contiene tokens de IO
    auto rangeContainsIO = [&](size_t start, size_t end) {
        for (const auto& marker : ioTokens()) {
            size_t pos = cleaned.find(marker, start);
            while (pos != std::string::npos && pos < end) {
                bool before = (pos > 0) && std::isalnum(static_cast<unsigned char>(cleaned[pos - 1]));
                bool after = (pos + marker.size() < cleaned.size()) &&
                             std::isalnum(static_cast<unsigned char>(cleaned[pos + marker.size()]));
                if (!before && !after) {
                    return true;
                }
                pos = cleaned.find(marker, pos + marker.size());
            }
        }
        return false;
    };

    std::vector<FunctionBlock> functions;
    std::regex funcRegex(R"((?:^|\s)([A-Za-z_]\w*)\s*\([^;{}]*\)\s*(?:const)?\s*\{)");
    for (std::sregex_iterator it(cleaned.begin(), cleaned.end(), funcRegex), end;
         it != end; ++it) {
        const std::smatch& match = *it;
        std::string funcName = match[1].str();

        // Ignorar keywords de control de flujo
        if (funcName == "if" || funcName == "for" || funcName == "while" ||
            funcName == "switch" || funcName == "catch" || funcName == "else") {
            continue;
        }

        size_t signaturePos = static_cast<size_t>(match.position());
        size_t bodyStart = signaturePos + match.length();

        // Encontrar cierre de la función
        int braceDepth = 1;
        size_t pos = bodyStart;
        while (pos < cleaned.size() && braceDepth > 0) {
            if (cleaned[pos] == '{') {
                ++braceDepth;
            } else if (cleaned[pos] == '}') {
                --braceDepth;
            }
            ++pos;
        }

        functions.push_back(FunctionBlock{
            funcName,
            signaturePos,
            bodyStart,
            std::min(pos, cleaned.size()),
            rangeContainsIO(bodyStart, std::min(pos, cleaned.size()))
        });
    }

    const FunctionBlock* mainBlock = nullptr;
    for (const auto& fn : functions) {
        if (fn.name == "main") {
            mainBlock = &fn;
            break;
        }
    }

    auto advancePastMarker = [&](size_t markerPos, size_t markerLen) {
        size_t newlinePos = code.find('\n', markerPos);
        if (newlinePos == std::string::npos) {
            return std::min(code.size(), markerPos + markerLen);
        }
        return std::min(code.size(), newlinePos + 1);
    };

    struct EditableRegion {
        size_t start = 0;
        size_t end = 0;
        bool valid = false;
        
        bool isValid() const {
            return valid && end > start;
        }
    };

    auto locateEditableRegion = [&]() -> EditableRegion {
        std::string lowerCode = toLower(code);
        
        const std::vector<std::pair<std::string, std::string>> markerPairs = {
            {"// === BEGIN USER CODE ===", "// === END USER CODE ==="},
            {"// BEGIN-STUDENT-CODE", "// END-STUDENT-CODE"},
            {"// BEGIN USER CODE", "// END USER CODE"},
            {"/* BEGIN STUDENT CODE */", "/* END STUDENT CODE */"},
            {"/* BEGIN USER CODE */", "/* END USER CODE */"},
        };

        for (const auto& pair : markerPairs) {
            std::string startLower = toLower(pair.first);
            std::string endLower = toLower(pair.second);
            size_t startPos = lowerCode.find(startLower);
            if (startPos == std::string::npos) {
                continue;
            }
            size_t endPos = lowerCode.find(endLower, startPos + startLower.size());
            size_t regionStart = advancePastMarker(startPos, startLower.size());
            size_t regionEnd = (endPos == std::string::npos) ? code.size() : endPos;
            if (regionEnd > regionStart) {
                return {regionStart, regionEnd, true};
            }
        }

        const std::vector<std::string> singleMarkers = {
            "// tu código aquí",
            "// tu codigo aqui",
            "// escribe tu código aquí",
            "// escribe tu codigo aqui",
            "// write your code here",
            "// begin solution",
            "// begin student code"
        };

        for (const auto& marker : singleMarkers) {
            std::string markerLower = toLower(marker);
            size_t markerPos = lowerCode.find(markerLower);
            if (markerPos == std::string::npos) {
                continue;
            }
            size_t regionStart = advancePastMarker(markerPos, markerLower.size());
            size_t regionEnd = code.size();
            for (const auto& fn : functions) {
                if (fn.bodyStart <= regionStart && regionStart < fn.bodyEnd) {
                    regionEnd = fn.bodyEnd - 1;
                    break;
                }
            }
            if (regionEnd > regionStart) {
                return {regionStart, regionEnd, true};
            }
        }

        return {};
    };

    EditableRegion userRegion = locateEditableRegion();
    
    auto sliceRegion = [&](const EditableRegion& region) -> std::string {
        if (!region.isValid()) {
            return "";
        }
        size_t start = std::min(region.start, code.size());
        size_t end = std::min(region.end, code.size());
        if (end <= start) {
            return "";
        }
        return code.substr(start, end - start);
    };

    std::string snippet = trimSnippet(sliceRegion(userRegion));
    std::set<std::string> includedFunctions;

    auto shouldIgnoreFunction = [&](const FunctionBlock& fn) {
        std::string lower = toLower(fn.name);
        for (const auto& keyword : ignoreKeywords()) {
            if (lower.find(keyword) != std::string::npos) {
                return true;
            }
        }
        return fn.hasIO;
    };

    auto appendFunctionCode = [&](const FunctionBlock& fn) {
        includedFunctions.insert(fn.name);
        snippet.append("\n");
        snippet.append(code.substr(fn.signaturePos, fn.bodyEnd - fn.signaturePos));
    };

    if (snippet.empty()) {
        for (const auto& fn : functions) {
            if (fn.name == "main" || shouldIgnoreFunction(fn)) {
                continue;
            }
            appendFunctionCode(fn);
        }
    }

    auto sliceAfterIO = [&](const FunctionBlock& fn) -> std::string {
        size_t searchStart = fn.bodyStart;
        size_t searchEnd = fn.bodyEnd;
        size_t lastIO = std::string::npos;
        for (const auto& marker : ioTokens()) {
            size_t pos = cleaned.find(marker, searchStart);
            while (pos != std::string::npos && pos < searchEnd) {
                lastIO = std::max(lastIO, pos);
                pos = cleaned.find(marker, pos + marker.size());
            }
        }
        if (lastIO == std::string::npos) {
            return code.substr(searchStart, searchEnd - searchStart);
        }
        size_t stmtEnd = cleaned.find(';', lastIO);
        if (stmtEnd == std::string::npos || stmtEnd >= searchEnd) {
            stmtEnd = searchEnd;
        } else {
            ++stmtEnd;
        }
        size_t trimmedStart = stmtEnd;
        while (trimmedStart < searchEnd &&
               std::isspace(static_cast<unsigned char>(code[trimmedStart]))) {
            ++trimmedStart;
        }
        if (trimmedStart >= searchEnd) {
            return "";
        }
        return code.substr(trimmedStart, searchEnd - trimmedStart);
    };

    if (snippet.empty() && mainBlock) {
        // Extraer desde main ignorando harness de IO
        snippet = trimSnippet(sliceAfterIO(*mainBlock));
    }

    auto includeHelpers = [&]() {
        bool added = true;
        while (added) {
            added = false;
            for (const auto& fn : functions) {
                if (fn.name == "main" || shouldIgnoreFunction(fn)) {
                    continue;
                }
                if (includedFunctions.count(fn.name)) {
                    continue;
                }
                std::regex callRegex("\\b" + fn.name + R"(\s*\()");
                if (std::regex_search(snippet, callRegex)) {
                    appendFunctionCode(fn);
                    added = true;
                }
            }
        }
    };

    if (!snippet.empty()) {
        // Incluir funciones helper referenciadas
        includeHelpers();
    }

    snippet = trimSnippet(snippet);
    if (snippet.empty()) {
        lastSnippetStatus = AnalysisStatus::NoUserCode;
        return {};
    }

    lastSnippetStatus = AnalysisStatus::OK;
    return snippet;
}

int SolutionAnalyzer::countNestedLoops(AnalysisContext& context) {
    collectLoopSignals(context);
    int maxDepth = 0;
    for (const auto& loop : context.loopSignals) {
        maxDepth = std::max(maxDepth, loop.depth);
    }
    return maxDepth;
}

bool SolutionAnalyzer::containsRecursion(const std::string& cleanedCode) {
    std::regex funcRegex(R"((?:^|\s)(\w+)\s*\([^)]*\)\s*(?:const)?\s*\{)");
    std::sregex_iterator iter(cleanedCode.begin(), cleanedCode.end(), funcRegex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        const std::smatch& match = *iter;
        std::string funcName = match[1].str();
        
        if (funcName == "if" || funcName == "for" || funcName == "while" ||
            funcName == "switch" || funcName == "catch" || funcName == "else") {
            continue;
        }
        
        size_t bodyStart = match.position() + match.length();
        int braceDepth = 1;
        size_t pos = bodyStart;
        
        while (pos < cleanedCode.size() && braceDepth > 0) {
            if (cleanedCode[pos] == '{') {
                ++braceDepth;
            } else if (cleanedCode[pos] == '}') {
                --braceDepth;
            }
            ++pos;
        }
        
        if (bodyStart >= pos) {
            continue;
        }
        
        std::string body = cleanedCode.substr(bodyStart, pos - bodyStart - 1);
        std::regex callRegex("\\b" + funcName + R"(\s*\()");
        if (std::regex_search(body, callRegex)) {
            return true;
        }
    }
    
    return false;
}

bool SolutionAnalyzer::containsBinarySearch(const std::string& cleanedCode) {
    std::regex wordRegex(R"(\b(left|right|mid|while|binary_search|lower_bound|upper_bound)\b)");
    std::set<std::string> tokens;
    
    auto begin = std::sregex_iterator(cleanedCode.begin(), cleanedCode.end(), wordRegex);
    auto finish = std::sregex_iterator();
    
    for (auto it = begin; it != finish; ++it) {
        tokens.insert(it->str());
    }
    
    bool hasStdAlgorithms = tokens.count("binary_search") ||
                            tokens.count("lower_bound") ||
                            tokens.count("upper_bound");
    
    bool hasClassicPattern = tokens.count("left") && tokens.count("right") &&
                             tokens.count("mid") && tokens.count("while");
    
    return hasStdAlgorithms || hasClassicPattern;
}

bool SolutionAnalyzer::containsSorting(const std::string& cleanedCode) {
    return cleanedCode.find("sort") != std::string::npos ||
           cleanedCode.find("std::sort") != std::string::npos ||
           cleanedCode.find("qsort") != std::string::npos ||
           cleanedCode.find("std::stable_sort") != std::string::npos;
}

std::vector<std::string> SolutionAnalyzer::detectDataStructures(const std::string& code) {
    return detectDataStructuresFromClean(cleanCode(code));
}

std::vector<std::string> SolutionAnalyzer::detectDataStructuresFromClean(const std::string& cleanedCode) {
    std::vector<std::string> dataStructures;
    
    const std::vector<std::pair<std::string, std::string>> patterns = {
        {"vector<", "Vector/Array Dinámico"},
        {"std::vector", "Vector/Array Dinámico"},
        {"unordered_map", "Hash Map"},
        {"std::unordered_map", "Hash Map"},
        {"map<", "Árbol Map (Balanceado)"},
        {"std::map", "Árbol Map (Balanceado)"},
        {"unordered_set", "Hash Set"},
        {"std::unordered_set", "Hash Set"},
        {"set<", "Árbol Set (Balanceado)"},
        {"std::set", "Árbol Set (Balanceado)"},
        {"stack<", "Stack"},
        {"std::stack", "Stack"},
        {"queue<", "Queue"},
        {"std::queue", "Queue"},
        {"priority_queue", "Priority Queue/Heap"},
        {"std::priority_queue", "Priority Queue/Heap"},
        {"deque<", "Deque"},
        {"std::deque", "Deque"},
        {"list<", "Lista Enlazada"},
        {"std::list", "Lista Enlazada"}
    };
    
    for (const auto& [pattern, name] : patterns) {
        if (cleanedCode.find(pattern) != std::string::npos &&
            std::find(dataStructures.begin(), dataStructures.end(), name) == dataStructures.end()) {
            dataStructures.push_back(name);
        }
    }
    
    std::regex arrayRegex(R"(\b\w+\s+\w+\s*\[\s*\d+\s*\])");
    if (std::regex_search(cleanedCode, arrayRegex) &&
        std::find(dataStructures.begin(), dataStructures.end(), "Array Estático") == dataStructures.end()) {
        dataStructures.push_back("Array Estático");
    }
    
    return dataStructures;
}

std::vector<AlgorithmPattern> SolutionAnalyzer::detectPatterns(const std::string& code) {
    AnalysisContext context;
    context.rawCode = code;
    if (!code.empty()) {
        context.cleanedCode = getCleaned(code);
    }
    context.status = code.empty() ? AnalysisStatus::NoUserCode : AnalysisStatus::OK;
    return detectPatterns(context);
}

std::vector<AlgorithmPattern> SolutionAnalyzer::detectPatterns(AnalysisContext& context) {
    collectLoopSignals(context);
    auto patterns = detectPatternsFromClean(context.cleanedCode);
    adjustPatternConfidence(patterns, context);
    return patterns;
}

ComplexityAnalysis SolutionAnalyzer::estimateComplexity(const std::string& code) {
    AnalysisContext context;
    context.rawCode = code;
    if (!code.empty()) {
        context.cleanedCode = getCleaned(code);
    }
    context.status = code.empty() ? AnalysisStatus::NoUserCode : AnalysisStatus::OK;
    return estimateComplexityFromContext(context);
}

ComplexityAnalysis SolutionAnalyzer::estimateComplexityFromClean(const std::string& cleanedCode) {
    AnalysisContext context;
    context.rawCode = cleanedCode;
    context.cleanedCode = cleanedCode;
    context.status = cleanedCode.empty() ? AnalysisStatus::NoUserCode : AnalysisStatus::OK;
    return estimateComplexityFromContext(context);
}

ComplexityAnalysis SolutionAnalyzer::estimateComplexityFromContext(AnalysisContext& context) {
    collectLoopSignals(context);
    ComplexityAnalysis analysis;
    const std::string& cleanedCode = context.cleanedCode;
    if (cleanedCode.empty()) {
        analysis.timeComplexity = "O(1)";
        analysis.spaceComplexity = "O(1)";
        analysis.confidence = "low";
        analysis.explanation = "Sin bucles detectados";
        return analysis;
    }
    int maxDepth = countNestedLoops(context);
    bool hasLogLoop = false;
    bool hasLinearLoop = false;
    bool hasHashStructure = false;
    bool hasMemo = detectMemoization(cleanedCode);
    for (const auto& loop : context.loopSignals) {
        hasLogLoop = hasLogLoop || loop.isLogarithmic;
        hasLinearLoop = hasLinearLoop || !loop.isLogarithmic;
        hasHashStructure = hasHashStructure || loop.hasHashStructure;
    }
    if (maxDepth <= 0) {
        analysis.timeComplexity = "O(1)";
        analysis.spaceComplexity = hasMemo ? "O(n)" : "O(1)";
        analysis.confidence = "high";
        analysis.explanation = hasMemo ? "Uso de memoización sin bucles" : "Sin bucles detectados";
        return analysis;
    }
    if (maxDepth == 1) {
        if (hasLogLoop && !hasLinearLoop) {
            analysis.timeComplexity = "O(log n)";
            analysis.explanation = "Loop con actualizaciones logarítmicas";
        } else {
            analysis.timeComplexity = "O(n)";
            analysis.explanation = hasLogLoop ? "Loop lineal con operaciones logarítmicas parciales" : "Loop lineal";
        }
    } else if (maxDepth == 2) {
        bool innerLog = false;
        for (const auto& loop : context.loopSignals) {
            if (loop.depth >= 2 && loop.isLogarithmic) {
                innerLog = true;
                break;
            }
        }
        if (innerLog) {
            analysis.timeComplexity = "O(n log n)";
            analysis.explanation = "Loop externo lineal con loop interno logarítmico";
        } else {
            analysis.timeComplexity = "O(n²)";
            analysis.explanation = "Loops anidados";
        }
    } else {
        analysis.timeComplexity = "O(n^" + std::to_string(maxDepth) + ")";
        analysis.explanation = "Más de dos niveles de loops";
    }
    analysis.spaceComplexity = hasMemo ? "O(n)" : (hasHashStructure ? "O(n)" : "O(1)");
    analysis.confidence = "high";
    if (analysis.spaceComplexity == "O(n)") {
        analysis.explanation += " | Espacio: estructuras auxiliares que crecen con la entrada";
    }
    return analysis;
}

// TEST NOTES:
// 1. Plantilla vacía (solo main con lectura) debe arrojar status=NoUserCode y complejidad N/A; validar ejecutando analyzer.analyze sobre template base.
// 2. Código con marcadores // === BEGIN USER CODE === envolviendo un bucle for debe reportar O(n) y status OK; asegura que el harness fuera del bloque no afecta el resultado.
// 3. Submisión sin marcadores pero con lógica posterior a cin/cout debe analizar únicamente el tramo posterior y detectar complejidad correcta (por ejemplo O(n²) si hay doble loop después de la lectura).
// 4. Helper neutro sin IO (ej. solve()) llamado desde main debe incluirse en el snippet y permitir detectar patrones (e.g., recursión -> Divide and Conquer).

std::string SolutionAnalyzer::makeLLMRequest(const std::string& prompt) {
    // Validar que prompt no esté vacío
    if (prompt.empty()) {
        return "";
    }
    
    // Validar que openAIKey no esté vacío
    if (openAIKey.empty()) {
        return "";
    }
    
    // Similar a OpenAIClient pero enfocado en análisis
    std::string requestFile = "/tmp/analyzer_request_" + std::to_string(getpid()) + ".json";
    
    json requestBody = {
        {"model", "gpt-3.5-turbo"},
        {"messages", json::array({
            {
                {"role", "system"},
                {"content", "Eres un experto en análisis de algoritmos y complejidad computacional. "
                           "Siempre responde en ESPAÑOL LATINOAMERICANO. Usa un lenguaje técnico pero natural de Latinoamérica."}
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
    analysis.status = "OK"; // Default status expuesto a la API.
    
    // Validar entrada
    if (code.empty()) {
        analysis.status = "NoUserCode"; // Explicita que no hay código que analizar.
        analysis.complexity.timeComplexity = "N/A";
        analysis.complexity.spaceComplexity = "N/A";
        analysis.complexity.confidence = "low";
        analysis.complexity.explanation = "Código vacío";
        return analysis;
    }
    
    // Validar que los valores de tests sean >= 0
    if (testsPassed < 0) testsPassed = 0;
    if (testsFailed < 0) testsFailed = 0;
    
    std::string rawSnippet = extractRelevantCode(code);
    AnalysisStatus snippetStatus = lastSnippetStatus;
    
    AnalysisContext context;
    context.status = snippetStatus;
    context.rawCode = rawSnippet;
    if (!rawSnippet.empty()) {
        context.cleanedCode = getCleaned(rawSnippet);
    }
    
    bool hasUserCode = snippetStatus == AnalysisStatus::OK &&
        context.cleanedCode.find_first_not_of(" \t\r\n") != std::string::npos;
    
    if (!hasUserCode) {
        // Surface status so UI can indicate plantilla vacía instead of false positives.
        analysis.status = "NoUserCode";
        analysis.complexity.timeComplexity = "N/A";
        analysis.complexity.spaceComplexity = "N/A";
        analysis.complexity.confidence = "low";
        analysis.complexity.explanation = "No se detectó código del estudiante";
        return analysis;
    }
    
    analysis.status = "OK";
    
    collectLoopSignals(context);
    
    // 1. Análisis estático
    analysis.complexity = estimateComplexityFromContext(context);
    analysis.dataStructures = detectDataStructuresFromClean(context.cleanedCode);
    analysis.patterns = detectPatterns(context);
    adjustComplexityConfidence(analysis.complexity, context);
    
    // 2. Análisis con LLM (opcional, más preciso)
    if (!openAIKey.empty()) {
        const std::string& llmSource = context.rawCode.empty() ? code : context.rawCode;
        std::string truncatedCode = llmSource.substr(0, std::min<size_t>(800, llmSource.size()));
        
        std::string prompt = 
            "IMPORTANTE: Responde EN ESPAÑOL LATINOAMERICANO. Usa un lenguaje técnico pero natural de Latinoamérica."
            "Analiza este código C++ y proporciona:\n"
            "1. Complejidad temporal (Big O)\n"
            "2. Complejidad espacial\n"
            "3. Patrón algorítmico usado\n"
            "4. Una sugerencia de optimización (si aplica)\n\n"
            "Problema: " + problemDescription + "\n\n"
            "Código:\n```cpp\n" + truncatedCode + "\n```\n\n"
            "Tests pasados: " + std::to_string(testsPassed) + "\n"
            "Tests fallidos: " + std::to_string(testsFailed) + "\n\n";
        
        std::string llmResponse = makeLLMRequest(prompt);
        
        if (!llmResponse.empty()) {
            analysis.suggestions.push_back(llmResponse);
        }
    }
    
    // 3. Sugerencias basadas en análisis estático
    const std::string& timeComplexity = analysis.complexity.timeComplexity;
    bool isQuadraticOrHigher = timeComplexity.find("n²") != std::string::npos ||
                               timeComplexity.find("n³") != std::string::npos ||
                               timeComplexity.find("n^2") != std::string::npos ||
                               timeComplexity.find("n^3") != std::string::npos ||
                               timeComplexity.find("n^") != std::string::npos;
    if (isQuadraticOrHigher) {
        analysis.suggestions.push_back(
            "La complejidad es cuadrática o mayor. Considera usar estructuras como "
            "hash maps o algoritmos más eficientes."
        );
    }
    
    if (isQuadraticOrHigher &&
        analysis.dataStructures.empty() && countNestedLoops(context) > 1) {
        analysis.suggestions.push_back(
            "Múltiples loops sin estructuras auxiliares. Considera usar un hash map o set "
            "para reducir la complejidad."
        );
    }
    
    bool heavyOperations = false;
    for (const auto& loop : context.loopSignals) {
        if (loop.hasMultiplicativeUpdate || loop.hasModulo || loop.hasPower) {
            heavyOperations = true;
            break;
        }
    }
    if (heavyOperations) {
        analysis.suggestions.push_back(
            "Dentro de tus loops hay multiplicaciones/divisiones o módulo repetidos. "
            "Evalúa si puedes mover esas operaciones fuera del loop o precalcular resultados."
        );
    }
    
    if (testsFailed > 0 && analysis.patterns.empty()) {
        analysis.suggestions.push_back(
            "No se detectó un patrón algorítmico claro. Revisa si el problema "
            "requiere two pointers, sliding window, o búsqueda binaria."
        );
    }
    
    if (containsRecursion(context.cleanedCode) &&
        analysis.complexity.explanation.find("memoización") == std::string::npos) {
        analysis.suggestions.push_back(
            "Recursión detectada sin memoización. Considera cachear resultados para evitar recomputar."
        );
    }
    
    return analysis;
}

LoopSignals SolutionAnalyzer::analyzeLoop(const std::string& header,
                            const std::string& body,
                            int depth) const {
    LoopSignals signals;
    signals.depth = depth;
    signals.hasTwoPointers = hasTwoPointerVariables(body);
    signals.hasOppositePointerMovement = detectOppositePointerMovement(body);
    signals.hasPointerStopConditions = detectPointerStopCondition(header);
    signals.hasBinarySearchPattern = detectBinarySearchPattern(header + "\n" + body);
    signals.isLogarithmic = detectLogarithmicUpdate(header) ||
                            detectLogarithmicUpdate(body) ||
                            signals.hasBinarySearchPattern;
    signals.hasMultiplicativeUpdate = detectMultiplicativeUpdate(body);
    signals.hasModulo = detectModuloUsage(body);
    signals.hasPower = detectPowerOperation(body);
    signals.hasSorting = detectSortingInLoop(body);
    signals.hasHashStructure = detectHashStructure(body);
    signals.hasMemoization = detectMemoization(body);
    signals.hasDivision = body.find('/') != std::string::npos;
    return signals;
}

void SolutionAnalyzer::collectLoopSignals(AnalysisContext& context) {
    if (!context.loopSignals.empty()) {
        return;
    }
    const std::string& code = context.cleanedCode;
    if (code.empty()) {
        return;
    }
    struct StackFrame {
        bool isLoop;
        int loopIndex;
    };
    std::vector<StackFrame> stack;
    size_t i = 0;
    while (i < code.size()) {
        if (std::isspace(static_cast<unsigned char>(code[i]))) {
            ++i;
            continue;
        }
        bool isFor = startsWithWord(code, i, "for");
        bool isWhile = startsWithWord(code, i, "while");
        if (isFor || isWhile) {
            size_t headerStart = i;
            size_t keywordLen = isFor ? 3 : 5;
            i += keywordLen;
            size_t parenStart = skipWhitespace(code, i);
            if (parenStart >= code.size() || code[parenStart] != '(') {
                ++i;
                continue;
            }
            size_t parenEnd = findMatching(code, parenStart, '(', ')');
            if (parenEnd == std::string::npos) {
                break;
            }
            size_t headerEnd = parenEnd;
            std::string header = code.substr(headerStart, headerEnd - headerStart + 1);
            size_t bodyStart = skipWhitespace(code, parenEnd + 1);
            bool hasBraces = bodyStart < code.size() && code[bodyStart] == '{';
            size_t bodyEnd;
            if (hasBraces) {
                bodyEnd = findMatching(code, bodyStart, '{', '}');
                if (bodyEnd == std::string::npos) {
                    bodyEnd = code.size() - 1;
                }
            } else {
                bodyEnd = findStatementEnd(code, bodyStart);
                if (bodyEnd == std::string::npos) {
                    bodyEnd = code.size();
                } else if (bodyEnd > 0) {
                    --bodyEnd;
                }
            }
            if (bodyEnd <= bodyStart || bodyStart >= code.size()) {
                ++i;
                continue;
            }
            std::string body = code.substr(bodyStart, bodyEnd - bodyStart + 1);
            int depth = 0;
            for (const auto& frame : stack) {
                if (frame.isLoop) {
                    ++depth;
                }
            }
            LoopSignals signals = analyzeLoop(header, body, depth + 1);
            context.loopSignals.push_back(signals);
            if (signals.hasTwoPointers) {
                context.patternScores["Two Pointers"] += signals.hasOppositePointerMovement ? 1.0 : 0.5;
            }
            if (signals.hasBinarySearchPattern) {
                context.patternScores["Binary Search"] += 1.0;
            }
            if (signals.isLogarithmic) {
                context.patternScores["Logarithmic"] += 1.0;
            }
            if (hasBraces) {
                stack.push_back({true, static_cast<int>(context.loopSignals.size() - 1)});
                i = bodyStart + 1;
            } else {
                i = bodyEnd + 1;
            }
            continue;
        }
        if (code[i] == '{') {
            stack.push_back({false, -1});
            ++i;
            continue;
        }
        if (code[i] == '}') {
            if (!stack.empty()) {
                stack.pop_back();
            }
            ++i;
            continue;
        }
        ++i;
    }
}

void SolutionAnalyzer::adjustPatternConfidence(std::vector<AlgorithmPattern>& patterns,
                                 const AnalysisContext& context) const {
    if (patterns.empty()) {
        return;
    }
    bool hasTwoPointers = false;
    bool validTwoPointers = false;
    bool hasBinarySearch = false;
    bool hasMemo = detectMemoization(context.cleanedCode);
    for (const auto& loop : context.loopSignals) {
        if (loop.hasTwoPointers) {
            hasTwoPointers = true;
            if (loop.hasOppositePointerMovement && loop.hasPointerStopConditions) {
                validTwoPointers = true;
            }
        }
        if (loop.hasBinarySearchPattern) {
            hasBinarySearch = true;
        }
    }
    for (auto& pattern : patterns) {
        if (pattern.patternName == "Two Pointers") {
            if (!hasTwoPointers) {
                pattern.confidence = "low";
                pattern.description += " (no se detectaron punteros complementarios)";
            } else if (!validTwoPointers) {
                pattern.confidence = "medium";
                pattern.description += " (los punteros no avanzan en direcciones opuestas)";
            } else {
                pattern.confidence = "high";
            }
        } else if (pattern.patternName == "Binary Search") {
            pattern.confidence = hasBinarySearch ? "high" : "low";
        } else if (pattern.patternName == "Dynamic Programming") {
            pattern.confidence = hasMemo ? "high" : "medium";
        }
    }
}

void SolutionAnalyzer::adjustComplexityConfidence(ComplexityAnalysis& analysis,
                                    const AnalysisContext& context) const {
    if (context.loopSignals.empty()) {
        if (analysis.timeComplexity != "O(1)") {
            analysis.confidence = "low";
        }
        return;
    }
    int maxDepth = 0;
    bool hasLog = false;
    for (const auto& loop : context.loopSignals) {
        maxDepth = std::max(maxDepth, loop.depth);
        if (loop.isLogarithmic || loop.hasBinarySearchPattern) {
            hasLog = true;
        }
    }
    bool mismatch = false;
    if ((analysis.timeComplexity.find("n²") != std::string::npos ||
         analysis.timeComplexity.find("n^2") != std::string::npos) &&
        maxDepth < 2) {
        mismatch = true;
    }
    if (analysis.timeComplexity.find("n log n") != std::string::npos && (!hasLog || maxDepth < 2)) {
        mismatch = true;
    }
    if (analysis.timeComplexity.find("log n") != std::string::npos && !hasLog) {
        mismatch = true;
    }
    if (analysis.timeComplexity == "O(1)" && maxDepth > 0) {
        mismatch = true;
    }
    if (mismatch) {
        analysis.confidence = (analysis.confidence == "high") ? "medium" : "low";
    } else if (analysis.confidence.empty()) {
        analysis.confidence = "high";
    }
}

std::vector<AlgorithmPattern> SolutionAnalyzer::detectPatternsFromClean(const std::string& cleanedCode) {
    std::vector<AlgorithmPattern> patterns;
    if (cleanedCode.empty()) {
        return patterns;
    }
    auto pushPattern = [&](const std::string& name, const std::string& desc) {
        patterns.push_back(AlgorithmPattern{name, "medium", desc});
    };
    if (hasTwoPointerVariables(cleanedCode)) {
        pushPattern("Two Pointers", "Uso de dos punteros sobre la colección");
    }
    if (containsBinarySearch(cleanedCode)) {
        pushPattern("Binary Search", "Comparaciones con mid, left y right");
    }
    if (detectMemoization(cleanedCode)) {
        pushPattern("Dynamic Programming", "Se detecta estructura de memoización");
    }
    if (cleanedCode.find("queue") != std::string::npos) {
        pushPattern("BFS", "Uso de cola para recorrido");
    }
    if (cleanedCode.find("stack") != std::string::npos) {
        pushPattern("DFS", "Uso de stack para recorrido");
    }
    if (cleanedCode.find("priority_queue") != std::string::npos) {
        pushPattern("Priority Queue", "Uso de heap / priority_queue");
    }
    return patterns;
}