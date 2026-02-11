#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include "lib.h"

#define MAX_DELIMS 64
#define MAX_TOKENS 32

// =========================================================
// DELIMITADORES
// =========================================================
static char delimitadores[MAX_DELIMS];
static int numDelims = 0;

// =========================================================
// BLOQUES
// =========================================================
static TBloque *listaBloques = NULL;
static TBloque *bloqueActivo = NULL;

// =========================================================
// STOP WORDS
// =========================================================
typedef struct TStopWord {
    char *palabra;
    struct TStopWord *sig;
} TStopWord;

static TStopWord *listaStopWords = NULL;

// =========================================================
// BYTES + ESPACIOS (estructura externa)
// =========================================================
typedef struct TBloqueBytes {
    TBloque *bloque;
    long bytes;
    long numEspacios;              // ===== EJERCICIO 2 =====
    struct TBloqueBytes *sig;
} TBloqueBytes;

static TBloqueBytes *listaBytes = NULL;

// =========================================================
// PROTOTIPOS
// =========================================================
static void normalizarPalabra(char *pal);

static int esDelimitador(char c);
static int existeDelimitador(char c);
static int parseDelimitadorToken(const char *token, char *outDelim);

static void comando_delimitador_ver(void);
static int comando_delimitador_insertar(char **args, int numArgs);

static TNodoPalabra* buscarPalabra(TNodoPalabra *lista, const char *pal);
static TNodoPalabra* insertarOIncrementarPalabra(TNodoPalabra **lista, const char *pal);

static void liberarListaPalabras(TNodoPalabra *lista);
static void liberarListaBloques(TBloque *lista);

static void liberarStopWords(void);
static int existeStopWord(const char *pal);
static int cargarStopWordsObligatorio(void);
static int cargarStopWordsSiExiste(void);
static int esStopWord(const char *pal);

static TBloque* crearBloqueVacio(const char *nombreBloque);
static void insertarBloqueEnListaAlFinal(TBloque *b);

static void procesarLineaTexto(TBloque *bloque, const char *linea, int usarStop);

static int comando_cargar(char **nombresFicheros, int numFicheros, int usarStop);
static void comando_frases(void);
static void comando_bloques(void);
static int comando_cambiar(const char *nombre);

static long contarTotalApariciones(TNodoPalabra *lista);
static int contarPalabrasDistintas(TNodoPalabra *lista);

static void comando_top(int N);
static void comando_ver_palabras(char **args, int numArgs);
static void comando_mostrar_numpalabras(void);


// ===== EJERCICIO 2 =====
static void comando_mostrar_espacios(void);
// ===== EJERCICIO 3 =====
static void eliminarBloqueDeLista(TBloque *b);
static int comando_recargar(void);
static void eliminarNodoBytes(TBloque *b);
// =========================================================

static int comando_filtro_ver(void);
static int comando_filtro_insertar(char **args, int numArgs);

static int compararStrings(const void *a, const void *b) {
    const char *sa = *(const char **)a;
    const char *sb = *(const char **)b;
    return strcmp(sa, sb);
}

