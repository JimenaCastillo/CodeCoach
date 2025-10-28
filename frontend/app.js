let currentProblem = null;
let problems = [];

// Inicializar aplicación
async function init() {
    await loadProblems();
    setupEventListeners();
}

// Cargar lista de problemas
async function loadProblems() {
    problems = await getProblems();
    renderProblems();
    
    // Seleccionar el primer problema automáticamente
    if (problems.length > 0) {
        selectProblem(problems[0]);
    }
}

// Renderizar lista de problemas
function renderProblems() {
    const problemsList = document.getElementById('problemsList');
    problemsList.innerHTML = '';
    
    problems.forEach((problem) => {
        const item = document.createElement('div');
        item.className = 'problem-item';
        
        // Agregar badge de dificultad
        const difficultyBadge = document.createElement('span');
        difficultyBadge.className = `difficulty-badge ${problem.difficulty.toLowerCase()}`;
        difficultyBadge.textContent = problem.difficulty;
        
        const titleDiv = document.createElement('div');
        titleDiv.textContent = problem.title;
        
        item.appendChild(difficultyBadge);
        item.appendChild(titleDiv);
        item.onclick = (e) => selectProblem(problem, e.currentTarget);
        problemsList.appendChild(item);
    });
}

// Seleccionar problema
async function selectProblem(problem, element) {
    // Cargar detalles completos del problema
    const fullProblem = await getProblem(problem.id);
    if (!fullProblem) return;
    
    currentProblem = fullProblem;
    
    // Actualizar UI
    document.getElementById('problemTitle').textContent = fullProblem.title;
    document.getElementById('problemDescription').textContent = fullProblem.description;
    
    // Mostrar categoría y dificultad
    const metaDiv = document.getElementById('problemMeta');
    if (metaDiv) {
        metaDiv.innerHTML = `
            <span class="badge">${fullProblem.category}</span>
            <span class="badge difficulty-${fullProblem.difficulty.toLowerCase()}">${fullProblem.difficulty}</span>
        `;
    }
    
    // Actualizar item activo
    document.querySelectorAll('.problem-item').forEach(item => {
        item.classList.remove('active');
    });
    if (element) {
        element.classList.add('active');
    }
    
    // Establecer template de solución si existe
    if (fullProblem.solutionTemplate) {
        setEditorCode(fullProblem.solutionTemplate);
    } else {
        clearEditor();
    }
    
    // Limpiar resultados anteriores
    document.getElementById('results').innerHTML = '<p>Ejecuta tu código para ver los resultados</p>';
}

// Setup de event listeners
function setupEventListeners() {
    document.getElementById('executeBtn').addEventListener('click', executeUserCode);
}

// Ejecutar código del usuario
async function executeUserCode() {
    if (!currentProblem) {
        alert('Por favor selecciona un problema primero');
        return;
    }
    
    const code = getEditorCode();
    if (!code.trim()) {
        alert('Por favor escribe código');
        return;
    }
    
    // Validar tamaño del código
    if (code.length > 50000) {
        alert('El código es demasiado largo (máximo 50KB)');
        return;
    }
    
    // Mostrar que está ejecutando
    const btn = document.getElementById('executeBtn');
    btn.textContent = 'Ejecutando...';
    btn.classList.add('loading');
    btn.disabled = true;
    
    // Limpiar resultados previos
    document.getElementById('results').innerHTML = '<p>⏳ Compilando y ejecutando...</p>';
    
    try {
        // Obtener test cases del problema
        const testCases = currentProblem.testCases || [
            { input: '', expectedOutput: '5' }
        ];
        
        // Ejecutar
        const result = await executeCode(code, testCases, currentProblem.description);
        
        // Mostrar resultados
        displayResults(result);
        
        // Mostrar análisis de complejidad si está disponible
        if (result.analysis) {
            displayAnalysis(result.analysis);
        }
        
        // Obtener feedback de OpenAI si hay error o fallo
        if (!result.success || result.testsFailed > 0) {
            displayLoadingFeedback();
            
            const feedbackData = await getFeedback(
                code,
                result.testsPassed || 0,
                result.testsFailed || 0,
                result.compilationError || '',
                result.runtimeError || ''
            );
            
            if (feedbackData.success) {
                displayFeedback(feedbackData.feedback);
            } else {
                displayFeedbackError(feedbackData.error);
            }
        }
    } catch (error) {
        displayError('Error inesperado: ' + error.message);
    } finally {
        // Restaurar botón
        btn.textContent = 'Ejecutar Código';
        btn.classList.remove('loading');
        btn.disabled = false;
    }
}

