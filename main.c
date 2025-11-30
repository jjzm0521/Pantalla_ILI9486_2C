/**
 * @file main.c
 * @brief Juego de demostración con pantalla ILI9486 en STM32F411.
 * @author Jules
 */

#include "stm32f4xx.h"
#include "ili9486.h"

// Definición de la Nave
typedef struct {
    uint16_t x, y;
    uint16_t w, h;
    uint16_t color;
    int16_t vx, vy;
} Nave;

Nave nave = {
    .x = 100, .y = 400,
    .w = 30, .h = 40,
    .color = ILI9486_ROJO,
    .vx = 2, .vy = 0
};

// Retardo simple
void Delay_ms(uint32_t ms) {
    uint32_t i;
    for(i = 0; i < ms * 4000; i++) __asm("nop");
}

int main(void) {
    // Inicializar Sistema (Relojes, etc. - Asumimos SystemInit llamado en startup)
    // SystemInit(); // Normalmente llamado antes de main

    // Inicializar Pantalla
    ILI9486_Init();

    // Fondo Negro
    ILI9486_FillRect(0, 0, ILI9486_ANCHO, ILI9486_ALTO, ILI9486_NEGRO);

    // Dibujar Suelo o estrellas estáticas si se desea
    ILI9486_FillRect(0, 450, 320, 30, ILI9486_VERDE); // "Suelo"

    while (1) {
        // 1. Borrar la posición anterior de la nave (pintar de negro)
        // Optimización: solo borrar el área que ocupaba la nave
        ILI9486_FillRect(nave.x, nave.y, nave.w, nave.h, ILI9486_NEGRO);

        // 2. Actualizar posición (Física simple)
        nave.x += nave.vx;

        // Rebote en los bordes
        if (nave.x <= 0 || nave.x + nave.w >= ILI9486_ANCHO) {
            nave.vx = -nave.vx;
        }

        // 3. Dibujar la nave en la nueva posición
        ILI9486_FillRect(nave.x, nave.y, nave.w, nave.h, nave.color);

        // Dibujar un detalle en la nave para que parezca más que un rectángulo
        ILI9486_FillRect(nave.x + 10, nave.y + 5, 10, 10, ILI9486_CIAN); // Cabina

        // 4. Control de FPS (retardo simple)
        Delay_ms(16); // ~60 FPS
    }
}
