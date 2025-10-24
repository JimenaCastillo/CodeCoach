let editor;

// Inicializar editor (simple textarea)
document.addEventListener('DOMContentLoaded', function() {
    editor = document.getElementById('editor');
    if (editor) {
        editor.value = '#include <iostream>\nint main() {\n    std::cout << "Hello, World!";\n    return 0;\n}';
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

// Limpiar editor
function clearEditor() {
    setEditorCode('#include <iostream>\nint main() {\n    // Tu código aquí\n    return 0;\n}');
}