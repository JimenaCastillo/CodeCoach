#ifndef SOLUTION_ANALYZER_H
#define SOLUTION_ANALYZER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

enum class AnalysisStatus {
    OK,
    NoUserCode
};

struct ComplexityAnalysis {
    std::string timeComplexity;
    std::string spaceComplexity;
    std::string confidence;
    std::string explanation;
};

struct AlgorithmPattern {
    std::string patternName;
    std::string confidence;
    std::string description;
};

struct LoopSignals {
    int depth = 0;
    bool hasMultiplicativeUpdate = false;
    bool hasModulo = false;
    bool hasDivision = false;
    bool hasPower = false;
    bool isLogarithmic = false;
    bool hasTwoPointers = false;
    bool hasOppositePointerMovement = false;
    bool hasPointerStopConditions = false;
    bool hasBinarySearchPattern = false;
    bool hasSorting = false;
    bool hasMemoization = false;
    bool hasHashStructure = false;
};

struct PatternScore {
    std::string name;
    std::string confidence;
    std::string description;
    double score = 0.0;
};

struct ComplexityScore {
    std::string timeComplexity;
    std::string spaceComplexity;
    std::string confidence;
    std::string explanation;
    double score = 0.0;
};

struct AnalysisContext {
    AnalysisStatus status = AnalysisStatus::NoUserCode;
    std::string rawCode;
    std::string cleanedCode;
    std::vector<LoopSignals> loopSignals;
    std::unordered_map<std::string, double> patternScores;
};

struct CodeAnalysis {
    std::string status;
    ComplexityAnalysis complexity;
    std::vector<AlgorithmPattern> patterns;
    std::vector<std::string> dataStructures;
    std::vector<std::string> suggestions;
};

class SolutionAnalyzer {
private:
    std::string openAIKey;
    std::unordered_map<std::string, std::string> cleanCache;
    AnalysisStatus lastSnippetStatus = AnalysisStatus::NoUserCode;

public:
    SolutionAnalyzer(const std::string& apiKey);
    
    CodeAnalysis analyze(const std::string& code, 
                        const std::string& problemDescription,
                        int testsPassed,
                        int testsFailed);
    
    ComplexityAnalysis estimateComplexity(const std::string& code);
    std::vector<std::string> detectDataStructures(const std::string& code);
    std::vector<AlgorithmPattern> detectPatterns(const std::string& code);
    
private:
    std::string cleanCode(const std::string& code);
    const std::string& getCleaned(const std::string& code);
    std::string extractRelevantCode(const std::string& code);
    std::string makeLLMRequest(const std::string& prompt);
    void collectLoopSignals(AnalysisContext& context);
    int countNestedLoops(AnalysisContext& context);
    bool containsRecursion(const std::string& cleanedCode);
    bool containsBinarySearch(const std::string& cleanedCode);
    bool containsSorting(const std::string& cleanedCode);
    bool detectLogarithmicUpdate(const std::string& line) const;
    bool detectOppositePointerMovement(const std::string& loopBody) const;
    bool detectPointerStopCondition(const std::string& loopHeader) const;
    bool detectMultiplicativeUpdate(const std::string& loopBody) const;
    bool detectModuloUsage(const std::string& loopBody) const;
    bool detectPowerOperation(const std::string& loopBody) const;
    bool detectBinarySearchPattern(const std::string& loopBody) const;
    bool detectSortingInLoop(const std::string& loopBody) const;
    bool detectHashStructure(const std::string& loopBody) const;
    bool detectMemoization(const std::string& code) const;
    bool hasTwoPointerVariables(const std::string& loopBody) const;
    LoopSignals analyzeLoop(const std::string& header,
                            const std::string& body,
                            int depth) const;
    std::vector<std::string> detectDataStructuresFromClean(const std::string& cleanedCode);
    std::vector<AlgorithmPattern> detectPatternsFromClean(const std::string& cleanedCode);
    std::vector<AlgorithmPattern> detectPatterns(AnalysisContext& context);
    ComplexityAnalysis estimateComplexityFromClean(const std::string& cleanedCode);
    ComplexityAnalysis estimateComplexityFromContext(AnalysisContext& context);
    void adjustPatternConfidence(std::vector<AlgorithmPattern>& patterns,
                                 const AnalysisContext& context) const;
    void adjustComplexityConfidence(ComplexityAnalysis& analysis,
                                    const AnalysisContext& context) const;
};

#endif
