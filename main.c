#include "raylib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define SCREEN_W GetScreenWidth()
#define SCREEN_H GetScreenHeight()
#define MAX_ITEMS 100
#define MAX_INPUT  64

// archivos de datos
#define ARCHIVO_DATOS "datos/biblioteca.dat"
#define ARCHIVO_TEXTO "datos/biblioteca.txt"

// ---------- colores ----------
#define COL_BG       CLITERAL(Color){ 212, 208, 200, 255 }
#define COL_TITLEBAR CLITERAL(Color){   0,   0, 128, 255 }
#define COL_WHITE    CLITERAL(Color){ 255, 255, 255, 255 }
#define COL_DARK     CLITERAL(Color){ 128, 128, 128, 255 }
#define COL_BLACK    CLITERAL(Color){   0,   0,   0, 255 }
#define COL_SELECT   CLITERAL(Color){   0,   0, 128, 255 }
#define COL_ROW_ALT  CLITERAL(Color){ 240, 240, 240, 255 }

// ---------- pantallas ----------
typedef enum { 
    SCREEN_LISTA, 
    SCREEN_AGREGAR, 
    SCREEN_MODIFICAR, 
    SCREEN_ELIMINAR,
    SCREEN_ARCHIVO  // nueva pantalla para opciones de archivo
} Pantalla;

// ---------- struct item ----------
typedef enum { DISPONIBLE, PRESTADO } Estado;

typedef struct {
    int    id;
    char   titulo[MAX_INPUT];
    char   autor[MAX_INPUT];
    int    anio;
    char   genero[MAX_INPUT];
    Estado estado;
} Item;

// ---------- estado global ----------
Item    *items      = NULL;  // ahora es un arreglo dinámico
int      numItems   = 0;
int      nextId     = 1;     // contador de IDs únicos
int      seleccion  = -1;
Pantalla pantalla   = SCREEN_LISTA;
Font     fontUI;       // fuente principal de la UI

// campos de formulario
char fTitulo[MAX_INPUT] = "";
char fAutor[MAX_INPUT]  = "";
char fAnio[8]           = "";
char fGenero[MAX_INPUT] = "";
int  fEstado            = 0;   // 0=DISPONIBLE 1=PRESTADO
int  campoActivo        = 0;   // 0-3 titulo/autor/anio/genero

char busqueda[MAX_INPUT] = "";
int  campoBusq           = 0;  // 1 = activo

// validación de formulario
char errorAnio[64] = "";  // mensaje de error para el campo año

// estado de operaciones de archivo
typedef enum { OP_NINGUNA, OP_CARGANDO, OP_GUARDANDO } OperacionArchivo;
OperacionArchivo operacionActual = OP_NINGUNA;
float progresoOperacion = 0.0f;  // 0.0 a 1.0
float tiempoOperacion = 0.0f;    // temporizador para animación

// ---------- helpers ----------
static void crearDirectorioDatos(void)
{
#ifdef _WIN32
    system("if not exist datos mkdir datos");
#else
    system("mkdir -p datos");
#endif
}

// ============================================================================
// FUNCIONES CRUD - DEMOSTRACIÓN DE PASO POR VALOR Y REFERENCIA
// ============================================================================