// =========================================================
// FRASES CON PALABRA (NUEVO)
// =========================================================
static void imprimirFrasesQueEmpiezanPor(const char *palabra) {
    if (!bloqueActivo) {
        setTextColor(12);
        printf("No hay bloque activo.\n");
        setTextColor(7);
        return;
    }

    char objetivo[256];
    strncpy(objetivo, palabra, sizeof(objetivo));
    objetivo[sizeof(objetivo)-1] = '\0';
    for (int i = 0; objetivo[i]; i++)
        objetivo[i] = (char)tolower((unsigned char)objetivo[i]);

    char frase[4096];
    int flen = 0;

    for (int fi = 0; fi < bloqueActivo->numFicheros; fi++) {
        FILE *f = fopen(bloqueActivo->ficheros[fi], "rb");
        if (!f) continue;

        int c;
        int prevDot = 0;

        while ((c = fgetc(f)) != EOF) {
            if (c == '.') {
                if (!prevDot) {
                    frase[flen] = '\0';

                    char *p = frase;
                    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
                        p++;

                    char first[256];
                    int k = 0;
                    while (*p && *p!=' ' && *p!='\t' && *p!='\n' && *p!='\r' && k < 255)
                        first[k++] = *p++;
                    first[k] = '\0';

                    for (int i = 0; first[i]; i++)
                        first[i] = (char)tolower((unsigned char)first[i]);

                    if (strcmp(first, objetivo) == 0) {
                        setTextColor(10);
                        printf("%s.\n", frase);
                        setTextColor(7);
                    }

                    flen = 0;
                    prevDot = 1;
                }
            } else {
                prevDot = 0;
                if (flen < (int)sizeof(frase) - 1)
                    frase[flen++] = (char)c;
            }
        }
        fclose(f);
    }
}
// ===== helpers string =====
static char* dupstr(const char *s) {
    size_t n = strlen(s);
    char *p = (char*)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static void tolower_inplace(char *s) {
    for (int i = 0; s[i]; i++) s[i] = (char)tolower((unsigned char)s[i]);
}

static void trim_left(char **p) {
    while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') (*p)++;
}

// =========================================================
// BYTES / ESPACIOS POR BLOQUE
// =========================================================
static TBloqueBytes* getNodoBytes(TBloque *b) {
    TBloqueBytes *aux = listaBytes;
    while (aux) {
        if (aux->bloque == b) return aux;
        aux = aux->sig;
    }
    return NULL;
}

static TBloqueBytes* crearNodoBytesSiNoExiste(TBloque *b) {
    TBloqueBytes *n = getNodoBytes(b);
    if (n) return n;

    n = (TBloqueBytes*)malloc(sizeof(TBloqueBytes));
    if (!n) return NULL;

    n->bloque = b;
    n->bytes = 0;
    n->numEspacios = 0; // ===== EJERCICIO 2 =====
    n->sig = listaBytes;
    listaBytes = n;
    return n;
}

static void setBytesBloque(TBloque *b, long bytes) {
    TBloqueBytes *n = crearNodoBytesSiNoExiste(b);
    if (!n) return;
    n->bytes = bytes;
}

static long getBytesBloque(TBloque *b) {
    TBloqueBytes *n = getNodoBytes(b);
    return n ? n->bytes : 0;
}

// ===== EJERCICIO 2 =====
static void setEspaciosBloque(TBloque *b, long espacios) {
    TBloqueBytes *n = crearNodoBytesSiNoExiste(b);
    if (!n) return;
    n->numEspacios = espacios;
}

static long getEspaciosBloque(TBloque *b) {
    TBloqueBytes *n = getNodoBytes(b);
    return n ? n->numEspacios : 0;
}

static void liberarBytesBloques(void) {
    TBloqueBytes *aux = listaBytes;
    while (aux) {
        TBloqueBytes *sig = aux->sig;
        free(aux);
        aux = sig;
    }
    listaBytes = NULL;
}
static void eliminarNodoBytes(TBloque *b) {
    TBloqueBytes *prev = NULL, *cur = listaBytes;
    while (cur) {
        if (cur->bloque == b) {
            if (!prev) listaBytes = cur->sig;
            else prev->sig = cur->sig;
            free(cur);
            return;
        }
        prev = cur;
        cur = cur->sig;
    }
}
// =========================================================
// COLORES
// =========================================================
void setTextColor(int color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

// =========================================================
// PANTALLA
// =========================================================
void imprimirDatosEstudiante(void) {
    setTextColor(14);
    printf("James Brown \n");
    printf("Estudiante de Ingenieria Informatica de Sistemas\n");
    setTextColor(7);
}

void imprimirPrompt(void) {
    if (bloqueActivo != NULL) {
        setTextColor(7);
        printf(":[%s](%ld)>> ", bloqueActivo->nombre, getBytesBloque(bloqueActivo));
    } else {
        setTextColor(7);
        printf(":>> ");
    }
    fflush(stdout);
}

void imprimirFin(void) {
    setTextColor(10);
    printf("FIN!!\n");
    setTextColor(7);
}

// =========================================================
// DELIMITADORES
// =========================================================
void initDelimitadores(void) {
    numDelims = 0;
    delimitadores[numDelims++] = ' ';
    delimitadores[numDelims++] = '\n';
}

static int esDelimitador(char c) {
    if (c == '\r') return 1;
    for (int i = 0; i < numDelims; i++) {
        if (delimitadores[i] == c) return 1;
    }
    return 0;
}

static int existeDelimitador(char c) {
    for (int i = 0; i < numDelims; i++) {
        if (delimitadores[i] == c) return 1;
    }
    return 0;
}

static void comando_delimitador_ver(void) {
    setTextColor(10);
    for (int i = 0; i < numDelims; i++) {
        char d = delimitadores[i];
        if (d == ' ')      printf("{ESP} ");
        else if (d == '\n') printf("{\\n} ");
        else if (d == '\t') printf("{\\t} ");
        else               printf("%c ", d);
    }
    printf("(%d delimitadores)\n", numDelims);
    setTextColor(7);
}

static int parseDelimitadorToken(const char *token, char *outDelim) {
    if (strcmp(token, "{\\t}") == 0) { *outDelim = '\t'; return 1; }
    if (strcmp(token, "{\\n}") == 0) { *outDelim = '\n'; return 1; }
    if (strcmp(token, "{ESP}") == 0) { *outDelim = ' ';  return 1; }
    if (strlen(token) == 1) { *outDelim = token[0]; return 1; }
    return 0;
}

static int comando_delimitador_insertar(char **args, int numArgs) {
    if (numArgs <= 0) {
        setTextColor(12);
        printf("Falta el delimitador\n");
        setTextColor(7);
        return 0;
    }

    char nuevos[MAX_DELIMS];
    int nNuevos = 0;

    for (int i = 0; i < numArgs; i++) {
        char d;
        if (!parseDelimitadorToken(args[i], &d)) {
            setTextColor(12);
            printf("Falta el delimitador\n");
            setTextColor(7);
            return 0;
        }
        nuevos[nNuevos++] = d;
    }

    for (int i = 0; i < nNuevos; i++) {
        char d = nuevos[i];
        if (!existeDelimitador(d)) {
            if (numDelims < MAX_DELIMS) delimitadores[numDelims++] = d;
        }
    }

    return 1; // OK silencioso
}

// =========================================================
// PALABRAS
// =========================================================
static void normalizarPalabra(char *pal) {
    for (int i = 0; pal[i] != '\0'; i++) pal[i] = (char)tolower((unsigned char)pal[i]);
}

static TNodoPalabra* buscarPalabra(TNodoPalabra *lista, const char *pal) {
    TNodoPalabra *aux = lista;
    while (aux != NULL) {
        if (strcmp(aux->palabra, pal) == 0) return aux;
        aux = aux->sig;
    }
    return NULL;
}

static TNodoPalabra* insertarOIncrementarPalabra(TNodoPalabra **lista, const char *pal) {
    TNodoPalabra *nodo = buscarPalabra(*lista, pal);
    if (nodo != NULL) {
        nodo->contador++;
        return nodo;
    }

    nodo = (TNodoPalabra*)malloc(sizeof(TNodoPalabra));
    if (!nodo) return NULL;

    nodo->palabra = (char*)malloc(strlen(pal) + 1);
    if (!nodo->palabra) { free(nodo); return NULL; }

    strcpy(nodo->palabra, pal);
    nodo->contador = 1;
    nodo->sig = NULL;

    if (*lista == NULL) {
        *lista = nodo;
    } else {
        TNodoPalabra *t = *lista;
        while (t->sig != NULL) t = t->sig;
        t->sig = nodo;
    }

    return nodo;
}

static long contarTotalApariciones(TNodoPalabra *lista) {
    long total = 0;
    while (lista != NULL) {
        total += lista->contador;
        lista = lista->sig;
    }
    return total;
}

static int contarPalabrasDistintas(TNodoPalabra *lista) {
    int c = 0;
    while (lista != NULL) { c++; lista = lista->sig; }
    return c;
}

// =========================================================
// STOP WORDS
// =========================================================
static void liberarStopWords(void) {
    TStopWord *aux = listaStopWords;
    while (aux != NULL) {
        TStopWord *sig = aux->sig;
        free(aux->palabra);
        free(aux);
        aux = sig;
    }
    listaStopWords = NULL;
}

static int existeStopWord(const char *pal) {
    TStopWord *aux = listaStopWords;
    while (aux != NULL) {
        if (strcmp(aux->palabra, pal) == 0) return 1;
        aux = aux->sig;
    }
    return 0;
}

static int cargarStopWordsDesdeFichero(FILE *f) {
    char linea[512];

    while (fgets(linea, sizeof(linea), f) != NULL) {
        char *tok = strtok(linea, " \t\r\n");
        while (tok != NULL) {
            char w[256];
            strncpy(w, tok, sizeof(w));
            w[sizeof(w)-1] = '\0';
            normalizarPalabra(w);

            if (w[0] != '\0' && !existeStopWord(w)) {
                TStopWord *n = (TStopWord*)malloc(sizeof(TStopWord));
                if (!n) return 0;
                n->palabra = (char*)malloc(strlen(w) + 1);
                if (!n->palabra) { free(n); return 0; }
                strcpy(n->palabra, w);
                n->sig = listaStopWords;
                listaStopWords = n;
            }

            tok = strtok(NULL, " \t\r\n");
        }
    }
    return 1;
}

static int cargarStopWordsObligatorio(void) {
    liberarStopWords();

    FILE *f = fopen("_stop_words.txt", "r");
    if (f == NULL) {
        setTextColor(12);
        printf("No se puede leer el fichero: _stop_words.txt\n");
        setTextColor(7);
        return 0;
    }

    int ok = cargarStopWordsDesdeFichero(f);
    fclose(f);
    return ok;
}

static int cargarStopWordsSiExiste(void) {
    liberarStopWords();

    FILE *f = fopen("_stop_words.txt", "r");
    if (f == NULL) return 1;

    int ok = cargarStopWordsDesdeFichero(f);
    fclose(f);
    return ok;
}

static int esStopWord(const char *pal) {
    return existeStopWord(pal);
}

// =========================================================
// BLOQUES
// =========================================================
static TBloque* crearBloqueVacio(const char *nombreBloque) {
    TBloque *b = (TBloque*)malloc(sizeof(TBloque));
    if (!b) return NULL;

    strncpy(b->nombre, nombreBloque, sizeof(b->nombre));
    b->nombre[sizeof(b->nombre)-1] = '\0';

    b->listaPalabras = NULL;
    b->numLineas = 0;
    b->numFrases = 0;
    b->sig = NULL;

   
    b->ficheros = NULL;
    b->numFicheros = 0;
    b->usarStop = 0;

    return b;
}


static void insertarBloqueEnListaAlFinal(TBloque *b) {
    b->sig = NULL;

    if (listaBloques == NULL) {
        listaBloques = b;
    } else {
        TBloque *t = listaBloques;
        while (t->sig != NULL) t = t->sig;
        t->sig = b;
    }

    bloqueActivo = b;
}

static void comando_bloques(void) {
    if (listaBloques == NULL) {
        setTextColor(12);
        printf("No hay bloques cargados.\n");
        setTextColor(7);
        return;
    }

    setTextColor(10);
    TBloque *aux = listaBloques;
    while (aux != NULL) {
        printf("%s (%ld)\n", aux->nombre, getBytesBloque(aux));
        aux = aux->sig;
    }
    setTextColor(7);
}

static int comando_cambiar(const char *nombre) {
    TBloque *aux = listaBloques;
    while (aux != NULL) {
        if (strcmp(aux->nombre, nombre) == 0) {
            bloqueActivo = aux;
            return 1;
        }
        aux = aux->sig;
    }

    setTextColor(12);
    printf("No existe el bloque\n");
    setTextColor(7);
    return 0;
}

// =========================================================
// PROCESADO TEXTO
// =========================================================
static void procesarLineaTexto(TBloque *bloque, const char *linea, int usarStop) {
    char palabra[256];
    int len = 0;

    for (int i = 0; linea[i] != '\0'; i++) {
        char c = linea[i];

        if (!esDelimitador(c)) {
            if (len < (int)sizeof(palabra) - 1) palabra[len++] = c;
        } else {
            if (len > 0) {
                palabra[len] = '\0';
                normalizarPalabra(palabra);

                if (!usarStop || !esStopWord(palabra)) {
                    insertarOIncrementarPalabra(&(bloque->listaPalabras), palabra);
                }
                len = 0;
            }
        }
    }

    if (len > 0) {
        palabra[len] = '\0';
        normalizarPalabra(palabra);

        if (!usarStop || !esStopWord(palabra)) {
            insertarOIncrementarPalabra(&(bloque->listaPalabras), palabra);
        }
    }
}

// =========================================================
// CARGAR  (CORREGIDO EJERCICIO 2 SIN FALLOS)
// =========================================================
static int comando_cargar(char **nombresFicheros, int numFicheros, int usarStop) {
    if (numFicheros <= 0) return 0;

    if (usarStop) {
        if (!cargarStopWordsObligatorio()) return 0;
    } else {
        liberarStopWords();
    }

    TBloque *b = crearBloqueVacio(nombresFicheros[0]);
    if (!b) { liberarStopWords(); return 0; }

    long bytesBloque = 0;
    long espaciosBloque = 0; // ===== EJERCICIO 2 =====
    b->numFicheros = numFicheros;
    b->usarStop = usarStop;
    b->ficheros = (char**)malloc(sizeof(char*) * numFicheros);
    if (!b->ficheros) {
        free(b);
        liberarStopWords();
        return 0;
    }
    for (int i = 0; i < numFicheros; i++) {
        b->ficheros[i] = dupstr(nombresFicheros[i]);
        if (!b->ficheros[i]) {
            for (int j = 0; j < i; j++) free(b->ficheros[j]);
            free(b->ficheros);
            free(b);
            liberarStopWords();
            return 0;
        }
    }
    for (int i = 0; i < numFicheros; i++) {
        FILE *f = fopen(nombresFicheros[i], "rb");
        if (f == NULL) {
            setTextColor(12);
            printf("No se puede leer el fichero: %s\n", nombresFicheros[i]);
            setTextColor(7);

            if (b->ficheros) {
            for (int j = 0; j < b->numFicheros; j++) free(b->ficheros[j]);
            free(b->ficheros);
            }
            free(b);
            liberarStopWords();
            return 0;
        }

        // ===== bytes del fichero (exacto) =====
        if (fseek(f, 0, SEEK_END) == 0) {
            long tam = ftell(f);
            if (tam > 0) bytesBloque += tam;
            fseek(f, 0, SEEK_SET);
        } else {
            fseek(f, 0, SEEK_SET);
        }

        // ===== EJERCICIO 2: contar blancos EXACTO por bytes =====
        int ch;
        while ((ch = fgetc(f)) != EOF) {
            if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
                espaciosBloque++;
            }
        }

        // volver al inicio para procesar líneas/palabras/frases
        rewind(f);

        char linea[1024];
        while (fgets(linea, sizeof(linea), f) != NULL) {
            int lineaVacia = 1;

            // contar frases por racha de puntos
            int prevWasDot = 0;

            for (int j = 0; linea[j] != '\0'; j++) {
                char c = linea[j];

                if (c == '.') {
                    if (!prevWasDot) {
                        b->numFrases++;
                        prevWasDot = 1;
                    }
                } else {
                    prevWasDot = 0;
                }

                if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                    lineaVacia = 0;
                }
            }

            if (!lineaVacia) b->numLineas++;
            procesarLineaTexto(b, linea, usarStop);
        }

        fclose(f);
    }

    insertarBloqueEnListaAlFinal(b);
    setBytesBloque(b, bytesBloque);
    setEspaciosBloque(b, espaciosBloque); // ===== EJERCICIO 2 =====

    liberarStopWords();
    return 1;
}

