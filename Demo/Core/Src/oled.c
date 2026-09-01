/* oled.c - 0.96" OLED (128x64, 驱动芯片 SSD1306) 软件 I2C 驱动
 * SCL=PB8, SDA=PB9 (开漏 + 内部上拉), 写模式(不读 ACK)
 */
#include "oled.h"

/* ---------------- 软件 I2C ---------------- */
#define SCL_H()  HAL_GPIO_WritePin(OLED_SCL_Port, OLED_SCL_Pin, GPIO_PIN_SET)
#define SCL_L()  HAL_GPIO_WritePin(OLED_SCL_Port, OLED_SCL_Pin, GPIO_PIN_RESET)
#define SDA_H()  HAL_GPIO_WritePin(OLED_SDA_Port, OLED_SDA_Pin, GPIO_PIN_SET)
#define SDA_L()  HAL_GPIO_WritePin(OLED_SDA_Port, OLED_SDA_Pin, GPIO_PIN_RESET)

static void i2c_delay(void)
{
  volatile uint32_t i;
  for (i = 0; i < 8U; i++) { __NOP(); }
}

static void i2c_start(void)
{
  SDA_H(); SCL_H(); i2c_delay();
  SDA_L(); i2c_delay();
  SCL_L();
}

static void i2c_stop(void)
{
  SDA_L(); SCL_H(); i2c_delay();
  SDA_H(); i2c_delay();
}

static void i2c_write_byte(uint8_t byte)
{
  uint8_t i;
  for (i = 0; i < 8U; i++)
  {
    if (byte & 0x80U) { SDA_H(); } else { SDA_L(); }
    i2c_delay();
    SCL_H(); i2c_delay();
    SCL_L();
    byte <<= 1;
  }
  /* 第 9 个时钟 = ACK 位, 写模式忽略 */
  SDA_H(); i2c_delay();
  SCL_H(); i2c_delay();
  SCL_L();
}

static void oled_write_cmd(uint8_t cmd)
{
  i2c_start();
  i2c_write_byte(OLED_ADDR << 1);         /* 写地址 */
  i2c_write_byte(0x00);                   /* 控制字节: 命令 */
  i2c_write_byte(cmd);
  i2c_stop();
}

static void oled_write_data(uint8_t data)
{
  i2c_start();
  i2c_write_byte(OLED_ADDR << 1);
  i2c_write_byte(0x40);                   /* 控制字节: 数据 */
  i2c_write_byte(data);
  i2c_stop();
}

/* ---------------- 显存 ---------------- */
static uint8_t framebuffer[OLED_WIDTH * OLED_HEIGHT / 8];

void OLED_Init(void)
{
  oled_write_cmd(0xAE);    /* display off */
  oled_write_cmd(0xD5); oled_write_cmd(0x80);  /* 时钟分频 */
  oled_write_cmd(0xA8); oled_write_cmd(0x3F);  /* 复用 64 行 */
  oled_write_cmd(0xD3); oled_write_cmd(0x00);  /* 显示偏移 */
  oled_write_cmd(0x40);    /* 起始行 0 */
  oled_write_cmd(0x8D); oled_write_cmd(0x14);  /* 电荷泵开 */
  oled_write_cmd(0x20); oled_write_cmd(0x00);  /* 水平寻址模式 */
  oled_write_cmd(0xA1);    /* 段重映射 */
  oled_write_cmd(0xC8);    /* COM 扫描方向 */
  oled_write_cmd(0xDA); oled_write_cmd(0x12);  /* COM 引脚 */
  oled_write_cmd(0x81); oled_write_cmd(0xCF);  /* 对比度 */
  oled_write_cmd(0xD9); oled_write_cmd(0xF1);  /* 预充电 */
  oled_write_cmd(0xDB); oled_write_cmd(0x40);  /* VCOM 检测 */
  oled_write_cmd(0xA4);    /* 输出跟随显存 */
  oled_write_cmd(0xA6);    /* 正常显示(非反色) */
  oled_write_cmd(0xAF);    /* display on */

  OLED_Clear();
  OLED_Update();
}

void OLED_Clear(void)
{
  uint16_t i;
  for (i = 0; i < (uint16_t)(OLED_WIDTH * OLED_HEIGHT / 8); i++)
  {
    framebuffer[i] = 0x00;
  }
}

