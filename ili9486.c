/**
 * @file ili9486.c
 * @brief Implementación del driver para pantalla ILI9486 en STM32F411.
 * @author Jules (Asistente de IA)
 */

#include "ili9486.h"

// Definición de Pines (Ajustar según conexión física)
// Se asume uso de SPI1 en Puerto A
#define ILI_PUERTO      GPIOA
#define ILI_SPI         SPI1
#define ILI_RCC_GPIO    RCC_AHB1Periph_GPIOA
#define ILI_RCC_SPI     RCC_APB2Periph_SPI1

#define PIN_SCLK        GPIO_Pin_5
#define PIN_MOSI        GPIO_Pin_7
// CS (Chip Select) - PA9
#define PIN_CS          GPIO_Pin_9
// RS (Reset) - PA10
#define PIN_RST         GPIO_Pin_10
// DC (Data/Command) - PA8
#define PIN_DC          GPIO_Pin_8

// Fuentes de pines para configuración AF
#define PIN_SCLK_SRC    GPIO_PinSource5
#define PIN_MOSI_SRC    GPIO_PinSource7

// Macros para control de pines (Optimización: uso directo de registros BSRR si fuera necesario, pero Standard Lib es segura)
#define ILI_CS_BAJO()   GPIO_ResetBits(ILI_PUERTO, PIN_CS)
#define ILI_CS_ALTO()   GPIO_SetBits(ILI_PUERTO, PIN_CS)
#define ILI_DC_CMD()    GPIO_ResetBits(ILI_PUERTO, PIN_DC)
#define ILI_DC_DATO()   GPIO_SetBits(ILI_PUERTO, PIN_DC)
#define ILI_RST_BAJO()  GPIO_ResetBits(ILI_PUERTO, PIN_RST)
#define ILI_RST_ALTO()  GPIO_SetBits(ILI_PUERTO, PIN_RST)

// Función auxiliar para retardo (bloqueante)
static void Delay(volatile uint32_t count) {
    while (count--) {
        __asm("nop");
    }
}

/**
 * @brief Envía un byte por SPI (bloqueante pero rápido).
 */
static void SPI_SendByte(uint8_t data) {
    // Esperar a que el buffer de transmisión esté vacío (TXE)
    while (SPI_I2S_GetFlagStatus(ILI_SPI, SPI_I2S_FLAG_TXE) == RESET);
    // Enviar dato
    SPI_I2S_SendData(ILI_SPI, (uint16_t)data);
    // Nota: Para máxima velocidad en ráfagas no esperamos BSY aquí, solo al final de la transacción o cambio de CS.
}

/**
 * @brief Envía una palabra de 16 bits por SPI.
 */
static void SPI_SendWord(uint16_t data) {
    // Si SPI está configurado a 8 bits, enviamos en dos partes.
    // El driver original configuraba 16 bits, pero ILI9486 SPI suele ser 8 bits por comando/dato.
    // Verificaremos la configuración. Asumiremos 8 bits para mayor compatibilidad con comandos.
    // Enviamos MSB primero.
    SPI_SendByte((data >> 8) & 0xFF);
    SPI_SendByte(data & 0xFF);
}

/**
 * @brief Espera a que la transmisión SPI termine completamente.
 */
static void SPI_WaitBusy(void) {
    while (SPI_I2S_GetFlagStatus(ILI_SPI, SPI_I2S_FLAG_BSY) == SET);
}

/**
 * @brief Envía un comando al LCD.
 */
static void ILI_WriteCmd(uint8_t cmd) {
    ILI_DC_CMD();
    SPI_SendByte(cmd);
    SPI_WaitBusy(); // Esperar antes de cambiar DC
}

/**
 * @brief Envía un dato (parametro) al LCD (8 bits).
 */
static void ILI_WriteData8(uint8_t data) {
    ILI_DC_DATO();
    SPI_SendByte(data);
    SPI_WaitBusy();
}

/**
 * @brief Envía un dato (color/pixel) al LCD (16 bits).
 */
static void ILI_WriteData16(uint16_t data) {
    ILI_DC_DATO();
    SPI_SendWord(data);
    SPI_WaitBusy();
}