// =========================================================
// COMANDOS
// =========================================================
static void comando_frases(void) {
    if (!bloqueActivo) {
        setTextColor(12);
        printf("No hay bloque activo.\n");
        setTextColor(7);
        return;
    }
    setTextColor(10);
    printf("%ld\n", bloqueActivo->numFrases);
    setTextColor(7);
}

static void comando_ver_palabras(char **args, int numArgs) {
    if (bloqueActivo == NULL) {
        setTextColor(12);
        printf("No hay bloque activo.\n");
        setTextColor(7);
        return;
    }

    for (int i = 0; i < numArgs; i++) {
        char buf[256];
        strncpy(buf, args[i], sizeof(buf));
        buf[sizeof(buf)-1] = '\0';
        normalizarPalabra(buf);

        TNodoPalabra *n = buscarPalabra(bloqueActivo->listaPalabras, buf);

        setTextColor(10);
        printf("%s: %d\n", buf, (n ? n->contador : 0));
        setTextColor(7);
    }
}

typedef struct {
    TNodoPalabra *n;
    int idx;
} TItem;

static int cmpTop(const void *a, const void *b) {
    const TItem *A = (const TItem*)a;
    const TItem *B = (const TItem*)b;

    if (B->n->contador != A->n->contador)
        return (B->n->contador - A->n->contador);
    return (A->idx - B->idx);
}

