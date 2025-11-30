# ILI9486 Driver para STM32F411 (Optimizado para Juegos)

Este proyecto contiene un driver escrito en C para la pantalla TFT ILI9486, diseñado específicamente para microcontroladores STM32F411 (como la placa Nucleo-F411RE).

## Características

*   **Alto Rendimiento**: Utiliza SPI por Hardware (SPI1) en lugar de bit-banging.
*   **Optimizado para Gráficos**: Incluye funciones como `ILI9486_FillRect` y `ILI9486_DrawImage` que utilizan transferencias en bloque, minimizando la sobrecarga de comandos y permitiendo tasas de refresco aptas para juegos simples.
*   **Documentación en Español**: Código comentado y explicado en español.

## Conexiones (Nucleo-F411RE / Arduino Header)

| Señal ILI9486 | Pin STM32 | Pin Arduino | Descripción |
| :--- | :--- | :--- | :--- |
| **SCLK** | PA5 | D13 | Reloj SPI |
| **MOSI** | PA7 | D11 | Datos SPI (Master Out) |
| **CS** | PA9 | D8 | Chip Select |
| **DC/RS** | PA8 | D7 | Data/Command |
| **RST** | PA10 | D2 | Reset |
| **VCC** | 5V/3.3V | 5V/3.3V | Alimentación |
| **GND** | GND | GND | Tierra |

*Nota: Verificar si la pantalla requiere 5V o 3.3V. La lógica del STM32 es 3.3V, lo cual es compatible con la mayoría de pantallas ILI9486.*

## Archivos

*   `ili9486.h`: Cabecera con definiciones de colores, pines y prototipos.
*   `ili9486.c`: Implementación del driver. Configura SPI1 y GPIOs.
*   `main.c`: Ejemplo de uso con un bucle de juego simple (nave rebotando).

## Cómo Compilar

Este código asume el uso de la **Standard Peripheral Library (SPL)** para STM32F4.

1.  Asegúrate de tener configurado tu entorno de desarrollo para STM32F4 (Keil, IAR, STM32CubeIDE con soporte SPL, o Makefile con GCC).
2.  Agrega `ili9486.c`, `ili9486.h` y `main.c` a tu proyecto.
3.  Asegúrate de incluir los archivos de sistema (`system_stm32f4xx.c`, `startup_stm32f4xx.s`) y las librerías SPL necesarias (`stm32f4xx_spi.c`, `stm32f4xx_gpio.c`, `stm32f4xx_rcc.c`).
4.  Compila y carga en tu placa.

## Ajustes

Si necesitas cambiar los pines, edita las definiciones al principio de `ili9486.c`.
