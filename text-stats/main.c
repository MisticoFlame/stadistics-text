#include <stdio.h>
#include <stdlib.h>
#include "lib.h"

int main() {
    // Imprimir datos del estudiante
    imprimirDatosEstudiante();

    // Inicializar delimitadores por defecto (espacio y '\n')
    initDelimitadores();

    while (1) {
        imprimirPrompt();

        // Lectura dinámica de la línea de comando
        char *command = NULL;
        int size = 0;
        char ch;

        while ((ch = getchar()) != '\n' && ch != EOF) {
            char *tmp = realloc(command, size + 1);
            if (tmp == NULL) {
                free(command);
                fprintf(stderr, "Memory allocation failed\n");
                return 1;
            }
            command = tmp;
            command[size++] = ch;
        }

        if (ch == EOF) {
            free(command);
            break;
        }

        char *tmp = realloc(command, size + 1);
        if (tmp == NULL) {
            free(command);
            fprintf(stderr, "Memory allocation failed\n");
            return 1;
        }
        command = tmp;
        command[size] = '\0';


        TEstadoComando estado = analizarComando(command);

        free(command);

        if (estado == CMD_SALIR) {
            break;
        }
    }

    // Liberar toda la memoria dinámica usada en bloques/palabras/stop-words
    liberarMemoria();

    // Mensaje final
    imprimirFin();
    return 0;
}
