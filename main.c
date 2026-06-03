#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#ifdef _WIN32
#include <windows.h>
#endif

// ─────────────────────────────────────────
//  TIPOS DE DATOS
// ─────────────────────────────────────────

typedef enum {
    DISPONIBLE,
    PRESTADO
} Estado;

typedef struct {
    int   id;
    char  titulo[100];
    char  autor[100];
    int   anio;
    char  genero[50];
    Estado estado;
} Item;

// ─────────────────────────────────────────
//  CONSTANTES GLOBALES
// ─────────────────────────────────────────

const char *ARCHIVO = "datos/biblioteca.dat";

// ─────────────────────────────────────────
//  BARRA DE PROGRESO
// ─────────────────────────────────────────

void barraProgreso(const char *mensaje) {
    printf("\n %s\n [", mensaje);
    for (int i = 0; i < 20; i++) {
        printf("=");
        fflush(stdout);
    }
    printf("] Listo!\n\n");
}

// ─────────────────────────────────────────
//  PERSISTENCIA
// ─────────────────────────────────────────

void guardarArchivo(Item *coleccion, int total, int nextId) {
    FILE *f = fopen(ARCHIVO, "wb");
    if (!f) { printf("Error al guardar.\n"); return; }
    fwrite(&total,  sizeof(int), 1, f);
    fwrite(&nextId, sizeof(int), 1, f);
    fwrite(coleccion, sizeof(Item), total, f);
    fclose(f);
    barraProgreso("Guardando datos...");
}

void cargarArchivo(Item **coleccion, int *total, int *nextId) {
    FILE *f = fopen(ARCHIVO, "rb");
    if (!f) {
        // Primera vez, no hay archivo - inicializar con calloc
        *coleccion = calloc(1, sizeof(Item));
        *total = 0;
        *nextId = 1;
        return;
    }

    fread(total,  sizeof(int), 1, f);
    fread(nextId, sizeof(int), 1, f);

    *coleccion = (Item *)calloc(*total, sizeof(Item));
    if (!*coleccion) { fclose(f); return; }

    fread(*coleccion, sizeof(Item), *total, f);
    fclose(f);
    barraProgreso("Cargando datos...");
}

// ─────────────────────────────────────────
//  MENU
// ─────────────────────────────────────────

void dibujarMenu() {
    system("clear || cls");
    printf("\n");
    printf("  +==============================+\n");
    printf("  |   BIBLIOTECA PERSONAL v1.0   |\n");
    printf("  +==============================+\n");
    printf("  |  1. Agregar Nuevo Item       |\n");
    printf("  |  2. Buscar Item              |\n");
    printf("  |  3. Mostrar Todos            |\n");
    printf("  |  4. Modificar Item           |\n");
    printf("  |  5. Eliminar Item            |\n");
    printf("  |  6. Guardar y Salir          |\n");
    printf("  +==============================+\n");
    printf("  >> Seleccione una opcion: ");
}

// ─────────────────────────────────────────
//  MOSTRAR TABLA
// ─────────────────────────────────────────

void mostrarTabla(Item *coleccion, int total) {
    if (total == 0) {
        printf("\n  (No hay items en la coleccion)\n");
        return;
    }
    printf("\n  %-4s %-25s %-20s %-6s %-15s %-12s\n",
           "ID", "TITULO", "AUTOR", "ANIO", "GENERO", "ESTADO");
    printf("  ");
    for (int i = 0; i < 84; i++) printf("-");
    printf("\n");

    for (int i = 0; i < total; i++) {
        printf("  %-4d %-25s %-20s %-6d %-15s %-12s\n",
               coleccion[i].id,
               coleccion[i].titulo,
               coleccion[i].autor,
               coleccion[i].anio,
               coleccion[i].genero,
               coleccion[i].estado == DISPONIBLE ? "DISPONIBLE" : "PRESTADO");
    }
    printf("\n");
}

// ─────────────────────────────────────────
//  CRUD
// ─────────────────────────────────────────

void agregarItem(Item **coleccion, int *total, int *nextId) {
    Item *tmp = (Item *)realloc(*coleccion, (*total + 1) * sizeof(Item));
    if (!tmp) { printf("Error de memoria.\n"); return; }
    *coleccion = tmp;

    Item *nuevo = &(*coleccion)[*total];
    nuevo->id = (*nextId)++;

    printf("\n  Titulo  : "); scanf(" %[^\n]", nuevo->titulo);
    printf("  Autor   : "); scanf(" %[^\n]", nuevo->autor);
    
    // Validar año
    while (1) {
        printf("  Anio    : ");
        if (scanf("%d", &nuevo->anio) == 1 && nuevo->anio > 0 && nuevo->anio <= 2026) {
            break;
        }
        printf("  [ERROR] Ingrese un anio valido (numero entre 1 y 2026)\n");
        while (getchar() != '\n'); // Limpiar buffer
    }
    
    printf("  Genero  : "); scanf(" %[^\n]", nuevo->genero);

    // Validar estado
    int op;
    while (1) {
        printf("  Estado (1=DISPONIBLE, 2=PRESTADO): ");
        if (scanf("%d", &op) == 1 && (op == 1 || op == 2)) {
            nuevo->estado = (op == 2) ? PRESTADO : DISPONIBLE;
            break;
        }
        printf("  [ERROR] Ingrese 1 o 2\n");
        while (getchar() != '\n'); // Limpiar buffer
    }

    (*total)++;
    printf("\n  [OK] Item agregado con ID %d\n", nuevo->id);
}

