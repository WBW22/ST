/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 简易示波器 - STM32F103C8T6 + 0.96" OLED
  *                   TIM2 在 PA0 产生 1kHz/50% 的 PWM，PA0 用杜邦线跳接到 PA1，
  *                   ADC1 由 TIM3 以 32kHz 触发采样，DMA 循环存 128 点(4 个周期)，
  *                   主循环把波形画到 SSD1306 上。
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "oled.h"

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;
TIM_HandleTypeDef htim2;   /* PWM 输出 */
TIM_HandleTypeDef htim3;   /* 采样触发 */

uint16_t adc_buf[128];     /* DMA 循环缓冲: 128 点 = 4 个 PWM 周期 */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_ADC1_Init(void);
static void Scope_Draw(void);

/* 绘图区: 波形显示在 y=12..60, 电压 0..3.3V 对应 ADC 0..4095 */
#define PLOT_TOP    12
#define PLOT_BOTTOM 60

/**
  * @brief  The application entry point.
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_ADC1_Init();

  OLED_Init();

  /* 启动 PWM 输出(PA0), 采样定时器(TIM3), 以及 ADC+DMA */
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_Base_Start(&htim3);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, 128);

  while (1)
  {
    Scope_Draw();
    HAL_Delay(30);
  }
}

/**
  * @brief 把 ADC 采样缓冲画成波形
  */
static void Scope_Draw(void)
{
  OLED_Clear();

  /* 顶部文字: 已知的 PWM 参数 */
  OLED_DrawString(0, 0, "F=1000Hz");
  OLED_DrawString(72, 0, "D=50%");

  /* 波形: 128 个点连成折线 */
  for (int x = 0; x < 127; x++)
  {
    int y0 = PLOT_BOTTOM - (int)((uint32_t)adc_buf[x] * (PLOT_BOTTOM - PLOT_TOP) / 4096U);
    int y1 = PLOT_BOTTOM - (int)((uint32_t)adc_buf[x + 1] * (PLOT_BOTTOM - PLOT_TOP) / 4096U);
    if (y0 < PLOT_TOP)    { y0 = PLOT_TOP; }
    if (y0 > PLOT_BOTTOM) { y0 = PLOT_BOTTOM; }
    if (y1 < PLOT_TOP)    { y1 = PLOT_TOP; }
    if (y1 > PLOT_BOTTOM) { y1 = PLOT_BOTTOM; }
    OLED_DrawLine((int16_t)x, (int16_t)y0, (int16_t)(x + 1), (int16_t)y1);
  }

  OLED_Update();
}

/**
  * @brief System Clock Configuration
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization: PA0=PWM(AF推挽), PA1=ADC(模拟), PB8/PB9=OLED(开漏+上拉)
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* PA0: TIM2_CH1 PWM 输出 -> 复用推挽 */
  GPIO_InitStruct.Pin = PWM_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(PWM_GPIO_Port, &GPIO_InitStruct);

  /* PA1: ADC1_IN1 -> 模拟输入 */
  GPIO_InitStruct.Pin = ADC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  HAL_GPIO_Init(ADC_GPIO_Port, &GPIO_InitStruct);

  /* PB8(SCL)/PB9(SDA): 软件 I2C -> 开漏 + 内部上拉 */
  GPIO_InitStruct.Pin = OLED_SCL_Pin | OLED_SDA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(OLED_SCL_Port, &GPIO_InitStruct);
}

/**
  * @brief TIM2: 1kHz PWM 输出在 PA0, 占空比 50%
  *        8MHz / (7+1) = 1MHz 计数频率, ARR=999 -> 1kHz
  */
static void MX_TIM2_Init(void)
{
  TIM_OC_InitTypeDef sConfigOC = {0};

  __HAL_RCC_TIM2_CLK_ENABLE();

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 500;              /* 50% 占空比 */
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM3: 32kHz 自由运行, TRGO=更新事件 -> 触发 ADC
  *        8MHz / 250 = 32kHz, 每周期采样 32 个点
  */
static void MX_TIM3_Init(void)
{
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  __HAL_RCC_TIM3_CLK_ENABLE();

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 249;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1: IN1(PA1), 由 TIM3_TRGO 触发, DMA 循环搬运到 adc_buf
  */
static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  __HAL_RCC_ADC1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA1 通道1: ADC1 专用, 外设->内存, 半字, 循环 */
  hdma_adc1.Instance = DMA1_Channel1;
  hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
  hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  hdma_adc1.Init.Mode = DMA_CIRCULAR;
  hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;
  if (HAL_DMA_Init(&hdma_adc1) != HAL_OK)
  {
    Error_Handler();
  }
  __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);

  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;   /* 每次触发只转换一次 */
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif /* USE_FULL_ASSERT */