static void comando_top(int N) {
    if (bloqueActivo == NULL) {
        setTextColor(12);
        printf("No hay bloque activo.\n");
        setTextColor(7);
        return;
    }

    if (N <= 0) return;

    int total = contarPalabrasDistintas(bloqueActivo->listaPalabras);
    if (total <= 0) return;
    if (N > total) N = total;

    TItem *arr = (TItem*)malloc(total * sizeof(TItem));
    if (!arr) return;

    TNodoPalabra *aux = bloqueActivo->listaPalabras;
    int k = 0;
    while (aux != NULL) {
        arr[k].n = aux;
        arr[k].idx = k;
        k++;
        aux = aux->sig;
    }

    qsort(arr, total, sizeof(TItem), cmpTop);

    setTextColor(10);
    for (int i = 0; i < N; i++) {
        printf("%s: %d\n", arr[i].n->palabra, arr[i].n->contador);
    }
    setTextColor(7);

    free(arr);
}

static void comando_mostrar_numpalabras(void) {
    if (bloqueActivo == NULL) {
        setTextColor(12);
        printf("No hay bloque activo.\n");
        setTextColor(7);
        return;
    }

    long total = contarTotalApariciones(bloqueActivo->listaPalabras);

    setTextColor(10);
    printf("%ld\n", total);
    setTextColor(7);
}

