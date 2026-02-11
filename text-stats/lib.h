#ifndef LIB_H
#define LIB_H

#include <windows.h>

// Estado del comando
typedef enum {
    CMD_OK = 0,
    CMD_ERROR = 1,
    CMD_SALIR = 2
} TEstadoComando;

// ------------------ ESTRUCTURAS ------------------

// Nodo de palabra (lista enlazada)
typedef struct TNodoPalabra {
    char *palabra;
    int contador;
    struct TNodoPalabra *sig;
} TNodoPalabra;

// Bloque de información
typedef struct TBloque {
    char nombre[256];
    TNodoPalabra *listaPalabras;
    long numLineas;
    long numFrases;
    struct TBloque *sig;

    // ===== NUEVO: para recargar y para frases <palabra> =====
    char **ficheros;     // array dinámico de nombres de fichero
    int numFicheros;     // cuántos
    int usarStop;        // si se cargó con "cargar stop ..."
} TBloque;



// ------------------ FUNCIONES PÚBLICAS ------------------

// Colores
void setTextColor(int color);

// Delimitadores
void initDelimitadores(void);

// Interfaz principal
void imprimirDatosEstudiante(void);
void imprimirPrompt(void);
TEstadoComando analizarComando(const char *command);
void imprimirFin(void);

// Liberación de memoria global
void liberarMemoria(void);

#endif
