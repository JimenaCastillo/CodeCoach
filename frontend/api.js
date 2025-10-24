const API_URL = 'http://localhost:8080/api';

// Obtener lista de problemas
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

// Obtener problema específico
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

// Ejecutar código
async function executeCode(code, testCases) {
    try {
        const response = await fetch(`${API_URL}/execute`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                code: code,
                testCases: testCases
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