// ===== EJERCICIO 2 =====
static void comando_mostrar_espacios(void) {
    if (bloqueActivo == NULL) {
        setTextColor(12);
        printf("No hay bloque activo.\n");
        setTextColor(7);
        return;
    }

    setTextColor(10);
    printf("%ld\n", getEspaciosBloque(bloqueActivo));
    setTextColor(7);
}
//===== ejercicio 3=====
// Elimina un bloque concreto de la lista (y lo libera)
static void eliminarBloqueDeLista(TBloque *b) {
    if (!b) return;

    TBloque *prev = NULL;
    TBloque *cur = listaBloques;

    while (cur && cur != b) {
        prev = cur;
        cur = cur->sig;
    }
    if (!cur) return;

    // quitar de lista
    if (!prev) listaBloques = cur->sig;
    else prev->sig = cur->sig;

    // liberar palabras
    liberarListaPalabras(cur->listaPalabras);

    // liberar ficheros
    if (cur->ficheros) {
        for (int i = 0; i < cur->numFicheros; i++) free(cur->ficheros[i]);
        free(cur->ficheros);
    }

  
    eliminarNodoBytes(cur);
    free(cur);
}


static void insertarBloqueEnPosicion(TBloque *nuevo, TBloque *prev, TBloque *next) {
    if (!prev) {
        nuevo->sig = listaBloques;
        listaBloques = nuevo;
    } else {
        nuevo->sig = next;
        prev->sig = nuevo;
    }
    bloqueActivo = nuevo;
}

