/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "nrf24.h"
#include <stdio.h>
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */

uint16_t joy_data[4];    // Mảng dữ liệu từ ADC DMA (X1, Y1, X2, Y2)
uint8_t nrf_address[5] = {0x11, 0x22, 0x33, 0x44, 0x55}; // Địa chỉ truyền nhận
uint8_t tx_packet[8];    // Gói tin sẽ gửi đi (8 byte)
uint8_t nrf_status = 0;  // Biến kiểm tra nRF có sống không

// Biến trạng thái xe
typedef struct {
    uint8_t left_signal;    // 0: tắt, 1: bật xi nhan trái
    uint8_t right_signal;   // 0: tắt, 1: bật xi nhan phải
    uint8_t hazard;         // 0: tắt, 1: bật đèn khẩn cấp
    uint8_t gear_mode;      // 0: N, 1: R, 2: D (Drive)
    uint8_t drive_gear;     // 0: không xác định, 1, 2, 3 (chỉ dùng khi gear_mode == 2)
} VehicleState_t;

VehicleState_t vehicle = {0, 0, 0, 0, 0};  // Khởi tạo drive_gear = 0

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Hàm xử lý công tắc xi nhan (PB10, PB11)
void ProcessTurnSignals(uint8_t sw1_on1, uint8_t sw1_on2) {
    // Ưu tiên hazard cao nhất
    if (vehicle.hazard) {
        vehicle.left_signal = 0;   // Hazard sẽ tự xử lý nhấp nháy ở xe
        vehicle.right_signal = 0;
        return;
    }

    // Xử lý xi nhan bình thường
    // LƯU Ý: sw1_on1 và sw1_on2 đã được đảo logic từ pull-up
    // sw1_on1 = 1, sw1_on2 = 0 → Vị trí TRÁI (gạt sang trái)
    // sw1_on1 = 0, sw1_on2 = 1 → Vị trí PHẢI (gạt sang phải)
    // sw1_on1 = 0, sw1_on2 = 0 → Vị trí GIỮA (tắt)

    if (sw1_on1 && !sw1_on2) {
        vehicle.left_signal = 1;   // Xi nhan trái
        vehicle.right_signal = 0;
    }
    else if (!sw1_on1 && sw1_on2) {
        vehicle.left_signal = 0;
        vehicle.right_signal = 1;  // Xi nhan phải
    }
    else {
        vehicle.left_signal = 0;
        vehicle.right_signal = 0;
    }
}