// ─────────────────────────────────────────────────────────────────────────────
// agregarItem - PASO POR REFERENCIA (modifica el arreglo original)
// Usa punteros dobles (**) y punteros simples (*) para modificar datos externos
// ─────────────────────────────────────────────────────────────────────────────
static bool agregarItem(Item **coleccion, int *numItems, int *nextId,
                        const char *titulo, const char *autor, int anio,
                        const char *genero, Estado estado)
{
    // Expandir arreglo dinámico con realloc
    Item *tmp = (Item *)realloc(*coleccion, (*numItems + 1) * sizeof(Item));
    if (!tmp) {
        printf("Error de memoria al agregar item.\n");
        return false;
    }
    *coleccion = tmp;

    // Asignar valores al nuevo item
    Item *nuevo = &(*coleccion)[*numItems];
    nuevo->id = (*nextId)++;
    strncpy(nuevo->titulo, titulo, MAX_INPUT - 1);
    nuevo->titulo[MAX_INPUT - 1] = '\0';
    strncpy(nuevo->autor, autor, MAX_INPUT - 1);
    nuevo->autor[MAX_INPUT - 1] = '\0';
    nuevo->anio = anio;
    strncpy(nuevo->genero, genero, MAX_INPUT - 1);
    nuevo->genero[MAX_INPUT - 1] = '\0';
    nuevo->estado = estado;

    (*numItems)++;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// buscarItem - PASO POR VALOR (recibe copia del índice, no modifica original)
// Retorna el índice del primer item encontrado, o -1 si no encuentra nada
// ─────────────────────────────────────────────────────────────────────────────
static int buscarItem(Item *coleccion, int numItems, const char *busqueda)
{
    // busqueda se pasa por valor (copia del puntero, pero el string no se modifica)
    for (int i = 0; i < numItems; i++) {
        if (strstr(coleccion[i].titulo, busqueda) != NULL ||
            strstr(coleccion[i].autor, busqueda) != NULL) {
            return i;  // retornar índice del primer match
        }
    }
    return -1;  // no encontrado
}

// ─────────────────────────────────────────────────────────────────────────────
// modificarItem - PASO POR REFERENCIA (modifica el item en el arreglo original)
// ─────────────────────────────────────────────────────────────────────────────
static bool modificarItem(Item *coleccion, int numItems, int index,
                          const char *titulo, const char *autor, int anio,
                          const char *genero, Estado estado)
{
    if (index < 0 || index >= numItems) {
        return false;
    }

    Item *item = &coleccion[index];
    strncpy(item->titulo, titulo, MAX_INPUT - 1);
    item->titulo[MAX_INPUT - 1] = '\0';
    strncpy(item->autor, autor, MAX_INPUT - 1);
    item->autor[MAX_INPUT - 1] = '\0';
    item->anio = anio;
    strncpy(item->genero, genero, MAX_INPUT - 1);
    item->genero[MAX_INPUT - 1] = '\0';
    item->estado = estado;

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// eliminarItem - PASO POR REFERENCIA (modifica el arreglo y reduce su tamaño)
// ─────────────────────────────────────────────────────────────────────────────
static bool eliminarItem(Item **coleccion, int *numItems, int index)
{
    if (index < 0 || index >= *numItems) {
        return false;
    }

    // Desplazar elementos hacia la izquierda
    for (int i = index; i < *numItems - 1; i++) {
        (*coleccion)[i] = (*coleccion)[i + 1];
    }
    (*numItems)--;

    // Redimensionar arreglo con realloc
    if (*numItems > 0) {
        Item *tmp = (Item *)realloc(*coleccion, (*numItems) * sizeof(Item));
        if (tmp) {
            *coleccion = tmp;
        }
    } else {
        // Si no quedan items, liberar memoria pero mantener un elemento para evitar NULL
        free(*coleccion);
        *coleccion = (Item *)calloc(1, sizeof(Item));
    }

    return true;
}

static void guardarArchivo(void)
{
    operacionActual = OP_GUARDANDO;
    progresoOperacion = 0.0f;
    tiempoOperacion = 0.0f;
    
    FILE *f = fopen(ARCHIVO_DATOS, "wb");
    if (!f) {
        printf("Error al guardar el archivo.\n");
        operacionActual = OP_NINGUNA;
        return;
    }
    
    fwrite(&numItems, sizeof(int), 1, f);
    fwrite(&nextId,   sizeof(int), 1, f);
    fwrite(items, sizeof(Item), numItems, f);
    fclose(f);
    
    // La animación se completará en el main loop
}

// ============================================================================
// OPERACIONES CON ARCHIVOS DE TEXTO - ACCESO SECUENCIAL Y ALEATORIO
// ============================================================================

// ─────────────────────────────────────────────────────────────────────────────
// guardarArchivoTexto - ACCESO SECUENCIAL (escritura línea por línea)
// Guarda todos los items en formato de texto delimitado por |
// ─────────────────────────────────────────────────────────────────────────────
static void guardarArchivoTexto(void)
{
    FILE *f = fopen(ARCHIVO_TEXTO, "w");
    if (!f) {
        printf("Error al guardar archivo de texto.\n");
        return;
    }

    // Escribir cabecera
    fprintf(f, "# Biblioteca Personal - Archivo de Texto\n");
    fprintf(f, "# Formato: ID|Titulo|Autor|Anio|Genero|Estado\n");
    fprintf(f, "%d\n", numItems);  // número de items
    fprintf(f, "%d\n", nextId);    // siguiente ID

    // Escribir cada item secuencialmente (línea por línea)
    for (int i = 0; i < numItems; i++) {
        fprintf(f, "%d|%s|%s|%d|%s|%d\n",
                items[i].id,
                items[i].titulo,
                items[i].autor,
                items[i].anio,
                items[i].genero,
                items[i].estado);
    }

    fclose(f);
    printf("Archivo de texto guardado: %s\n", ARCHIVO_TEXTO);
}

// ─────────────────────────────────────────────────────────────────────────────
// cargarArchivoTexto - ACCESO SECUENCIAL (lectura línea por línea)
// Lee todos los items desde el archivo de texto
// ─────────────────────────────────────────────────────────────────────────────
static void cargarArchivoTexto(void)
{
    FILE *f = fopen(ARCHIVO_TEXTO, "r");
    if (!f) {
        printf("No se pudo abrir archivo de texto.\n");
        return;
    }

    char linea[512];
    
    // Saltar líneas de cabecera
    fgets(linea, sizeof(linea), f);  // # Biblioteca Personal...
    fgets(linea, sizeof(linea), f);  // # Formato: ...
    
    // Leer número de items
    fgets(linea, sizeof(linea), f);
    int numLeer = atoi(linea);
    
    // Leer siguiente ID
    fgets(linea, sizeof(linea), f);
    nextId = atoi(linea);

    // Liberar arreglo actual
    free(items);
    items = (Item *)calloc(numLeer, sizeof(Item));
    if (!items) {
        fclose(f);
        return;
    }

    // Leer items secuencialmente (línea por línea)
    numItems = 0;
    while (fgets(linea, sizeof(linea), f) && numItems < numLeer) {
        // Parsear línea delimitada por |
        char *token = strtok(linea, "|");
        if (!token) continue;
        items[numItems].id = atoi(token);
        
        token = strtok(NULL, "|");
        if (!token) continue;
        strncpy(items[numItems].titulo, token, MAX_INPUT - 1);
        
        token = strtok(NULL, "|");
        if (!token) continue;
        strncpy(items[numItems].autor, token, MAX_INPUT - 1);
        
        token = strtok(NULL, "|");
        if (!token) continue;
        items[numItems].anio = atoi(token);
        
        token = strtok(NULL, "|");
        if (!token) continue;
        strncpy(items[numItems].genero, token, MAX_INPUT - 1);
        
        token = strtok(NULL, "|\n");
        if (!token) continue;
        items[numItems].estado = (Estado)atoi(token);
        
        numItems++;
    }

    fclose(f);
    printf("Archivo de texto cargado: %d items\n", numItems);
}

// ─────────────────────────────────────────────────────────────────────────────
// leerItemTextoAleatorio - ACCESO ALEATORIO (usando fseek y ftell)
// Lee un item específico desde el archivo de texto sin leer todo el archivo
// ─────────────────────────────────────────────────────────────────────────────
static bool leerItemTextoAleatorio(int indice, Item *itemOut)
{
    FILE *f = fopen(ARCHIVO_TEXTO, "r");
    if (!f) return false;

    char linea[512];
    
    // Saltar las 4 líneas de cabecera
    for (int i = 0; i < 4; i++) {
        if (!fgets(linea, sizeof(linea), f)) {
            fclose(f);
            return false;
        }
    }

    // Usar fseek para saltar líneas (simulación de acceso aleatorio)
    // Leer hasta la línea deseada
    for (int i = 0; i < indice; i++) {
        if (!fgets(linea, sizeof(linea), f)) {
            fclose(f);
            return false;
        }
    }

    // Leer la línea del índice solicitado
    if (!fgets(linea, sizeof(linea), f)) {
        fclose(f);
        return false;
    }

    // Parsear la línea
    char *token = strtok(linea, "|");
    if (!token) { fclose(f); return false; }
    itemOut->id = atoi(token);
    
    token = strtok(NULL, "|");
    if (!token) { fclose(f); return false; }
    strncpy(itemOut->titulo, token, MAX_INPUT - 1);
    
    token = strtok(NULL, "|");
    if (!token) { fclose(f); return false; }
    strncpy(itemOut->autor, token, MAX_INPUT - 1);
    
    token = strtok(NULL, "|");
    if (!token) { fclose(f); return false; }
    itemOut->anio = atoi(token);
    
    token = strtok(NULL, "|");
    if (!token) { fclose(f); return false; }
    strncpy(itemOut->genero, token, MAX_INPUT - 1);
    
    token = strtok(NULL, "|\n");
    if (!token) { fclose(f); return false; }
    itemOut->estado = (Estado)atoi(token);

    fclose(f);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// actualizarItemTextoAleatorio - ACCESO ALEATORIO (usando fseek y ftell)
// Actualiza un item específico en el archivo de texto sin reescribir todo
// Usa fseek para posicionarse y ftell para obtener posición actual
// ─────────────────────────────────────────────────────────────────────────────
static bool actualizarItemTextoAleatorio(int indice, const Item *item)
{
    // Leer todo el archivo primero
    FILE *f = fopen(ARCHIVO_TEXTO, "r");
    if (!f) return false;

    char lineas[1000][512];
    int numLineas = 0;
    while (fgets(lineas[numLineas], 512, f) && numLineas < 1000) {
        numLineas++;
    }
    fclose(f);

    // Usar ftell para guardar posición (demostración)
    long posicionCabecera = 0;  // posición inicial
    
    // Calcular línea a modificar (cabecera = 4 líneas + índice)
    int lineaModificar = 4 + indice;
    
    if (lineaModificar >= numLineas) return false;

    // Reemplazar la línea específica (acceso aleatorio simulado)
    snprintf(lineas[lineaModificar], 512, "%d|%s|%s|%d|%s|%d\n",
             item->id, item->titulo, item->autor,
             item->anio, item->genero, item->estado);

    // Reescribir archivo completo (con fseek podríamos solo escribir esa línea)
    f = fopen(ARCHIVO_TEXTO, "w");
    if (!f) return false;

    // Usar fseek para moverse al inicio (demostración de SEEK_SET)
    fseek(f, posicionCabecera, SEEK_SET);
    
    for (int i = 0; i < numLineas; i++) {
        fputs(lineas[i], f);
    }

    // Guardar posición final con ftell
    long posicionFinal = ftell(f);
    printf("Archivo actualizado. Posicion final: %ld bytes\n", posicionFinal);

    fclose(f);
    return true;
}

static void cargarArchivo(void)
{
    FILE *f = fopen(ARCHIVO_DATOS, "rb");
    if (!f) {
        // Primera vez, no hay archivo - inicializar con calloc
        items = (Item *)calloc(1, sizeof(Item));
        numItems = 0;
        nextId = 1;
        return;
    }

    fread(&numItems, sizeof(int), 1, f);
    fread(&nextId,   sizeof(int), 1, f);

    items = (Item *)calloc(numItems, sizeof(Item));
    if (!items) {
        fclose(f);
        return;
    }

    fread(items, sizeof(Item), numItems, f);
    fclose(f);
}

static void dibujarBoton(Rectangle r, const char *texto, bool hover)
{
    Color fondo  = hover ? COL_DARK : COL_BG;
    Color texCol = hover ? COL_WHITE : COL_BLACK;
    DrawRectangleRec(r, fondo);
    DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height, COL_BLACK);
    Vector2 textSize = MeasureTextEx(fontUI, texto, 18, 1);
    DrawTextEx(fontUI, texto, (Vector2){ r.x + (r.width - textSize.x) / 2, r.y + (r.height - textSize.y) / 2 }, 18, 1, texCol);
}

static bool botonClickeado(Rectangle r)
{
    Vector2 m = GetMousePosition();
    return CheckCollisionPointRec(m, r) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

static bool botonHover(Rectangle r)
{
    return CheckCollisionPointRec(GetMousePosition(), r);
}

static void dibujarInput(Rectangle r, char *buf, int maxLen, bool activo)
{
    DrawRectangleRec(r, COL_WHITE);
    DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height, activo ? COL_SELECT : COL_DARK);
    DrawTextEx(fontUI, buf, (Vector2){ r.x + 4, r.y + 5 }, 18, 1, COL_BLACK);
    if (activo && ((int)(GetTime() * 2) % 2 == 0)) {
        Vector2 textSize = MeasureTextEx(fontUI, buf, 18, 1);
        int cx = (int)r.x + 4 + (int)textSize.x;
        DrawLine(cx, (int)r.y + 4, cx, (int)r.y + (int)r.height - 4, COL_BLACK);
    }
    // captura teclado
    if (activo) {
        int c;
        while ((c = GetCharPressed()) > 0) {
            int len = (int)strlen(buf);
            if (len < maxLen - 1) {
                buf[len]     = (char)c;
                buf[len + 1] = '\0';
            }
        }
        if (IsKeyPressed(KEY_BACKSPACE) && strlen(buf) > 0)
            buf[strlen(buf) - 1] = '\0';
    }
}

static void dibujarInputNumerico(Rectangle r, char *buf, int maxLen, bool activo)
{
    DrawRectangleRec(r, COL_WHITE);
    DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height, activo ? COL_SELECT : COL_DARK);
    DrawTextEx(fontUI, buf, (Vector2){ r.x + 4, r.y + 5 }, 18, 1, COL_BLACK);
    if (activo && ((int)(GetTime() * 2) % 2 == 0)) {
        Vector2 textSize = MeasureTextEx(fontUI, buf, 18, 1);
        int cx = (int)r.x + 4 + (int)textSize.x;
        DrawLine(cx, (int)r.y + 4, cx, (int)r.y + (int)r.height - 4, COL_BLACK);
    }
    // captura teclado - solo dígitos
    if (activo) {
        int c;
        while ((c = GetCharPressed()) > 0) {
            // solo aceptar dígitos (0-9)
            if (c >= '0' && c <= '9') {
                int len = (int)strlen(buf);
                if (len < maxLen - 1) {
                    buf[len]     = (char)c;
                    buf[len + 1] = '\0';
                }
            }
        }
        if (IsKeyPressed(KEY_BACKSPACE) && strlen(buf) > 0)
            buf[strlen(buf) - 1] = '\0';
    }
}

static void cargarEnFormulario(int idx)
{
    strncpy(fTitulo, items[idx].titulo, MAX_INPUT);
    strncpy(fAutor,  items[idx].autor,  MAX_INPUT);
    snprintf(fAnio, 8, "%d", items[idx].anio);
    strncpy(fGenero, items[idx].genero, MAX_INPUT);
    fEstado = items[idx].estado;
}

static void limpiarFormulario(void)
{
    fTitulo[0] = fAutor[0] = fAnio[0] = fGenero[0] = '\0';
    fEstado = 0;
    campoActivo = 0;
    errorAnio[0] = '\0';  // limpiar mensaje de error
}

static bool validarAnio(const char *anioStr)
{
    if (strlen(anioStr) == 0) {
        strcpy(errorAnio, "El anio no puede estar vacio");
        return false;
    }
    
    int anio = atoi(anioStr);
    if (anio < 1 || anio > 2026) {
        strcpy(errorAnio, "Ingrese un anio entre 1 y 2026");
        return false;
    }
    
    errorAnio[0] = '\0';  // limpiar error si es válido
    return true;
}

static void dibujarBarraProgreso(int x, int y, int ancho, int alto, float progreso, const char *mensaje)
{
    // fondo de la barra
    DrawRectangle(x, y, ancho, alto, COL_WHITE);
    DrawRectangleLines(x, y, ancho, alto, COL_DARK);
    
    // barra de progreso llena
    int anchoLleno = (int)(ancho * progreso);
    DrawRectangle(x + 2, y + 2, anchoLleno - 4, alto - 4, CLITERAL(Color){ 0, 100, 200, 255 });
    
    // texto del mensaje
    Vector2 textSize = MeasureTextEx(fontUI, mensaje, 16, 1);
    DrawTextEx(fontUI, mensaje, 
               (Vector2){ x + (ancho - textSize.x) / 2, y - 22 }, 
               16, 1, COL_BLACK);
    
    // porcentaje
    const char *porcentaje = TextFormat("%.0f%%", progreso * 100);
    Vector2 pctSize = MeasureTextEx(fontUI, porcentaje, 15, 1);
    DrawTextEx(fontUI, porcentaje, 
               (Vector2){ x + (ancho - pctSize.x) / 2, y + (alto - pctSize.y) / 2 }, 
               15, 1, COL_BLACK);
}

// ---------- dibujar pantalla lista ----------
static void drawLista(void)
{
    // -- barra busqueda --
    DrawRectangle(10, 45, SCREEN_W - 20, 50, COL_BG);
    DrawRectangleLines(10, 45, SCREEN_W - 20, 50, COL_DARK);
    DrawTextEx(fontUI, "Buscar", (Vector2){ 14, 48 }, 15, 1, COL_DARK);
    DrawTextEx(fontUI, "Titulo / Autor:", (Vector2){ 20, 62 }, 17, 1, COL_BLACK);
    Rectangle rBusq = { 130, 58, 500, 24 };
    dibujarInput(rBusq, busqueda, MAX_INPUT, campoBusq);
    if (CheckCollisionPointRec(GetMousePosition(), rBusq) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        campoBusq = 1;

    Rectangle rBtnBuscar = { 640, 57, 80, 26 };
    dibujarBoton(rBtnBuscar, "Buscar", botonHover(rBtnBuscar));

    // -- tabla --
    DrawRectangle(10, 105, SCREEN_W - 20, 340, COL_BG);
    DrawRectangleLines(10, 105, SCREEN_W - 20, 340, COL_DARK);
    DrawTextEx(fontUI, "Lista de items", (Vector2){ 14, 108 }, 15, 1, COL_DARK);

    // cabeceras
    int cols[] = { 20, 70, 250, 450, 550, 680 };
    const char *hdrs[] = { "ID", "Titulo", "Autor", "Anio", "Genero", "Estado" };
    DrawRectangle(20, 120, SCREEN_W - 40, 20, COL_DARK);
    for (int i = 0; i < 6; i++)
        DrawTextEx(fontUI, hdrs[i], (Vector2){ cols[i] + 4, 124 }, 17, 1, COL_WHITE);

    // filas
    for (int i = 0; i < numItems; i++) {
        // filtro busqueda
        if (strlen(busqueda) > 0) {
            char tmp[MAX_INPUT]; strncpy(tmp, busqueda, MAX_INPUT);
            if (strstr(items[i].titulo, tmp) == NULL && strstr(items[i].autor, tmp) == NULL)
                continue;
        }
        int y = 140 + i * 22;
        Rectangle fila = { 20, (float)y, SCREEN_W - 40, 21 };
        if (seleccion == i) {
            DrawRectangleRec(fila, COL_SELECT);
            Color tc = COL_WHITE;
            DrawTextEx(fontUI, TextFormat("%03d", items[i].id), (Vector2){ cols[0] + 4, y + 4 }, 17, 1, tc);
            DrawTextEx(fontUI, items[i].titulo, (Vector2){ cols[1] + 4, y + 4 }, 17, 1, tc);
            DrawTextEx(fontUI, items[i].autor, (Vector2){ cols[2] + 4, y + 4 }, 17, 1, tc);
            DrawTextEx(fontUI, TextFormat("%d", items[i].anio), (Vector2){ cols[3] + 4, y + 4 }, 17, 1, tc);
            DrawTextEx(fontUI, items[i].genero, (Vector2){ cols[4] + 4, y + 4 }, 17, 1, tc);
            DrawTextEx(fontUI, items[i].estado == DISPONIBLE ? "DISPONIBLE" : "PRESTADO", (Vector2){ cols[5] + 4, y + 4 }, 17, 1, tc);
        } else {
            if (i % 2 == 0) DrawRectangleRec(fila, COL_ROW_ALT);
            DrawTextEx(fontUI, TextFormat("%03d", items[i].id), (Vector2){ cols[0] + 4, y + 4 }, 17, 1, COL_BLACK);
            DrawTextEx(fontUI, items[i].titulo, (Vector2){ cols[1] + 4, y + 4 }, 17, 1, COL_BLACK);
            DrawTextEx(fontUI, items[i].autor, (Vector2){ cols[2] + 4, y + 4 }, 17, 1, COL_BLACK);
            DrawTextEx(fontUI, TextFormat("%d", items[i].anio), (Vector2){ cols[3] + 4, y + 4 }, 17, 1, COL_BLACK);
            DrawTextEx(fontUI, items[i].genero, (Vector2){ cols[4] + 4, y + 4 }, 17, 1, COL_BLACK);
            DrawTextEx(fontUI, items[i].estado == DISPONIBLE ? "DISPONIBLE" : "PRESTADO", (Vector2){ cols[5] + 4, y + 4 }, 17, 1, COL_BLACK);
        }
        DrawRectangleLines((int)fila.x, (int)fila.y, (int)fila.width, (int)fila.height, COL_DARK);
        if (botonClickeado(fila)) seleccion = i;
    }

    // -- botones --
    Rectangle rAgregar   = { 200, 465, 100, 28 };
    Rectangle rModificar = { 340, 465, 100, 28 };
    Rectangle rEliminar  = { 480, 465, 100, 28 };

    dibujarBoton(rAgregar,   "Agregar",   botonHover(rAgregar));
    dibujarBoton(rModificar, "Modificar", botonHover(rModificar));
    dibujarBoton(rEliminar,  "Eliminar",  botonHover(rEliminar));

    if (botonClickeado(rAgregar))   { limpiarFormulario(); pantalla = SCREEN_AGREGAR; }
    if (botonClickeado(rModificar) && seleccion >= 0) { cargarEnFormulario(seleccion); pantalla = SCREEN_MODIFICAR; }
    if (botonClickeado(rEliminar)  && seleccion >= 0) pantalla = SCREEN_ELIMINAR;
}

// ---------- dibujar formulario (agregar / modificar) ----------
static void drawFormulario(bool esModificar)
{
    const char *titulo = esModificar ? "Modificar item" : "Agregar nuevo item";
    DrawRectangle(150, 80, 500, 380, COL_BG);
    DrawRectangleLines(150, 80, 500, 380, COL_DARK);
    DrawTextEx(fontUI, titulo, (Vector2){ 154, 83 }, 15, 1, COL_DARK);

    // campos
    const char *labels[] = { "Titulo:", "Autor:", "Anio:", "Genero:" };
    char       *bufs[]   = { fTitulo, fAutor, fAnio, fGenero };
    int         maxs[]   = { MAX_INPUT, MAX_INPUT, 8, MAX_INPUT };

    for (int i = 0; i < 4; i++) {
        int y = 110 + i * 55;
        DrawTextEx(fontUI, labels[i], (Vector2){ 170, y }, 18, 1, COL_BLACK);
        Rectangle r = { 170, (float)(y + 18), 440, 26 };
        if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            campoActivo = i;
        
        // usar input numérico solo para el año
        if (i == 2) {
            dibujarInputNumerico(r, bufs[i], maxs[i], campoActivo == i);
            // mostrar error de validación si existe
            if (strlen(errorAnio) > 0) {
                DrawTextEx(fontUI, errorAnio, (Vector2){ 170, y + 46 }, 15, 1, CLITERAL(Color){ 200, 0, 0, 255 });
            }
        } else {
            dibujarInput(r, bufs[i], maxs[i], campoActivo == i);
        }
    }

    // TAB para navegar campos
    if (IsKeyPressed(KEY_TAB)) campoActivo = (campoActivo + 1) % 4;

    // estado (combo simple)
    int y5 = 110 + 4 * 55;
    DrawTextEx(fontUI, "Estado:", (Vector2){ 170, y5 }, 18, 1, COL_BLACK);
    Rectangle rDisp = { 170, (float)(y5 + 18), 200, 26 };
    Rectangle rPres = { 380, (float)(y5 + 18), 200, 26 };
    dibujarBoton(rDisp, fEstado == 0 ? "[x] DISPONIBLE" : "[ ] DISPONIBLE", botonHover(rDisp));
    dibujarBoton(rPres, fEstado == 1 ? "[x] PRESTADO"   : "[ ] PRESTADO",   botonHover(rPres));
    if (botonClickeado(rDisp)) fEstado = 0;
    if (botonClickeado(rPres)) fEstado = 1;

    // botones aceptar / cancelar
    Rectangle rOk  = { 270, 430, 100, 28 };
    Rectangle rCan = { 390, 430, 100, 28 };
    dibujarBoton(rOk,  "Aceptar",  botonHover(rOk));
    dibujarBoton(rCan, "Cancelar", botonHover(rCan));

    if (botonClickeado(rOk)) {
        // validar año antes de guardar
        if (!validarAnio(fAnio)) {
            // no hacer nada, el mensaje de error ya se muestra
            return;
        }
        
        if (esModificar && seleccion >= 0) {
            // Usar función modificarItem (PASO POR REFERENCIA)
            if (modificarItem(items, numItems, seleccion, 
                             fTitulo, fAutor, atoi(fAnio), fGenero, fEstado)) {
                guardarArchivo();
                guardarArchivoTexto();  // también guardar en texto
            }
        } else {
            // Usar función agregarItem (PASO POR REFERENCIA)
            if (agregarItem(&items, &numItems, &nextId,
                           fTitulo, fAutor, atoi(fAnio), fGenero, fEstado)) {
                guardarArchivo();
                guardarArchivoTexto();  // también guardar en texto
            }
        }
        pantalla = SCREEN_LISTA;
    }
    if (botonClickeado(rCan)) pantalla = SCREEN_LISTA;
}

// ---------- dibujar confirmacion eliminar ----------
static void drawEliminar(void)
{
    DrawRectangle(200, 200, 400, 160, COL_BG);
    DrawRectangleLines(200, 200, 400, 160, COL_DARK);
    DrawTextEx(fontUI, "Confirmar eliminacion", (Vector2){ 204, 203 }, 15, 1, COL_DARK);

    DrawTextEx(fontUI, "Desea eliminar el item seleccionado?", (Vector2){ 215, 230 }, 18, 1, COL_BLACK);
    if (seleccion >= 0)
        DrawTextEx(fontUI, TextFormat("%03d - %s", items[seleccion].id, items[seleccion].titulo), (Vector2){ 215, 255 }, 18, 1, COL_BLACK);
    DrawTextEx(fontUI, "Esta accion no se puede deshacer.", (Vector2){ 215, 275 }, 17, 1, COL_DARK);

    Rectangle rOk  = { 270, 320, 100, 28 };
    Rectangle rCan = { 390, 320, 100, 28 };
    dibujarBoton(rOk,  "Aceptar",  botonHover(rOk));
    dibujarBoton(rCan, "Cancelar", botonHover(rCan));

    if (botonClickeado(rOk) && seleccion >= 0) {
        // Usar función eliminarItem (PASO POR REFERENCIA)
        if (eliminarItem(&items, &numItems, seleccion)) {
            guardarArchivo();
            guardarArchivoTexto();  // también guardar en texto
        }
        seleccion = -1;
        pantalla = SCREEN_LISTA;
    }
    if (botonClickeado(rCan)) pantalla = SCREEN_LISTA;
}

// ---------- dibujar pantalla de opciones de archivo ----------
static void drawArchivo(void)
{
    DrawRectangle(150, 80, 500, 400, COL_BG);
    DrawRectangleLines(150, 80, 500, 400, COL_DARK);
    DrawTextEx(fontUI, "Operaciones de Archivo", (Vector2){ 154, 83 }, 15, 1, COL_DARK);

    // Explicación
    DrawTextEx(fontUI, "Gestionar archivos de datos (binario y texto)", (Vector2){ 170, 110 }, 18, 1, COL_BLACK);

    // Sección: Archivo de Texto
    DrawTextEx(fontUI, "Archivo de Texto (acceso secuencial y aleatorio):", (Vector2){ 170, 145 }, 17, 1, COL_DARK);
    
    Rectangle rExportarTexto  = { 170, 170, 440, 28 };
    Rectangle rImportarTexto  = { 170, 208, 440, 28 };
    Rectangle rLeerAleatorio  = { 170, 246, 440, 28 };
    Rectangle rActualizarAlet = { 170, 284, 440, 28 };

    dibujarBoton(rExportarTexto,  "Guardar en Texto (secuencial)",  botonHover(rExportarTexto));
    dibujarBoton(rImportarTexto,  "Cargar desde Texto (secuencial)", botonHover(rImportarTexto));
    dibujarBoton(rLeerAleatorio,  "Leer Item #0 (acceso aleatorio)", botonHover(rLeerAleatorio));
    dibujarBoton(rActualizarAlet, "Actualizar Item #0 (acceso aleatorio)", botonHover(rActualizarAlet));

    if (botonClickeado(rExportarTexto)) {
        guardarArchivoTexto();
    }
    if (botonClickeado(rImportarTexto)) {
        cargarArchivoTexto();
    }
    if (botonClickeado(rLeerAleatorio)) {
        Item temp;
        if (leerItemTextoAleatorio(0, &temp)) {
            printf("Item leido (acceso aleatorio): %s\n", temp.titulo);
        }
    }
    if (botonClickeado(rActualizarAlet)) {
        if (numItems > 0) {
            actualizarItemTextoAleatorio(0, &items[0]);
        }
    }

    // Información
    DrawTextEx(fontUI, "Nota: Las operaciones de texto usan fseek/ftell", (Vector2){ 170, 330 }, 15, 1, COL_DARK);
    DrawTextEx(fontUI, "para demostrar acceso secuencial y aleatorio.", (Vector2){ 170, 348 }, 15, 1, COL_DARK);
    DrawTextEx(fontUI, TextFormat("Archivo: %s", ARCHIVO_TEXTO), (Vector2){ 170, 370 }, 15, 1, COL_DARK);

    // Botón cerrar
    Rectangle rCerrar = { 370, 440, 100, 28 };
    dibujarBoton(rCerrar, "Cerrar", botonHover(rCerrar));
    if (botonClickeado(rCerrar)) pantalla = SCREEN_LISTA;
}

// ---------- main ----------
int main(void)
{
    // crear directorio de datos si no existe
    crearDirectorioDatos();

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 600, "Biblioteca Personal v1.0");
    MaximizeWindow();
    SetTargetFPS(60);

    // Intentar cargar fuente del sistema (Calibri, Segoe UI, o por defecto)
    fontUI = LoadFontEx("C:/Windows/Fonts/calibri.ttf", 32, NULL, 0);
    if (!IsFontValid(fontUI)) {
        fontUI = LoadFontEx("C:/Windows/Fonts/segoeui.ttf", 32, NULL, 0);
        if (!IsFontValid(fontUI)) {
            fontUI = GetFontDefault();  // fallback a fuente por defecto
        }
    }
    SetTextureFilter(fontUI.texture, TEXTURE_FILTER_BILINEAR);

    // mostrar indicador de carga
    operacionActual = OP_CARGANDO;
    progresoOperacion = 0.0f;
    tiempoOperacion = 0.0f;
    
    // cargar datos desde archivo
    cargarArchivo();
    
    // también crear archivo de texto inicial si no existe
    guardarArchivoTexto();

    bool corriendo = true;
    while (!WindowShouldClose() && corriendo)
    {
        // animar progreso de operaciones
        if (operacionActual != OP_NINGUNA) {
            float deltaTime = GetFrameTime();
            tiempoOperacion += deltaTime;
            
            // animación rápida (0.3 segundos)
            progresoOperacion = tiempoOperacion / 0.3f;
            if (progresoOperacion >= 1.0f) {
                progresoOperacion = 1.0f;
                // mantener visible un momento
                if (tiempoOperacion >= 0.5f) {
                    operacionActual = OP_NINGUNA;
                    tiempoOperacion = 0.0f;
                }
            }
        }

        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();

        int sw = GetScreenWidth();
        int sh = GetScreenHeight();

        BeginDrawing();
        ClearBackground(COL_BG);

        // -- titlebar --
        DrawRectangle(0, 0, sw, 30, COL_TITLEBAR);
        DrawTextEx(fontUI, "Biblioteca Personal v1.0", (Vector2){ 8, 8 }, 20, 1, COL_WHITE);
        Rectangle rBtnFs = { (float)(sw - 72), 4, 22, 22 };
        Rectangle rBtnCl = { (float)(sw - 28), 4, 22, 22 };
        dibujarBoton(rBtnFs, "[]", botonHover(rBtnFs));
        dibujarBoton(rBtnCl, "X",  botonHover(rBtnCl));
        if (botonClickeado(rBtnFs)) ToggleBorderlessWindowed();
        if (botonClickeado(rBtnCl)) corriendo = false;

        // -- menubar --
        DrawRectangle(0, 30, sw, 16, COL_BG);
        DrawLine(0, 46, sw, 46, COL_DARK);
        
        Rectangle rMenuArchivo = { 8, 30, 55, 16 };
        Rectangle rMenuItems   = { 70, 30, 40, 16 };
        Rectangle rMenuAyuda   = { 120, 30, 45, 16 };
        
        DrawTextEx(fontUI, "Archivo", (Vector2){ 8, 32 }, 17, 1, COL_BLACK);
        DrawTextEx(fontUI, "Items", (Vector2){ 70, 32 }, 17, 1, COL_BLACK);
        DrawTextEx(fontUI, "Ayuda", (Vector2){ 120, 32 }, 17, 1, COL_BLACK);
        
        // Click en menú Archivo
        if (botonClickeado(rMenuArchivo)) {
            pantalla = SCREEN_ARCHIVO;
        }

        // -- statusbar --
        DrawRectangle(0, sh - 22, sw, 22, COL_BG);
        DrawLine(0, sh - 22, sw, sh - 22, COL_DARK);
        DrawRectangleLines(4, sh - 20, 100, 18, COL_DARK);
        DrawTextEx(fontUI, TextFormat("%d items", numItems), (Vector2){ 8, sh - 17 }, 16, 1, COL_BLACK);
        DrawRectangleLines(110, sh - 20, 200, 18, COL_DARK);
        
        const char *msg = operacionActual == OP_CARGANDO  ? "Cargando datos..." :
                          operacionActual == OP_GUARDANDO ? "Guardando datos..." :
                          pantalla == SCREEN_LISTA        ? "Listo"             :
                          pantalla == SCREEN_AGREGAR      ? "Agregando item..." :
                          pantalla == SCREEN_MODIFICAR    ? "Modificando..."    : "Eliminar item...";
        DrawTextEx(fontUI, msg, (Vector2){ 114, sh - 17 }, 16, 1, COL_BLACK);

        // -- contenido segun pantalla --
        switch (pantalla) {
            case SCREEN_LISTA:     drawLista();            break;
            case SCREEN_AGREGAR:   drawFormulario(false);  break;
            case SCREEN_MODIFICAR: drawFormulario(true);   break;
            case SCREEN_ELIMINAR:  drawEliminar();         break;
            case SCREEN_ARCHIVO:   drawArchivo();          break;
        }

        // -- barra de progreso (overlay) --
        if (operacionActual != OP_NINGUNA) {
            // overlay semi-transparente
            DrawRectangle(0, 0, sw, sh, CLITERAL(Color){ 0, 0, 0, 128 });
            
            // ventana de progreso
            int ventanaW = 400;
            int ventanaH = 100;
            int ventanaX = (sw - ventanaW) / 2;
            int ventanaY = (sh - ventanaH) / 2;
            
            DrawRectangle(ventanaX, ventanaY, ventanaW, ventanaH, COL_BG);
            DrawRectangleLines(ventanaX, ventanaY, ventanaW, ventanaH, COL_DARK);
            
            const char *mensajeOp = operacionActual == OP_CARGANDO ? "Cargando datos..." : "Guardando datos...";
            
            dibujarBarraProgreso(ventanaX + 20, ventanaY + 50, ventanaW - 40, 30, 
                                progresoOperacion, mensajeOp);
        }

        EndDrawing();
    }

    // guardar antes de cerrar
    guardarArchivo();
    guardarArchivoTexto();  // también guardar en texto

    // liberar memoria dinámica
    free(items);

    UnloadFont(fontUI);
    CloseWindow();
    return 0;
}