// comando recargar: elimina el bloqueActivo y lo vuelve a cargar (MISMA LOGICA QUE cargar)
static int comando_recargar(void) {
    if (!bloqueActivo) {
        setTextColor(12);
        printf("No hay bloque activo.\n");
        setTextColor(7);
        return 0;
    }

    TBloque *old = bloqueActivo;

    // localizar posición (prev y next) para mantener orden
    TBloque *prev = NULL;
    TBloque *cur = listaBloques;
    while (cur && cur != old) {
        prev = cur;
        cur = cur->sig;
    }
    TBloque *next = (old ? old->sig : NULL);

    int usarStop = old->usarStop;
    int nfiles = old->numFicheros;

    // copiar nombres de ficheros (porque al borrar el bloque se liberan)
    char **files = (char**)malloc(sizeof(char*) * nfiles);
    if (!files) return 0;

    for (int i = 0; i < nfiles; i++) {
        files[i] = dupstr(old->ficheros[i]);
        if (!files[i]) {
            for (int j = 0; j < i; j++) free(files[j]);
            free(files);
            return 0;
        }
    }

    // eliminar bloque viejo
    eliminarBloqueDeLista(old);

    // crear bloque nuevo (sin meterlo al final; lo insertamos en la misma posición)
    TBloque *b = crearBloqueVacio(files[0]);
    if (!b) {
        for (int i = 0; i < nfiles; i++) free(files[i]);
        free(files);
        return 0;
    }

    b->numFicheros = nfiles;
    b->usarStop = usarStop;
    b->ficheros = files;

    // stop words según usarStop
    if (usarStop) {
        if (!cargarStopWordsObligatorio()) {
            eliminarBloqueDeLista(b);
            return 0;
        }
    } else {
        liberarStopWords();
    }

    long bytesBloque = 0;
    long espaciosBloque = 0;

    for (int i = 0; i < nfiles; i++) {
        FILE *f = fopen(b->ficheros[i], "rb");
        if (!f) {
            setTextColor(12);
            printf("No se puede leer el fichero: %s\n", b->ficheros[i]);
            setTextColor(7);

            if (usarStop) liberarStopWords();
            eliminarBloqueDeLista(b);
            return 0;
        }

        // ===== bytes del fichero (exacto) =====
        if (fseek(f, 0, SEEK_END) == 0) {
            long tam = ftell(f);
            if (tam > 0) bytesBloque += tam;
            fseek(f, 0, SEEK_SET);
        } else {
            fseek(f, 0, SEEK_SET);
        }

        // ===== espacios/blancos EXACTO por bytes (igual que en cargar) =====
        int ch;
        while ((ch = fgetc(f)) != EOF) {
            if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
                espaciosBloque++;
            }
        }

        // volver al inicio para procesar líneas/palabras/frases
        rewind(f);

        char linea[1024];
        while (fgets(linea, sizeof(linea), f) != NULL) {
            int lineaVacia = 1;
            int prevWasDot = 0;

            for (int j = 0; linea[j] != '\0'; j++) {
                char c = linea[j];

                // frases (racha de puntos = 1)
                if (c == '.') {
                    if (!prevWasDot) {
                        b->numFrases++;
                        prevWasDot = 1;
                    }
                } else {
                    prevWasDot = 0;
                }

                if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                    lineaVacia = 0;
                }
            }

            if (!lineaVacia) b->numLineas++;
            procesarLineaTexto(b, linea, usarStop);
        }

        fclose(f);
    }

    if (usarStop) liberarStopWords();

    // guardar bytes/espacios en tu estructura externa
    setBytesBloque(b, bytesBloque);
    setEspaciosBloque(b, espaciosBloque);

    // insertar en la misma posición para no cambiar el orden de "bloques"
    insertarBloqueEnPosicion(b, prev, next);

    return 1;
}


