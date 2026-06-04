#include "raylib.h"

int main(void)
{
    InitWindow(800, 600, "Biblioteca Personal");

    while (!WindowShouldClose())
    {
        BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("Hola mundo!", 350, 280, 20, DARKGRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