void ILI9486_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef  SPI_InitStructure;

    // 1. Habilitar Relojes
    RCC_AHB1PeriphClockCmd(ILI_RCC_GPIO, ENABLE);
    RCC_APB2PeriphClockCmd(ILI_RCC_SPI, ENABLE);

    // 2. Configurar GPIOs
    // CS, RST, DC como Salida Push-Pull
    GPIO_InitStructure.GPIO_Pin = PIN_CS | PIN_RST | PIN_DC;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz; // Alta velocidad
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(ILI_PUERTO, &GPIO_InitStructure);

    // SCLK, MOSI como Función Alternativa
    GPIO_InitStructure.GPIO_Pin = PIN_SCLK | PIN_MOSI;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(ILI_PUERTO, &GPIO_InitStructure);

    // Conectar pines a AF SPI1
    GPIO_PinAFConfig(ILI_PUERTO, PIN_SCLK_SRC, GPIO_AF_SPI1);
    GPIO_PinAFConfig(ILI_PUERTO, PIN_MOSI_SRC, GPIO_AF_SPI1);

    // Estado inicial de pines
    ILI_CS_ALTO();
    ILI_RST_ALTO();

    // 3. Configurar SPI
    SPI_DeInit(ILI_SPI);
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    // Usamos 8 bits para tener control total sobre comandos y datos
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    // Prescaler 8 o 16 para obtener ~10MHz (dependiendo de fPCLK)
    // F411: APB2 suele ser 100MHz (si PLL alto) o 50MHz.
    // Si PCLK es 100MHz, Prescaler 8 -> 12.5MHz. Seguro.
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(ILI_SPI, &SPI_InitStructure);

    SPI_Cmd(ILI_SPI, ENABLE);

    // 4. Secuencia de Reset e Inicialización
    ILI_CS_BAJO();

    // Reset Hardware
    ILI_RST_ALTO();
    Delay(50000);
    ILI_RST_BAJO();
    Delay(50000);
    ILI_RST_ALTO();
    Delay(200000);

    // Comandos de Inicialización (Basado en el código original pero adaptado)
    ILI_WriteCmd(0xB0); // Interface Mode Control
    ILI_WriteData8(0x00);

    ILI_WriteCmd(0x11); // Sleep OUT
    Delay(150000);

    ILI_WriteCmd(0x3A); // Interface Pixel Format
    ILI_WriteData8(0x55); // 16 bits/pixel (5-6-5)

    ILI_WriteCmd(0x36); // Memory Access Control
    ILI_WriteData8(0x28); // BGR Order (Ajustar si los colores salen invertidos RGB/BGR)

    ILI_WriteCmd(0xC2); // Power Control 3
    ILI_WriteData8(0x44);

    ILI_WriteCmd(0xC5); // VCOM Control
    ILI_WriteData8(0x00);
    ILI_WriteData8(0x00);
    ILI_WriteData8(0x00);
    ILI_WriteData8(0x00);

    // Gamma... (Omitido por brevedad, usar valores por defecto o copiar si se ve mal)
    // Usaremos valores por defecto del driver original si es necesario,
    // pero para probar funcionalidad básica esto suele bastar.
    // Agrego los gamma del original para asegurar calidad.

    ILI_WriteCmd(0xE0); // PGAMCTRL
    uint8_t gammaP[] = {0x0F, 0x1F, 0x1C, 0x0C, 0x0F, 0x08, 0x48, 0x98, 0x37, 0x0A, 0x13, 0x04, 0x11, 0x0D, 0x00};
    for(int i=0; i<15; i++) ILI_WriteData8(gammaP[i]);

    ILI_WriteCmd(0xE1); // NGAMCTRL
    uint8_t gammaN[] = {0x0F, 0x32, 0x2E, 0x0B, 0x0D, 0x05, 0x47, 0x75, 0x37, 0x06, 0x10, 0x03, 0x24, 0x20, 0x00};
    for(int i=0; i<15; i++) ILI_WriteData8(gammaN[i]);

    ILI_WriteCmd(0x29); // Display ON
    Delay(150000);

    ILI_CS_ALTO();
}

void ILI9486_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    ILI_CS_BAJO();

    // Column Address Set
    ILI_WriteCmd(0x2A);
    ILI_WriteData8(x0 >> 8);
    ILI_WriteData8(x0 & 0xFF);
    ILI_WriteData8(x1 >> 8);
    ILI_WriteData8(x1 & 0xFF);

    // Page Address Set
    ILI_WriteCmd(0x2B);
    ILI_WriteData8(y0 >> 8);
    ILI_WriteData8(y0 & 0xFF);
    ILI_WriteData8(y1 >> 8);
    ILI_WriteData8(y1 & 0xFF);

    // Preparar para escribir en RAM
    ILI_WriteCmd(0x2C);

    ILI_CS_ALTO();
}

void ILI9486_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if ((x >= ILI9486_ANCHO) || (y >= ILI9486_ALTO)) return;

    ILI9486_SetWindow(x, y, x, y);

    ILI_CS_BAJO();
    ILI_DC_DATO(); // Datos
    SPI_SendWord(color);
    SPI_WaitBusy();
    ILI_CS_ALTO();
}

void ILI9486_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if ((x >= ILI9486_ANCHO) || (y >= ILI9486_ALTO)) return;
    if ((x + w - 1) >= ILI9486_ANCHO) w = ILI9486_ANCHO - x;
    if ((y + h - 1) >= ILI9486_ALTO) h = ILI9486_ALTO - y;

    ILI9486_SetWindow(x, y, x + w - 1, y + h - 1);

    ILI_CS_BAJO();
    ILI_DC_DATO();

    uint32_t total_pixels = (uint32_t)w * h;
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;

    // Loop optimizado: enviamos bytes continuamente.
    // Al ser de 8 bits el SPI, enviamos Hi luego Lo.
    while (total_pixels--) {
        // Enviar Hi
        while (SPI_I2S_GetFlagStatus(ILI_SPI, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData(ILI_SPI, hi);

        // Enviar Lo
        while (SPI_I2S_GetFlagStatus(ILI_SPI, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData(ILI_SPI, lo);
    }

    SPI_WaitBusy();
    ILI_CS_ALTO();
}

void ILI9486_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* data) {
    if ((x >= ILI9486_ANCHO) || (y >= ILI9486_ALTO)) return;
    if ((x + w - 1) >= ILI9486_ANCHO) w = ILI9486_ANCHO - x;
    if ((y + h - 1) >= ILI9486_ALTO) h = ILI9486_ALTO - y;

    ILI9486_SetWindow(x, y, x + w - 1, y + h - 1);

    ILI_CS_BAJO();
    ILI_DC_DATO();

    uint32_t total_pixels = (uint32_t)w * h;
    const uint16_t* p = data;

    while (total_pixels--) {
        uint16_t color = *p++;
        // Enviar Hi
        while (SPI_I2S_GetFlagStatus(ILI_SPI, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData(ILI_SPI, (color >> 8));

        // Enviar Lo
        while (SPI_I2S_GetFlagStatus(ILI_SPI, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData(ILI_SPI, (color & 0xFF));
    }

    SPI_WaitBusy();
    ILI_CS_ALTO();
}