// Hàm xử lý hộp số (PB12, PB13) - dùng joy1_y dạng 8-bit
void ProcessGear(uint8_t sw2_on1, uint8_t sw2_on2, uint8_t joy1_y_8bit) {
    // Xác định chế độ hộp số
    // sw2_on1 = 1, sw2_on2 = 0 → Vị trí TRÁI → R (lùi)
    // sw2_on1 = 0, sw2_on2 = 1 → Vị trí PHẢI → D (Drive - tiến)
    // sw2_on1 = 0, sw2_on2 = 0 → Vị trí GIỮA → N (trung gian)

    if (sw2_on1 && !sw2_on2) {
        vehicle.gear_mode = 1;  // R (lùi)
        vehicle.drive_gear = 0; // Reset số tiến
    }
    else if (!sw2_on1 && sw2_on2) {
        vehicle.gear_mode = 2;  // D (Drive - tiến)

        // Nếu ở chế độ D, dùng joystick trái (Y) để chọn số 1, 2, 3
        // joy1_y_8bit có giá trị 0-255
        // Chia làm 3 vùng:
        // 0-85: số 1
        // 86-170: số 2
        // 171-255: số 3
        if (joy1_y_8bit < 86) {
            vehicle.drive_gear = 1;
        }
        else if (joy1_y_8bit < 171) {
            vehicle.drive_gear = 2;
        }
        else {
            vehicle.drive_gear = 3;
        }
    }
    else {
        vehicle.gear_mode = 0;  // N (trung gian)
        vehicle.drive_gear = 0; // Reset số tiến
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)joy_data, 4);
  HAL_Delay(100);
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 4;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(NRF_CSN_GPIO_Port, NRF_CSN_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(NRF_CE_GPIO_Port, NRF_CE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : NRF_CSN_Pin */
  GPIO_InitStruct.Pin = NRF_CSN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(NRF_CSN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : NRF_CE_Pin */
  GPIO_InitStruct.Pin = NRF_CE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(NRF_CE_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : NRF_IRQ_Pin */
  GPIO_InitStruct.Pin = NRF_IRQ_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(NRF_IRQ_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : SW1_ON1_Pin SW1_ON2_Pin SW2_ON1_Pin SW2_ON2_Pin
                           SW3_ON_Pin */
  GPIO_InitStruct.Pin = SW1_ON1_Pin|SW1_ON2_Pin|SW2_ON1_Pin|SW2_ON2_Pin
                          |SW3_ON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI1_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
	/* USER CODE BEGIN 5 */
	  // --- KHỞI TẠO NRF24 ---
	  nRF24_Init();

	  // Kiểm tra xem module có phản hồi không
	  if (nRF24_Check()) {
	      nrf_status = 1; // nRF hoạt động tốt
	  } else {
	      nrf_status = 0; // nRF bị lỗi hoặc lỏng dây
	  }

	  // Cấu hình thông số truyền
	  nRF24_SetRFChannel(76);              // Chọn kênh 76 (tránh nhiễu Wifi)
	  nRF24_SetDataRate(nRF24_DR_1Mbps);   // Tốc độ 1Mbps cho ổn định
	  nRF24_SetAddrWidth(5);               // Độ dài địa chỉ 5 byte
	  nRF24_SetAddr(nRF24_PIPETX, nrf_address);
	  nRF24_SetTXPower(nRF24_TXPWR_0dBm);  // Công suất phát mạnh nhất
	  nRF24_SetOperationalMode(nRF24_MODE_TX); // Chế độ Phát
	  nRF24_SetPowerMode(nRF24_PWR_UP);    // Bật nguồn module

	  /* Infinite loop */
	  for(;;)
	  {
	    // --- ĐỌC GIÁ TRỊ JOYSTICK TỪ DMA ---
	    // Nén 12-bit ADC (0-4095) về 8-bit (0-255) cho nhẹ
	    uint8_t joy1_x = (uint8_t)(joy_data[0] >> 4); // PA0 - Joystick 1 X
	    uint8_t joy1_y = (uint8_t)(joy_data[1] >> 4); // PA1 - Joystick 1 Y
	    uint8_t joy2_x = (uint8_t)(joy_data[2] >> 4); // PA2 - Joystick 2 X
	    uint8_t joy2_y = (uint8_t)(joy_data[3] >> 4); // PA3 - Joystick 2 Y

	    // --- ĐỌC CÔNG TẮC (ĐẢO LOGIC DO PULL-UP) ---
	    // Vì dùng Pull-up, khi công tắc bật (nối GND) đọc là 0, cần đảo thành 1
	    uint8_t sw1_on1 = !HAL_GPIO_ReadPin(GPIOB, SW1_ON1_Pin);  // PB10
	    uint8_t sw1_on2 = !HAL_GPIO_ReadPin(GPIOB, SW1_ON2_Pin);  // PB11
	    uint8_t sw2_on1 = !HAL_GPIO_ReadPin(GPIOB, SW2_ON1_Pin);  // PB12
	    uint8_t sw2_on2 = !HAL_GPIO_ReadPin(GPIOB, SW2_ON2_Pin);  // PB13
	    uint8_t sw3_on  = !HAL_GPIO_ReadPin(GPIOB, SW3_ON_Pin);   // PB14

	    // --- XỬ LÝ HAZARD (Công tắc 3) ---
	    // BẬT → Kích hoạt đèn flash khẩn cấp toàn bộ
	    // TẮT → Tắt đèn cảnh báo
	    vehicle.hazard = sw3_on;

	    // --- XỬ LÝ XI NHAN (Công tắc 1) ---
	    // Lưu ý: sw1_on1 và sw1_on2 đã được đảo logic
	    ProcessTurnSignals(sw1_on1, sw1_on2);

	    // --- XỬ LÝ HỘP SỐ (Công tắc 2 + Joystick trái) ---
	    ProcessGear(sw2_on1, sw2_on2, joy1_y);

	    // --- ĐÓNG GÓI DỮ LIỆU GỬI ĐI (8 byte) ---
	    tx_packet[0] = joy1_x;                // Joystick 1 X
	    tx_packet[1] = joy1_y;                // Joystick 1 Y
	    tx_packet[2] = joy2_x;                // Joystick 2 X
	    tx_packet[3] = joy2_y;                // Joystick 2 Y
	    tx_packet[4] = vehicle.left_signal;   // Đèn xi nhan trái
	    tx_packet[5] = vehicle.right_signal;  // Đèn xi nhan phải
	    tx_packet[6] = vehicle.hazard;        // Đèn khẩn cấp (hazard)
	    tx_packet[7] = (vehicle.gear_mode << 2) | (vehicle.drive_gear & 0x03); // Đóng gói 2 thông số vào 1 byte
	    // Bit 7-6: gear_mode (0-2)
	    // Bit 5-0: drive_gear (0-3) - 0 là không xác định, 1-3 là số

	    // --- GỬI DỮ LIỆU QUA NRF24 ---
	    if (nrf_status == 1) {
	        nRF24_WritePayload(tx_packet, 8); // Đưa 8 byte vào FIFO
	        nRF24_CE_H();                     // Kích xung phát
	        osDelay(1);                       // Đợi phát (1ms là quá dư)
	        nRF24_CE_L();

	        // Xóa cờ ngắt TX để sẵn sàng cho lần sau
	        nRF24_ClearIRQFlags();
	    }

	    osDelay(20); // Gửi thông tin 50 lần/giây (lái xe cực mượt)
	  }
	  /* USER CODE END 5 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM4 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM4)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