void OLED_Update(void)
{
  uint16_t i;
  /* 水平寻址模式下, 设置列与页范围 */
  oled_write_cmd(0x21); oled_write_cmd(0x00); oled_write_cmd(127);
  oled_write_cmd(0x22); oled_write_cmd(0x00); oled_write_cmd(7);

  for (i = 0; i < (uint16_t)(OLED_WIDTH * OLED_HEIGHT / 8); i++)
  {
    oled_write_data(framebuffer[i]);
  }
}

/* ---------------- 绘图 ---------------- */
void OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t on)
{
  if (x >= OLED_WIDTH || y >= OLED_HEIGHT) { return; }
  if (on)
  {
    framebuffer[(y >> 3) * OLED_WIDTH + x] |= (uint8_t)(1U << (y & 0x07U));
  }
  else
  {
    framebuffer[(y >> 3) * OLED_WIDTH + x] &= (uint8_t)~(1U << (y & 0x07U));
  }
}

void OLED_DrawHLine(uint8_t x, uint8_t y, uint8_t w)
{
  uint8_t i;
  for (i = 0; i < w; i++) { OLED_DrawPixel(x + i, y, 1); }
}

void OLED_DrawVLine(uint8_t x, uint8_t y, uint8_t h)
{
  uint8_t i;
  for (i = 0; i < h; i++) { OLED_DrawPixel(x, y + i, 1); }
}

void OLED_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
  int16_t dx = (x1 > x0) ? (int16_t)(x1 - x0) : (int16_t)(x0 - x1);
  int16_t dy = (y1 > y0) ? (int16_t)(y1 - y0) : (int16_t)(y0 - y1);
  int16_t sx = (x0 < x1) ? 1 : -1;
  int16_t sy = (y0 < y1) ? 1 : -1;
  int16_t err = dx - dy;

  while (1)
  {
    OLED_DrawPixel((uint8_t)x0, (uint8_t)y0, 1);
    if (x0 == x1 && y0 == y1) { break; }
    int16_t e2 = 2 * err;
    if (e2 > -dy) { err -= dy; x0 += sx; }
    if (e2 <  dx) { err += dx; y0 += sy; }
  }
}

/* ---------------- 8x8 字体 (精简字符集) ---------------- */
/* 索引: 0-9='0'-'9', 10='F',11='H',12='D',13='z',14='=',15='%',16=' ',17='.' */
static const uint8_t font8x8[18][8] = {
  {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}, /* 0 */
  {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, /* 1 */
  {0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00}, /* 2 */
  {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, /* 3 */
  {0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x00}, /* 4 */
  {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, /* 5 */
  {0x3C,0x66,0x60,0x7C,0x66,0x66,0x3C,0x00}, /* 6 */
  {0x7E,0x66,0x0C,0x18,0x18,0x18,0x18,0x00}, /* 7 */
  {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, /* 8 */
  {0x3C,0x66,0x66,0x3E,0x06,0x66,0x3C,0x00}, /* 9 */
  {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00}, /* F */
  {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, /* H */
  {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, /* D */
  {0x7E,0x06,0x0C,0x18,0x30,0x7E,0x00,0x00}, /* z */
  {0x00,0x7E,0x00,0x7E,0x00,0x00,0x00,0x00}, /* = */
  {0x62,0x64,0x08,0x10,0x26,0x46,0x00,0x00}, /* % */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 空格 */
  {0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00}  /* . */
};

static uint8_t font_index(char c)
{
  if (c >= '0' && c <= '9') { return (uint8_t)(c - '0'); }
  switch (c)
  {
    case 'F': return 10;
    case 'H': return 11;
    case 'D': return 12;
    case 'z': return 13;
    case '=': return 14;
    case '%': return 15;
    case '.': return 17;
    default:  return 16;   /* 未知字符按空格处理 */
  }
}

void OLED_DrawChar(uint8_t x, uint8_t y, char c)
{
  uint8_t idx = font_index(c);
  uint8_t row;
  for (row = 0; row < 8; row++)
  {
    uint8_t bits = font8x8[idx][row];
    uint8_t col;
    for (col = 0; col < 8; col++)
    {
      if (bits & (uint8_t)(0x80U >> col)) { OLED_DrawPixel(x + col, y + row, 1); }
    }
  }
}

void OLED_DrawString(uint8_t x, uint8_t y, const char *s)
{
  while (*s != '\0')
  {
    OLED_DrawChar(x, y, *s);
    x += 8;
    if (x >= OLED_WIDTH) { break; }
    s++;
  }
}