// Mostrar resultados
function displayResults(result) {
    const resultsDiv = document.getElementById('results');
    resultsDiv.innerHTML = '';
    
    if (result.compilationError) {
        const item = document.createElement('div');
        item.className = 'result-item error';
        item.innerHTML = `
            <div class="result-label">❌ Error de Compilación</div>
            <div class="result-content"><pre>${escapeHtml(result.compilationError)}</pre></div>
        `;
        resultsDiv.appendChild(item);
        return;
    }
    
    if (result.runtimeError && result.runtimeError.includes('TIMEOUT')) {
        const item = document.createElement('div');
        item.className = 'result-item error';
        item.innerHTML = `
            <div class="result-label">⏱️ Timeout</div>
            <div class="result-content">${escapeHtml(result.runtimeError)}</div>
        `;
        resultsDiv.appendChild(item);
        return;
    }
    
    if (!result.success) {
        const item = document.createElement('div');
        item.className = 'result-item failed';
        item.innerHTML = `
            <div class="result-label">⚠️ Tests Fallidos</div>
            <div class="result-content">
                Pasados: ${result.testsPassed} / Fallidos: ${result.testsFailed}
            </div>
        `;
        resultsDiv.appendChild(item);
        
        // Mostrar detalle de cada test
        if (result.testOutputs && result.testResults) {
            result.testOutputs.forEach((output, i) => {
                const passed = result.testResults[i];
                const item = document.createElement('div');
                item.className = `result-item ${passed ? 'passed' : 'failed'}`;
                item.innerHTML = `
                    <div class="result-label">${passed ? '✅' : '❌'} Test ${i + 1}</div>
                    <div class="result-content">
                        <strong>Output:</strong> <code>${escapeHtml(output)}</code>
                    </div>
                `;
                resultsDiv.appendChild(item);
            });
        }
    } else {
        const item = document.createElement('div');
        item.className = 'result-item passed';
        item.innerHTML = `
            <div class="result-label">✅ ¡Todos los tests pasaron!</div>
            <div class="result-content">
                <strong>Tests Pasados:</strong> ${result.testsPassed}<br>
                <strong>Tiempo de Ejecución:</strong> ${result.executionTime.toFixed(3)}s
            </div>
        `;
        resultsDiv.appendChild(item);
    }
}

// Mostrar análisis de complejidad
function displayAnalysis(analysis) {
    const resultsDiv = document.getElementById('results');
    const analysisItem = document.createElement('div');
    analysisItem.className = 'result-item analysis';
    analysisItem.style.borderLeftColor = '#a371f7';
    analysisItem.style.backgroundColor = '#1c1525';
    
    let patternsHTML = '';
    if (analysis.patterns && analysis.patterns.length > 0) {
        patternsHTML = '<br><strong>Patrones Detectados:</strong><ul>';
        analysis.patterns.forEach(pattern => {
            patternsHTML += `<li><strong>${pattern.name}</strong> (${pattern.confidence}): ${pattern.description}</li>`;
        });
        patternsHTML += '</ul>';
    }
    
    let dataStructuresHTML = '';
    if (analysis.dataStructures && analysis.dataStructures.length > 0) {
        dataStructuresHTML = '<br><strong>Estructuras de Datos:</strong> ' + 
                            analysis.dataStructures.join(', ');
    }
    
    let suggestionsHTML = '';
    if (analysis.suggestions && analysis.suggestions.length > 0) {
        suggestionsHTML = '<br><strong>Sugerencias:</strong><ul>';
        analysis.suggestions.forEach(suggestion => {
            suggestionsHTML += `<li>${escapeHtml(suggestion)}</li>`;
        });
        suggestionsHTML += '</ul>';
    }
    
    analysisItem.innerHTML = `
        <div class="result-label">📊 Análisis de Complejidad</div>
        <div class="result-content">
            <strong>Complejidad Temporal:</strong> ${analysis.timeComplexity} 
            <span class="confidence">(Confianza: ${analysis.confidence})</span><br>
            <strong>Complejidad Espacial:</strong> ${analysis.spaceComplexity}<br>
            <em>${escapeHtml(analysis.explanation)}</em>
            ${patternsHTML}
            ${dataStructuresHTML}
            ${suggestionsHTML}
        </div>
    `;
    resultsDiv.appendChild(analysisItem);
}

// Mostrar loading mientras espera feedback
function displayLoadingFeedback() {
    const resultsDiv = document.getElementById('results');
    const feedbackItem = document.createElement('div');
    feedbackItem.id = 'feedbackItem';
    feedbackItem.className = 'result-item';
    feedbackItem.style.borderLeftColor = '#58a6ff';
    feedbackItem.style.backgroundColor = '#0d2438';
    feedbackItem.innerHTML = `
        <div class="result-label">💡 Feedback del Coach IA</div>
        <div class="result-content">⏳ Analizando tu código...</div>
    `;
    resultsDiv.appendChild(feedbackItem);
}

// Mostrar feedback del LLM
function displayFeedback(feedback) {
    const feedbackItem = document.getElementById('feedbackItem');
    if (feedbackItem) {
        feedbackItem.innerHTML = `
            <div class="result-label">💡 Feedback del Coach IA</div>
            <div class="result-content">${escapeHtml(feedback)}</div>
        `;
    }
}

// Mostrar error de feedback
function displayFeedbackError(error) {
    const feedbackItem = document.getElementById('feedbackItem');
    if (feedbackItem) {
        feedbackItem.innerHTML = `
            <div class="result-label">💡 Feedback del Coach IA</div>
            <div class="result-content">⚠️ ${escapeHtml(error)}</div>
        `;
    }
}

// Mostrar error general
function displayError(message) {
    const resultsDiv = document.getElementById('results');
    resultsDiv.innerHTML = `
        <div class="result-item error">
            <div class="result-label">❌ Error</div>
            <div class="result-content">${escapeHtml(message)}</div>
        </div>
    `;
}

// Escape HTML para seguridad
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// Iniciar cuando el DOM esté listo
document.addEventListener('DOMContentLoaded', init);