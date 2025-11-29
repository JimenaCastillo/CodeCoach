// URL base del backend (puede cambiarse según configuración)
const API_URL = 'http://localhost:8080/api';

// Obtener lista de problemas desde el backend
async function getProblems() {
    try {
        const response = await fetch(`${API_URL}/problems`);
        if (!response.ok) throw new Error('Error al obtener problemas');
        return await response.json();
    } catch (error) {
        console.error('Error:', error);
        return [];
    }
}

// Obtiene detalles completos de un problema específico
async function getProblem(id) {
    try {
        const response = await fetch(`${API_URL}/problems/${id}`);
        if (!response.ok) throw new Error('Error al obtener problema');
        return await response.json();
    } catch (error) {
        console.error('Error:', error);
        return null;
    }
}

// Ejecuta código del usuario en el backend
async function executeCode(code, testCases, problemDescription = '') {
    try {
        const response = await fetch(`${API_URL}/execute`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                code: code,
                testCases: testCases,
                problemDescription: problemDescription
            })
        });
        
        if (!response.ok) throw new Error('Error al ejecutar código');
        return await response.json();
    } catch (error) {
        console.error('Error:', error);
        return {
            success: false,
            error: error.message
        };
    }
}

// Obtener feedback de OpenAI
async function getFeedback(code, testsPassed, testsFailed, compilationError, runtimeError) {
    try {
        const response = await fetch(`${API_URL}/feedback`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                code: code,
                testsPassed: testsPassed,
                testsFailed: testsFailed,
                compilationError: compilationError,
                runtimeError: runtimeError
            })
        });
        
        if (!response.ok) throw new Error('Error al obtener feedback');
        return await response.json();
    } catch (error) {
        console.error('Error:', error);
        return {
            success: false,
            error: error.message
        };
    }
}

// Crear nuevo problema (ADMIN)
async function createProblem(problemData) {
    try {
        const response = await fetch(`${API_URL}/problems`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(problemData)
        });
        
        if (!response.ok) throw new Error('Error al crear problema');
        return await response.json();
    } catch (error) {
        console.error('Error:', error);
        return {
            success: false,
            error: error.message
        };
    }
}