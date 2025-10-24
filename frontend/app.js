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
        item.textContent = problem.title;
        item.onclick = (e) => selectProblem(problem, e.target);
        problemsList.appendChild(item);
    });
}

// Seleccionar problema
function selectProblem(problem, element) {
    currentProblem = problem;
    
    // Actualizar UI
    document.getElementById('problemTitle').textContent = problem.title;
    document.getElementById('problemDescription').textContent = problem.description;
    
    // Actualizar item activo
    document.querySelectorAll('.problem-item').forEach(item => {
        item.classList.remove('active');
    });
    if (element) {
        element.classList.add('active');
    }
    
    // Limpiar código
    clearEditor();
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
    
    // Mostrar que está ejecutando
    const btn = document.getElementById('executeBtn');
    btn.textContent = 'Ejecutando...';
    btn.classList.add('loading');
    btn.disabled = true;
    
    // Ejecutar
    const result = await executeCode(code, [
        { input: '', expectedOutput: '5' }
    ]);
    
    // Restaurar botón
    btn.textContent = 'Ejecutar Código';
    btn.classList.remove('loading');
    btn.disabled = false;
    
    // Mostrar resultados
    displayResults(result);
    
    // Obtener feedback de OpenAI si hay error o fallo
    if (!result.success || result.testsFailed > 0) {
        const feedbackData = await getFeedback(
            code,
            result.testsPassed || 0,
            result.testsFailed || 0,
            result.compilationError || '',
            result.runtimeError || ''
        );
        
        if (feedbackData.success) {
            displayFeedback(feedbackData.feedback);
        }
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
            <div class="result-content">${escapeHtml(result.compilationError)}</div>
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
        result.testOutputs.forEach((output, i) => {
            const passed = result.testResults[i];
            const item = document.createElement('div');
            item.className = `result-item ${passed ? 'passed' : 'failed'}`;
            item.innerHTML = `
                <div class="result-label">${passed ? '✅' : '❌'} Test ${i + 1}</div>
                <div class="result-content">Output: ${escapeHtml(output)}</div>
            `;
            resultsDiv.appendChild(item);
        });
    } else {
        const item = document.createElement('div');
        item.className = 'result-item passed';
        item.innerHTML = `
            <div class="result-label">✅ ¡Todos los tests pasaron!</div>
            <div class="result-content">
                Tests Pasados: ${result.testsPassed}<br>
                Tiempo: ${result.executionTime.toFixed(3)}s
            </div>
        `;
        resultsDiv.appendChild(item);
    }
}

// Escape HTML para seguridad
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// Mostrar feedback del LLM
function displayFeedback(feedback) {
    const resultsDiv = document.getElementById('results');
    const feedbackItem = document.createElement('div');
    feedbackItem.className = 'result-item';
    feedbackItem.style.borderLeftColor = '#58a6ff';
    feedbackItem.style.backgroundColor = '#0d2438';
    feedbackItem.innerHTML = `
        <div class="result-label">💡 Feedback del Coach IA</div>
        <div class="result-content">${escapeHtml(feedback)}</div>
    `;
    resultsDiv.appendChild(feedbackItem);
}

// Iniciar cuando el DOM esté listo
document.addEventListener('DOMContentLoaded', init);