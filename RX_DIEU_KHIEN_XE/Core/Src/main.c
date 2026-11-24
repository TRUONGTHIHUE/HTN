/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : RECEIVER (XE) - BARE METAL POLLING
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "nrf24.h" // Thư viện nRF24 bắt buộc phải có
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* Cấu trúc lưu trữ lệnh điều khiển từ tay cầm */
typedef struct {
    uint8_t ga;          // 0 - 100%
    uint8_t lai;         // 0 - 100%
    uint8_t mode;        // 0:N, 1:R, 2:D
    uint8_t level;       // 1, 2, 3
    uint8_t connected;   // 1: Có sóng, 0: Mất sóng
} XeControl_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */
uint8_t rx_data[8];      // Bộ đệm nhận 8 byte
uint8_t stat_rx;         // Biến soi trạng thái RF trong Live Expressions
uint8_t nrf_addr[5] = {0x11, 0x22, 0x33, 0x44, 0x55}; // Phải khớp 100% với mạch TX
uint8_t payload_len;

XeControl_t xe_control = {0, 0, 0, 0, 0};
uint32_t last_recv_time = 0; // Biến phục vụ Fail-Safe
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_SPI1_Init();

  /* USER CODE BEGIN 2 */
  /* --- KHỞI TẠO NRF24 TRƯỚC KHI VÀO VÒNG LẶP --- */
  nRF24_Init();
  nRF24_SetRFChannel(76);
  nRF24_SetAddrWidth(5);

  // Thiết lập đường ống nhận Pipe 1
  nRF24_SetAddr(nRF24_PIPE1, nrf_addr);
  nRF24_SetRXPipe(nRF24_PIPE1, nRF24_AA_ON, 8); // 8 byte, bật Auto-Ack

  nRF24_SetOperationalMode(nRF24_MODE_RX); // Chuyển sang chế độ thu
  nRF24_SetPowerMode(nRF24_PWR_UP);

  nRF24_CE_H(); // Mở to tai để nghe ngóng liên tục
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* --- 1. QUÉT TÍN HIỆU RF (POLLING) --- */
    stat_rx = nRF24_GetStatus();

    // Nếu có dữ liệu nhảy vào FIFO (Bit RX_DR lên 1)
    if (stat_rx & (1 << 6)) {
        if (nRF24_ReadPayload(rx_data, &payload_len) != nRF24_RX_EMPTY) {

            // Xác thực Checksum
            uint8_t check = rx_data[0] + rx_data[1] + rx_data[5] + rx_data[6];

            if (check == rx_data[7]) {
                // Đổ dữ liệu vào struct điều khiển
                xe_control.ga    = rx_data[0];
                xe_control.lai   = rx_data[1];
                xe_control.mode  = rx_data[5];
                xe_control.level = rx_data[6];
                xe_control.connected = 1;

                last_recv_time = HAL_GetTick(); // Chốt thời gian nhận tin cuối
                HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin); // Nháy LED báo hiệu
            }
        }
        nRF24_ClearIRQFlags(); // Xóa cờ ngắt để chờ tin tiếp theo
    }

    /* --- 2. FAIL-SAFE (CHỐNG MẤT KIỂM SOÁT) --- */
    // Nếu quá 500ms không có tin mới -> Ép ga về 0
    if ((HAL_GetTick() - last_recv_time) > 500) {
        xe_control.connected = 0;
        xe_control.ga = 0;
    }

    /* --- 3. ĐIỀU KHIỂN ĐỘNG CƠ TẠI ĐÂY --- */
    // Update_Motor_Speed(xe_control.ga);
    // Update_Servo_Angle(xe_control.lai);

    HAL_Delay(10); // Quét chu kỳ 10ms (100Hz)
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

  /* ĐÃ SỬA: Prescaler = 16 để tốc độ SPI đạt 4.5MHz (Phù hợp với nRF24) */
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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(NRF_CSN_GPIO_Port, NRF_CSN_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(NRF_CE_GPIO_Port, NRF_CE_Pin, GPIO_PIN_RESET); // Đã sửa về RESET lúc init

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : NRF_CSN_Pin */
  GPIO_InitStruct.Pin = NRF_CSN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(NRF_CSN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : NRF_CE_Pin */
  GPIO_InitStruct.Pin = NRF_CE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(NRF_CE_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