void buscarItem(Item *coleccion, int total) {
    char busqueda[100];
    printf("\n  Buscar por titulo: ");
    scanf(" %[^\n]", busqueda);

    int encontrado = 0;
    for (int i = 0; i < total; i++) {
        if (strstr(coleccion[i].titulo, busqueda) != NULL) {
            printf("\n  ID: %d | %s | %s | %d | %s | %s\n",
                   coleccion[i].id, coleccion[i].titulo,
                   coleccion[i].autor, coleccion[i].anio,
                   coleccion[i].genero,
                   coleccion[i].estado == DISPONIBLE ? "DISPONIBLE" : "PRESTADO");
            encontrado = 1;
        }
    }
    if (!encontrado) printf("\n  No se encontro ningun item.\n");
}

void modificarItem(Item *coleccion, int total) {
    int id;
    printf("\n  ID del item a modificar: "); scanf("%d", &id);

    for (int i = 0; i < total; i++) {
        if (coleccion[i].id == id) {
            printf("  Nuevo titulo  (actual: %s): ", coleccion[i].titulo);
            scanf(" %[^\n]", coleccion[i].titulo);
            printf("  Nuevo autor   (actual: %s): ", coleccion[i].autor);
            scanf(" %[^\n]", coleccion[i].autor);
            
            // Validar año
            while (1) {
                printf("  Nuevo anio    (actual: %d): ", coleccion[i].anio);
                if (scanf("%d", &coleccion[i].anio) == 1 && coleccion[i].anio > 0 && coleccion[i].anio <= 2026) {
                    break;
                }
                printf("  [ERROR] Ingrese un anio valido (numero entre 1 y 2026)\n");
                while (getchar() != '\n'); // Limpiar buffer
            }
            
            printf("  Nuevo genero  (actual: %s): ", coleccion[i].genero);
            scanf(" %[^\n]", coleccion[i].genero);
            
            // Validar estado
            int op;
            while (1) {
                printf("  Estado (1=DISPONIBLE, 2=PRESTADO): ");
                if (scanf("%d", &op) == 1 && (op == 1 || op == 2)) {
                    coleccion[i].estado = (op == 2) ? PRESTADO : DISPONIBLE;
                    break;
                }
                printf("  [ERROR] Ingrese 1 o 2\n");
                while (getchar() != '\n'); // Limpiar buffer
            }
            
            printf("\n  [OK] Item modificado.\n");
            return;
        }
    }
    printf("\n  ID no encontrado.\n");
}

void eliminarItem(Item **coleccion, int *total) {
    int id;
    printf("\n  ID del item a eliminar: "); scanf("%d", &id);

    for (int i = 0; i < *total; i++) {
        if ((*coleccion)[i].id == id) {
            // Desplazar elementos
            for (int j = i; j < *total - 1; j++)
                (*coleccion)[j] = (*coleccion)[j + 1];
            (*total)--;

            Item *tmp = (Item *)realloc(*coleccion, (*total) * sizeof(Item));
            if (tmp || *total == 0) *coleccion = tmp;

            printf("\n  [OK] Item eliminado.\n");
            return;
        }
    }
    printf("\n  ID no encontrado.\n");
}

// ─────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────

int main() {
    // Configurar UTF-8 para Windows
    #ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    #endif
    setlocale(LC_ALL, "");
    
    // Inicializar variables localmente
    Item *coleccion = NULL;
    int total = 0;
    int nextId = 1;

    cargarArchivo(&coleccion, &total, &nextId);

    int opcion;
    do {
        dibujarMenu();
        if (scanf("%d", &opcion) != 1) {
            printf("\n  [ERROR] Ingrese un numero valido\n");
            while (getchar() != '\n'); // Limpiar buffer
            printf("\n  Presione Enter para continuar...");
            getchar();
            continue;
        }

        switch (opcion) {
            case 1: 
                agregarItem(&coleccion, &total, &nextId);
                guardarArchivo(coleccion, total, nextId);
                break;
            case 2: 
                buscarItem(coleccion, total);
                break;
            case 3: 
                mostrarTabla(coleccion, total);
                break;
            case 4: 
                modificarItem(coleccion, total);
                guardarArchivo(coleccion, total, nextId);
                break;
            case 5: 
                eliminarItem(&coleccion, &total);
                guardarArchivo(coleccion, total, nextId);
                break;
            case 6: 
                // Solo liberar memoria y salir (ya se guarda automáticamente)
                break;
            default: 
                printf("\n  Opcion invalida.\n");
        }

        if (opcion != 6) {
            printf("\n  Presione Enter para continuar...");
            while (getchar() != '\n'); // Limpiar buffer
            getchar(); // Esperar Enter
        }

    } while (opcion != 6);

    free(coleccion);
    printf("  Hasta luego.\n\n");
    return 0;
}
