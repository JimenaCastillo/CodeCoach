#include "solution_analyzer.h"
#include <iostream>
#include <sstream>
#include <regex>
#include <vector>

static std::string replicateExtract(const std::string& code) {
    auto cleanCode = [](const std::string& source) {
        std::string cleaned;
        cleaned.reserve(source.size());
        bool inString = false;
        bool inLine = false;
        bool inBlock = false;
        char stringChar = '\0';
        for (size_t i = 0; i < source.size(); ++i) {
            char c = source[i];
            char prev = (i > 0) ? source[i - 1] : '\0';
            char next = (i + 1 < source.size()) ? source[i + 1] : '\0';
            if (inLine) {
                if (c == '\n') {
                    inLine = false;
                    cleaned += '\n';
                }
                continue;
            }
            if (inBlock) {
                if (c == '*' && next == '/') {
                    inBlock = false;
                    ++i;
                }
                continue;
            }
            if (!inString) {
                if (c == '/' && next == '/') {
                    inLine = true;
                    ++i;
                    continue;
                }
                if (c == '/' && next == '*') {
                    inBlock = true;
                    ++i;
                    continue;
                }
            }
            if ((c == '"' || c == '\'') && prev != '\\') {
                if (!inString) {
                    inString = true;
                    stringChar = c;
                } else if (c == stringChar) {
                    inString = false;
                }
            }
            if (inString) {
                cleaned += (c == '\n') ? '\n' : ' ';
            } else {
                cleaned += c;
            }
        }
        return cleaned;
    };
    std::string cleaned = cleanCode(code);
    std::regex funcRegex(R"((?:^|\s)([A-Za-z_]\w*)\s*\([^;{}]*\)\s*(?:const)?\s*\{)");
    std::sregex_iterator it(cleaned.begin(), cleaned.end(), funcRegex);
    std::sregex_iterator end;
    std::string relevant;
    while (it != end) {
        std::string funcName = (*it)[1];
        if (funcName == "if" || funcName == "for" || funcName == "while" ||
            funcName == "switch" || funcName == "catch" || funcName == "else") {
            ++it;
            continue;
        }
        std::string lower;
        for (char c : funcName) {
            lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        if (lower != "main") {
            static const std::vector<std::string> ignoreKeywords = {
                "parse", "read", "print", "input", "output", "write",
                "scan", "display", "log", "debug"
            };
            bool skip = false;
            for (const auto& keyword : ignoreKeywords) {
                if (lower.find(keyword) != std::string::npos) {
                    skip = true;
                    break;
                }
            }
            if (skip) {
                ++it;
                continue;
            }
        }
        size_t signaturePos = static_cast<size_t>((*it).position());
        size_t bodyStart = signaturePos + (*it).length();
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
        relevant.append(cleaned.substr(signaturePos, pos - signaturePos));
        relevant.push_back('\n');
        ++it;
    }
    if (relevant.empty()) {
        return cleaned;
    }
    return relevant;
}

int main() {
    std::stringstream buffer;
    buffer << std::cin.rdbuf();
    std::string code = buffer.str();
    SolutionAnalyzer analyzer("");
    auto direct = analyzer.estimateComplexity(code);
    std::string snippet = replicateExtract(code);
    auto snippetEstimate = analyzer.estimateComplexity(snippet);
    auto analysis = analyzer.analyze(code, "", 3, 0);
    std::cout << "estimate(time): " << direct.timeComplexity << "\n";
    std::cout << "estimate(space): " << direct.spaceComplexity << "\n";
    std::cout << "snippet(time): " << snippetEstimate.timeComplexity << "\n";
    std::cout << "snippet(space): " << snippetEstimate.spaceComplexity << "\n";
    std::cout << "analysis(time): " << analysis.complexity.timeComplexity << "\n";
    std::cout << "analysis(space): " << analysis.complexity.spaceComplexity << "\n";
    std::cout << "analysis explanation: " << analysis.complexity.explanation << "\n";
    std::cout << "patterns:\n";
    for (const auto& p : analysis.patterns) {
        std::cout << "- " << p.patternName << "\n";
    }
    std::cout << "\nRelevant snippet:\n" << snippet << "\n";
    return 0;
}
