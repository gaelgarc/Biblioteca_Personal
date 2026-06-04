#include "raylib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define SCREEN_W GetScreenWidth()
#define SCREEN_H GetScreenHeight()
#define MAX_ITEMS 100
#define MAX_INPUT  64

// archivo de datos
#define ARCHIVO_DATOS "datos/biblioteca.dat"

// ---------- colores ----------
#define COL_BG       CLITERAL(Color){ 212, 208, 200, 255 }
#define COL_TITLEBAR CLITERAL(Color){   0,   0, 128, 255 }
#define COL_WHITE    CLITERAL(Color){ 255, 255, 255, 255 }
#define COL_DARK     CLITERAL(Color){ 128, 128, 128, 255 }
#define COL_BLACK    CLITERAL(Color){   0,   0,   0, 255 }
#define COL_SELECT   CLITERAL(Color){   0,   0, 128, 255 }
#define COL_ROW_ALT  CLITERAL(Color){ 240, 240, 240, 255 }

// ---------- pantallas ----------
typedef enum { SCREEN_LISTA, SCREEN_AGREGAR, SCREEN_MODIFICAR, SCREEN_ELIMINAR } Pantalla;

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

// ---------- helpers ----------
static void crearDirectorioDatos(void)
{
#ifdef _WIN32
    system("if not exist datos mkdir datos");
#else
    system("mkdir -p datos");
#endif
}

static void guardarArchivo(void)
{
    FILE *f = fopen(ARCHIVO_DATOS, "wb");
    if (!f) {
        printf("Error al guardar el archivo.\n");
        return;
    }
    fwrite(&numItems, sizeof(int), 1, f);
    fwrite(&nextId,   sizeof(int), 1, f);
    fwrite(items, sizeof(Item), numItems, f);
    fclose(f);
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
    int cols[] = { 20, 80, 300, 480, 550, 650 };
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
        DrawRectangleLines((int)fila.x, (int)fila.y, (int)fila.width, (int)fila.height, COL_ROW_ALT);
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
            strncpy(items[seleccion].titulo, fTitulo, MAX_INPUT);
            strncpy(items[seleccion].autor,  fAutor,  MAX_INPUT);
            items[seleccion].anio   = atoi(fAnio);
            strncpy(items[seleccion].genero, fGenero, MAX_INPUT);
            items[seleccion].estado = fEstado;
            guardarArchivo();  // guardar después de modificar
        } else {
            // agregar nuevo item - expandir arreglo dinámico
            Item *tmp = (Item *)realloc(items, (numItems + 1) * sizeof(Item));
            if (!tmp) {
                printf("Error de memoria al agregar item.\n");
                pantalla = SCREEN_LISTA;
                return;
            }
            items = tmp;

            items[numItems].id = nextId++;
            strncpy(items[numItems].titulo, fTitulo, MAX_INPUT);
            strncpy(items[numItems].autor,  fAutor,  MAX_INPUT);
            items[numItems].anio   = atoi(fAnio);
            strncpy(items[numItems].genero, fGenero, MAX_INPUT);
            items[numItems].estado = fEstado;
            numItems++;
            guardarArchivo();  // guardar después de agregar
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
        // desplazar elementos hacia la izquierda
        for (int i = seleccion; i < numItems - 1; i++)
            items[i] = items[i + 1];
        numItems--;

        // redimensionar arreglo
        Item *tmp = (Item *)realloc(items, numItems * sizeof(Item));
        if (tmp || numItems == 0)
            items = tmp;

        guardarArchivo();  // guardar después de eliminar
        seleccion = -1;
        pantalla = SCREEN_LISTA;
    }
    if (botonClickeado(rCan)) pantalla = SCREEN_LISTA;
}

// ---------- main ----------
int main(void)
{
    // crear directorio de datos si no existe
    crearDirectorioDatos();

    // cargar datos desde archivo
    cargarArchivo();

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

    bool corriendo = true;
    while (!WindowShouldClose() && corriendo)
    {
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
        DrawTextEx(fontUI, "Archivo", (Vector2){ 8, 32 }, 17, 1, COL_BLACK);
        DrawTextEx(fontUI, "Items", (Vector2){ 70, 32 }, 17, 1, COL_BLACK);
        DrawTextEx(fontUI, "Ayuda", (Vector2){ 120, 32 }, 17, 1, COL_BLACK);

        // -- statusbar --
        DrawRectangle(0, sh - 22, sw, 22, COL_BG);
        DrawLine(0, sh - 22, sw, sh - 22, COL_DARK);
        DrawRectangleLines(4, sh - 20, 100, 18, COL_DARK);
        DrawTextEx(fontUI, TextFormat("%d items", numItems), (Vector2){ 8, sh - 17 }, 16, 1, COL_BLACK);
        DrawRectangleLines(110, sh - 20, 200, 18, COL_DARK);
        const char *msg = pantalla == SCREEN_LISTA     ? "Listo"             :
                          pantalla == SCREEN_AGREGAR   ? "Agregando item..." :
                          pantalla == SCREEN_MODIFICAR ? "Modificando..."    : "Eliminar item...";
        DrawTextEx(fontUI, msg, (Vector2){ 114, sh - 17 }, 16, 1, COL_BLACK);

        // -- contenido segun pantalla --
        switch (pantalla) {
            case SCREEN_LISTA:     drawLista();            break;
            case SCREEN_AGREGAR:   drawFormulario(false);  break;
            case SCREEN_MODIFICAR: drawFormulario(true);   break;
            case SCREEN_ELIMINAR:  drawEliminar();         break;
        }

        EndDrawing();
    }

    // guardar antes de cerrar
    guardarArchivo();

    // liberar memoria dinámica
    free(items);

    UnloadFont(fontUI);
    CloseWindow();
    return 0;
}
