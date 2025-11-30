/**
 * @file ili9486.h
 * @brief Driver para pantalla LCD TFT ILI9486 optimizado para juegos.
 * @author Jules (Asistente de IA)
 *
 * Este driver utiliza SPI por Hardware para máxima velocidad en STM32F411.
 */

#ifndef ILI9486_H
#define ILI9486_H

#include "stm32f4xx.h"

// Definición de colores básicos en formato RGB565 (16 bits)
#define ILI9486_NEGRO       0x0000
#define ILI9486_AZUL        0x001F
#define ILI9486_ROJO        0xF800
#define ILI9486_VERDE       0x07E0
#define ILI9486_CIAN        0x07FF
#define ILI9486_MAGENTA     0xF81F
#define ILI9486_AMARILLO    0xFFE0
#define ILI9486_BLANCO      0xFFFF

// Dimensiones de la pantalla (ajustar según orientación)
#define ILI9486_ANCHO       320
#define ILI9486_ALTO        480

/**
 * @brief Inicializa el hardware SPI y la pantalla ILI9486.
 */
void ILI9486_Init(void);

/**
 * @brief Establece la ventana de dibujo activa.
 * @param x0 Coordenada X inicial
 * @param y0 Coordenada Y inicial
 * @param x1 Coordenada X final
 * @param y1 Coordenada Y final
 */
void ILI9486_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief Dibuja un píxel en la posición especificada.
 * Nota: No es eficiente para llenar áreas grandes.
 */
void ILI9486_DrawPixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief Rellena un rectángulo con un color sólido de forma optimizada.
 * Ideal para borrar la pantalla o dibujar fondos.
 * @param x Coordenada X de la esquina superior izquierda
 * @param y Coordenada Y de la esquina superior izquierda
 * @param w Ancho del rectángulo
 * @param h Alto del rectángulo
 * @param color Color de relleno (RGB565)
 */
void ILI9486_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief Dibuja una imagen (bitmap) en la pantalla.
 * @param x Coordenada X
 * @param y Coordenada Y
 * @param w Ancho de la imagen
 * @param h Alto de la imagen
 * @param data Puntero a los datos de la imagen (array de 16 bits RGB565)
 */
void ILI9486_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* data);

#endif // ILI9486_H
