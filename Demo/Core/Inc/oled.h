#ifndef __OLED_H
#define __OLED_H

#include "main.h"

#define OLED_WIDTH  128
#define OLED_HEIGHT 64
#define OLED_ADDR   0x3C   /* 0x3C = 7-bit address, 0.96" OLED 常见地址 */

void OLED_Init(void);
void OLED_Clear(void);
void OLED_Update(void);   /* 把显存刷到屏幕 */

void OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t on);
void OLED_DrawHLine(uint8_t x, uint8_t y, uint8_t w);
void OLED_DrawVLine(uint8_t x, uint8_t y, uint8_t h);
void OLED_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void OLED_DrawChar(uint8_t x, uint8_t y, char c);
void OLED_DrawString(uint8_t x, uint8_t y, const char *s);

#endif /* __OLED_H */