// =========================================================
// FILTRO
// =========================================================
static int comando_filtro_ver(void) {
    if (!cargarStopWordsSiExiste()) return 0;

    setTextColor(10);
    TStopWord *aux = listaStopWords;
    int first = 1;
    while (aux != NULL) {
        if (!first) printf(" / ");
        printf("%s", aux->palabra);
        first = 0;
        aux = aux->sig;
    }
    printf("\n");
    setTextColor(7);

    liberarStopWords();
    return 1;
}

static int comando_filtro_insertar(char **args, int numArgs) {
    if (numArgs <= 0) return 0;
    if (!cargarStopWordsSiExiste()) return 0;

    for (int i = 0; i < numArgs; i++) {
        char w[256];
        strncpy(w, args[i], sizeof(w));
        w[sizeof(w)-1] = '\0';
        normalizarPalabra(w);

        if (w[0] == '\0') continue;

        if (!existeStopWord(w)) {
            TStopWord *n = (TStopWord*)malloc(sizeof(TStopWord));
            if (!n) { liberarStopWords(); return 0; }

            n->palabra = (char*)malloc(strlen(w) + 1);
            if (!n->palabra) { free(n); liberarStopWords(); return 0; }

            strcpy(n->palabra, w);
            n->sig = listaStopWords;
            listaStopWords = n;
        }
    }

    FILE *f = fopen("_stop_words.txt", "w");
    if (f == NULL) { liberarStopWords(); return 0; }

    for (TStopWord *aux = listaStopWords; aux != NULL; aux = aux->sig) {
        fprintf(f, "%s\n", aux->palabra);
    }
    fclose(f);

    liberarStopWords();
    return 1;
}

// =========================================================
// LIBERACION
// =========================================================
static void liberarListaPalabras(TNodoPalabra *lista) {
    TNodoPalabra *aux = lista;
    while (aux != NULL) {
        TNodoPalabra *sig = aux->sig;
        free(aux->palabra);
        free(aux);
        aux = sig;
    }
}

static void liberarListaBloques(TBloque *lista) {
    TBloque *aux = lista;
    while (aux != NULL) {
        TBloque *sig = aux->sig;

        liberarListaPalabras(aux->listaPalabras);

        if (aux->ficheros) {
            for (int i = 0; i < aux->numFicheros; i++) free(aux->ficheros[i]);
            free(aux->ficheros);
        }

        free(aux);
        aux = sig;
    }
}

void liberarMemoria(void) {
    liberarListaBloques(listaBloques);
    listaBloques = NULL;
    bloqueActivo = NULL;
    liberarStopWords();
    liberarBytesBloques();
}

