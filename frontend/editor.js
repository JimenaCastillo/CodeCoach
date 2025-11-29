let editor; // Referencia al textarea del editor

// Marcadores para identificar sección de código del usuario
const USER_BLOCK_START = '// === BEGIN USER CODE ===';
const USER_BLOCK_END = '// === END USER CODE ===';

// Inicializar editor (simple textarea)
document.addEventListener('DOMContentLoaded', function() {
    editor = document.getElementById('editor');
    if (editor) {
        // Código inicial por defecto
        editor.value =
`#include <iostream>
int main() {
    ${USER_BLOCK_START}
    std::cout << "Hello, World!";
    ${USER_BLOCK_END}
    return 0;
}`;
    }
});

// Obtener código del editor
function getEditorCode() {
    return editor ? editor.value : '';
}

// Establecer código en el editor
function setEditorCode(code) {
    if (editor) {
        editor.value = code;
    }
}

// Limpiar editor con template por defecto
function clearEditor() {
    setEditorCode(
`#include <iostream>
int main() {
    ${USER_BLOCK_START}
    // Tu código aquí
    ${USER_BLOCK_END}
    return 0;
}`);
}