// =========================================================
// ANALIZADOR DE COMANDOS
// =========================================================
TEstadoComando analizarComando(const char *command) {
    char buffer[512];
    strncpy(buffer, command, sizeof(buffer));
    buffer[sizeof(buffer)-1] = '\0';

    char *p = buffer;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0') return CMD_OK;

    char *tokens[MAX_TOKENS];
    int ntok = 0;

    char *tok = strtok(p, " \t");
    while (tok != NULL && ntok < MAX_TOKENS) {
        tokens[ntok++] = tok;
        tok = strtok(NULL, " \t");
    }

    if (!strcmp(tokens[0], "salir")) {
        if (ntok != 1) {
            setTextColor(12);
            printf("Numero maximo de argumentos excedido.\n");
            setTextColor(7);
            return CMD_ERROR;
        }
        return CMD_SALIR;
    }

    if (!strcmp(tokens[0], "cargar")) {
        int usarStop = 0;
        int first = 1;

        if (ntok >= 2 && !strcmp(tokens[1], "stop")) {
            usarStop = 1;
            first = 2;
        }

        int nfiles = ntok - first;
        if (nfiles <= 0) {
            setTextColor(12);
            printf("Falta el nombre del fichero\n");
            setTextColor(7);
            return CMD_ERROR;
        }

        return comando_cargar(&tokens[first], nfiles, usarStop) ? CMD_OK : CMD_ERROR;
    }

    if (!strcmp(tokens[0], "delimitador")) {
        if (ntok == 1) {
            setTextColor(12);
            printf("Falta la segunda parte del comando\n");
            setTextColor(7);
            return CMD_ERROR;
        }

        if (!strcmp(tokens[1], "ver")) {
            if (ntok != 2) {
                setTextColor(12);
                printf("Numero maximo de argumentos excedido.\n");
                setTextColor(7);
                return CMD_ERROR;
            }
            comando_delimitador_ver();
            return CMD_OK;
        }

        if (!strcmp(tokens[1], "insertar")) {
            if (ntok < 3) {
                setTextColor(12);
                printf("Falta el delimitador\n");
                setTextColor(7);
                return CMD_ERROR;
            }
            return comando_delimitador_insertar(&tokens[2], ntok - 2) ? CMD_OK : CMD_ERROR;
        }

        setTextColor(12);
        printf("Falta la segunda parte del comando\n");
        setTextColor(7);
        return CMD_ERROR;
    }

    if (!strcmp(tokens[0], "cambiar")) {
        if (ntok != 2) {
            setTextColor(12);
            printf("Falta el nombre del bloque\n");
            setTextColor(7);
            return CMD_ERROR;
        }
        return comando_cambiar(tokens[1]) ? CMD_OK : CMD_ERROR;
    }

    if (!strcmp(tokens[0], "bloques")) {
        if (ntok != 1) {
            setTextColor(12);
            printf("Numero maximo de argumentos excedido.\n");
            setTextColor(7);
            return CMD_ERROR;
        }
        comando_bloques();
        return CMD_OK;
    }

       if (!strcmp(tokens[0], "frases")) {
    if (ntok == 1) {
        comando_frases();                 
        return CMD_OK;
    }
    if (ntok == 2) {
        imprimirFrasesQueEmpiezanPor(tokens[1]);  
        return CMD_OK;
    }
    setTextColor(12);
    printf("Numero maximo de argumentos excedido.\n");
    setTextColor(7);
    return CMD_ERROR;
}


    if (!strcmp(tokens[0], "top")) {
        if (ntok != 2) {
            setTextColor(12);
            printf("Numero maximo de argumentos excedido.\n");
            setTextColor(7);
            return CMD_ERROR;
        }
        comando_top(atoi(tokens[1]));
        return CMD_OK;
    }

    if (!strcmp(tokens[0], "ver")) {
        int ordenar = 0;
        int first = 1;

        if (ntok >= 2 && !strcmp(tokens[1], "_alf")) {
            ordenar = 1;
            first = 2;
        }

        int n = ntok - first;
        if (n <= 0) {
            setTextColor(12);
            printf("Numero maximo de argumentos excedido.\n");
            setTextColor(7);
            return CMD_ERROR;
        }

        if (!ordenar) {
            comando_ver_palabras(&tokens[first], n);
            return CMD_OK;
        }

        char **arr = (char**)malloc(n * sizeof(char*));
        if (!arr) return CMD_ERROR;

        for (int i = 0; i < n; i++) arr[i] = tokens[first + i];
        qsort(arr, n, sizeof(char*), compararStrings);
        comando_ver_palabras(arr, n);
        free(arr);
        return CMD_OK;
    }

    // ===== EJERCICIO 2 =====
    if (!strcmp(tokens[0], "mostrar")) {
        if (ntok == 2 && strcmp(tokens[1], "numpalabras") == 0) {
            comando_mostrar_numpalabras();
            return CMD_OK;
        }
        if (ntok == 2 && strcmp(tokens[1], "espacios") == 0) {
            comando_mostrar_espacios();
            return CMD_OK;
        }

        setTextColor(12);
        printf("Numero maximo de argumentos excedido.\n");
        setTextColor(7);
        return CMD_ERROR;
    }

    if (!strcmp(tokens[0], "filtro")) {
        if (ntok < 2) {
            setTextColor(12);
            printf("Subcomando de filtro desconocido.\n");
            setTextColor(7);
            return CMD_ERROR;
        }

        if (!strcmp(tokens[1], "ver")) {
            if (ntok != 2) {
                setTextColor(12);
                printf("Numero maximo de argumentos excedido.\n");
                setTextColor(7);
                return CMD_ERROR;
            }
            return comando_filtro_ver() ? CMD_OK : CMD_ERROR;
        }

        if (!strcmp(tokens[1], "insertar")) {
            if (ntok < 3) {
                setTextColor(12);
                printf("Numero maximo de argumentos excedido.\n");
                setTextColor(7);
                return CMD_ERROR;
            }
            return comando_filtro_insertar(&tokens[2], ntok - 2) ? CMD_OK : CMD_ERROR;
        }
        

        setTextColor(12);
        printf("Subcomando de filtro desconocido.\n");
        setTextColor(7);
        return CMD_ERROR;
    }
    
    if (!strcmp(tokens[0], "recargar")) {
        if (ntok != 1) {
            setTextColor(12);
            printf("Numero maximo de argumentos excedido.\n");
            setTextColor(7);
            return CMD_ERROR;
        }
        return comando_recargar() ? CMD_OK : CMD_ERROR;
    }
    setTextColor(12);
    printf("Error: comando no valido\n");
    setTextColor(7);
    return CMD_ERROR;
}

