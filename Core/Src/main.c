/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Industrial Motor Condition Monitoring Gateway
  *
  * STM32F401RE + FreeRTOS + ESP32
  *
  * Features:
  * - Simulated Temperature
  * - Simulated Current
  * - Simulated Vibration
  * - Motor State Machine
  * - Fault Detection
  * - Error Counter
  * - UART communication with ESP32
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

/* UART Handles */
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* FreeRTOS Task Handles */
osThreadId_t SensorTaskHandle;
osThreadId_t FaultTaskHandle;
osThreadId_t CommunicationTaskHandle;

/* ============================================================
   SIMULATED SENSOR VALUES
   ============================================================ */

float temperature = 85.0f;   // FAULT TEST
float current = 2.50f;
float vibration = 1.80f;
uint8_t humidity = 62;

/* ============================================================
   THRESHOLDS
   ============================================================ */

#define TEMP_WARNING       60.0f
#define TEMP_FAULT         80.0f

#define CURRENT_WARNING    5.0f
#define CURRENT_FAULT      7.0f

#define VIB_WARNING        3.0f
#define VIB_FAULT          5.0f

/* ============================================================
   MOTOR STATE
   ============================================================ */

typedef enum
{
    MOTOR_RUNNING = 0,
    MOTOR_WARNING,
    MOTOR_FAULT

} MotorState_t;

MotorState_t motorState = MOTOR_RUNNING;

/* Error counter */
uint32_t error_count = 0;

/* UART transmission buffer */
char tx_packet[200];

/* ============================================================
   FUNCTION PROTOTYPES
   ============================================================ */

void SystemClock_Config(void);

static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);

void StartSensorTask(void *argument);
void StartFaultTask(void *argument);
void StartCommunicationTask(void *argument);

MotorState_t CheckMotorHealth(float temp,
                              float curr,
                              float vib);

const char* GetMotorStateString(MotorState_t state);

void Debug_Print(const char *msg);

/* ============================================================
   DEBUG PRINT FUNCTION
   USART2 -> USB TTL -> X-CTU
   ============================================================ */

void Debug_Print(const char *msg)
{
    HAL_UART_Transmit(&huart2,
                      (uint8_t *)msg,
                      strlen(msg),
                      HAL_MAX_DELAY);
}

/* ============================================================
   MOTOR HEALTH CHECK
   ============================================================ */

MotorState_t CheckMotorHealth(float temp,
                              float curr,
                              float vib)
{
    /* FAULT condition */

    if ((temp >= TEMP_FAULT) ||
        (curr >= CURRENT_FAULT) ||
        (vib >= VIB_FAULT))
    {
        return MOTOR_FAULT;
    }

    /* WARNING condition */

    if ((temp >= TEMP_WARNING) ||
        (curr >= CURRENT_WARNING) ||
        (vib >= VIB_WARNING))
    {
        return MOTOR_WARNING;
    }

    /* Normal condition */

    return MOTOR_RUNNING;
}

/* ============================================================
   MOTOR STATE STRING
   ============================================================ */

const char* GetMotorStateString(MotorState_t state)
{
    switch (state)
    {
        case MOTOR_RUNNING:
            return "RUNNING";

        case MOTOR_WARNING:
            return "WARNING";

        case MOTOR_FAULT:
            return "FAULT";

        default:
            return "UNKNOWN";
    }
}

/* ============================================================
   MAIN
   ============================================================ */

int main(void)
{
    /* HAL initialization */
    HAL_Init();

    /* Clock configuration */
    SystemClock_Config();

    /* GPIO initialization */
    MX_GPIO_Init();

    /* UART initialization */
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    /* IMPORTANT:
       Initialize FreeRTOS kernel BEFORE creating tasks */
    osKernelInitialize();

    /* ========================================================
       STARTUP MESSAGE
       ======================================================== */

    Debug_Print("\r\n");
    Debug_Print("========================================\r\n");
    Debug_Print(" INDUSTRIAL MOTOR MONITORING GATEWAY\r\n");
    Debug_Print(" STM32F401RE + FreeRTOS + ESP32\r\n");
    Debug_Print("========================================\r\n");

    Debug_Print("System Started\r\n");
    Debug_Print("Fault Detection Enabled\r\n");
    Debug_Print("Motor State Machine Enabled\r\n");
    Debug_Print("Simulated Sensors Enabled\r\n");

    Debug_Print("----------------------------------------\r\n\r\n");

    /* ========================================================
       SENSOR TASK
       ======================================================== */

    const osThreadAttr_t sensorTask_attributes =
    {
        .name = "SensorTask",
        .priority = (osPriority_t)osPriorityNormal,
        .stack_size = 256 * 4
    };

    SensorTaskHandle =
        osThreadNew(StartSensorTask,
                    NULL,
                    &sensorTask_attributes);

    /* ========================================================
       FAULT TASK
       ======================================================== */

    const osThreadAttr_t faultTask_attributes =
    {
        .name = "FaultTask",
        .priority = (osPriority_t)osPriorityAboveNormal,
        .stack_size = 256 * 4
    };

    FaultTaskHandle =
        osThreadNew(StartFaultTask,
                    NULL,
                    &faultTask_attributes);

    /* ========================================================
       COMMUNICATION TASK
       ======================================================== */

    const osThreadAttr_t communicationTask_attributes =
    {
        .name = "CommunicationTask",
        .priority = (osPriority_t)osPriorityAboveNormal,
        .stack_size = 512 * 4
    };

    CommunicationTaskHandle =
        osThreadNew(StartCommunicationTask,
                    NULL,
                    &communicationTask_attributes);

    /* ========================================================
       CHECK TASK CREATION
       ======================================================== */

    if ((SensorTaskHandle == NULL) ||
        (FaultTaskHandle == NULL) ||
        (CommunicationTaskHandle == NULL))
    {
        Debug_Print("ERROR: FreeRTOS Task Creation Failed\r\n");

        Error_Handler();
    }

    Debug_Print("All FreeRTOS Tasks Created\r\n");
    Debug_Print("Starting FreeRTOS Scheduler...\r\n\r\n");

    /* Start FreeRTOS */
    osKernelStart();

    /* Should never reach here */

    while (1)
    {
    }
}

/* ============================================================
   SENSOR TASK
   ============================================================ */

void StartSensorTask(void *argument)
{
    (void)argument;

    for (;;)
    {
        /*
         * Simulated sensor values
         *
         * Temperature = 85°C
         * This will generate FAULT condition.
         */

        temperature = 85.0f;

        humidity = 62;

        current = 2.50f;

        vibration = 1.80f;

        /* Toggle onboard LED */
        HAL_GPIO_TogglePin(GPIOA,
                           GPIO_PIN_5);

        /* Run every 1 second */
        osDelay(1000);
    }
}

/* ============================================================
   FAULT DETECTION + STATE MACHINE TASK
   ============================================================ */

void StartFaultTask(void *argument)
{
    (void)argument;

    MotorState_t previousState = MOTOR_RUNNING;

    for (;;)
    {
        /* Check motor health */

        motorState =
            CheckMotorHealth(temperature,
                             current,
                             vibration);

        /* Check whether state changed */

        if (motorState != previousState)
        {
            char debug_msg[150];

            sprintf(debug_msg,
                    "\r\nMOTOR STATE CHANGED: %s -> %s\r\n",
                    GetMotorStateString(previousState),
                    GetMotorStateString(motorState));

            Debug_Print(debug_msg);

            /* =================================================
               FAULT
               ================================================= */

            if (motorState == MOTOR_FAULT)
            {
                error_count++;

                Debug_Print("!!! MOTOR FAULT DETECTED !!!\r\n");

                char fault_msg[100];

                sprintf(fault_msg,
                        "Error Count = %lu\r\n",
                        (unsigned long)error_count);

                Debug_Print(fault_msg);
            }

            /* =================================================
               WARNING
               ================================================= */

            else if (motorState == MOTOR_WARNING)
            {
                Debug_Print("!!! MOTOR WARNING !!!\r\n");
            }

            /* Update previous state */

            previousState = motorState;
        }

        /* Run every 500 ms */

        osDelay(500);
    }
}

/* ============================================================
   COMMUNICATION TASK
   STM32 USART1 -> ESP32
   ============================================================ */

void StartCommunicationTask(void *argument)
{
    (void)argument;

    for (;;)
    {
        /* Create sensor packet */

        sprintf(tx_packet,
                "SENSOR_DATA,TEMP=%.0f,HUM=%d,CURRENT=%.2f,VIBRATION=%.2f,STATE=%s,ERROR=%lu\r\n",
                temperature,
                humidity,
                current,
                vibration,
                GetMotorStateString(motorState),
                (unsigned long)error_count);

        /* Send packet to ESP32 */

        HAL_UART_Transmit(&huart1,
                          (uint8_t *)tx_packet,
                          strlen(tx_packet),
                          HAL_MAX_DELAY);

        /* Debug output through USART2 */

        Debug_Print("STM32 -> ESP32 : ");

        Debug_Print(tx_packet);

        /* Send every 2 seconds */

        osDelay(2000);
    }
}

/* ============================================================
   SYSTEM CLOCK
   ============================================================ */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};

    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Power interface clock enable */

    __HAL_RCC_PWR_CLK_ENABLE();

    __HAL_PWR_VOLTAGESCALING_CONFIG(
        PWR_REGULATOR_VOLTAGE_SCALE2);

    /* HSI configuration */

    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSI;

    RCC_OscInitStruct.HSIState =
        RCC_HSI_ON;

    RCC_OscInitStruct.HSICalibrationValue =
        RCC_HSICALIBRATION_DEFAULT;

    /* PLL disabled */

    RCC_OscInitStruct.PLL.PLLState =
        RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct)
        != HAL_OK)
    {
        Error_Handler();
    }

    /* Clock configuration */

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_HSI;

    RCC_ClkInitStruct.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV1;

    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct,
                            FLASH_LATENCY_0)
        != HAL_OK)
    {
        Error_Handler();
    }
}

/* ============================================================
   USART1
   STM32 <-> ESP32
   ============================================================ */

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;

    huart1.Init.BaudRate = 115200;

    huart1.Init.WordLength =
        UART_WORDLENGTH_8B;

    huart1.Init.StopBits =
        UART_STOPBITS_1;

    huart1.Init.Parity =
        UART_PARITY_NONE;

    huart1.Init.Mode =
        UART_MODE_TX_RX;

    huart1.Init.HwFlowCtl =
        UART_HWCONTROL_NONE;

    huart1.Init.OverSampling =
        UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ============================================================
   USART2
   STM32 <-> USB TTL / X-CTU
   ============================================================ */

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;

    huart2.Init.BaudRate = 115200;

    huart2.Init.WordLength =
        UART_WORDLENGTH_8B;

    huart2.Init.StopBits =
        UART_STOPBITS_1;

    huart2.Init.Parity =
        UART_PARITY_NONE;

    huart2.Init.Mode =
        UART_MODE_TX_RX;

    huart2.Init.HwFlowCtl =
        UART_HWCONTROL_NONE;

    huart2.Init.OverSampling =
        UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ============================================================
   GPIO
   PA5 = ONBOARD LED
   ============================================================ */

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIOA clock enable */

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA5 configuration */

    GPIO_InitStruct.Pin =
        GPIO_PIN_5;

    GPIO_InitStruct.Mode =
        GPIO_MODE_OUTPUT_PP;

    GPIO_InitStruct.Pull =
        GPIO_NOPULL;

    GPIO_InitStruct.Speed =
        GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOA,
                  &GPIO_InitStruct);

    /* LED initially OFF */

    HAL_GPIO_WritePin(GPIOA,
                      GPIO_PIN_5,
                      GPIO_PIN_RESET);
}

/* ============================================================
   ERROR HANDLER
   ============================================================ */

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
        /* Fast LED blinking indicates error */

        HAL_GPIO_TogglePin(GPIOA,
                           GPIO_PIN_5);

        HAL_Delay(200);
    }
}

/* ============================================================
   ASSERT FAILED
   ============================================================ */

#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file,
                   uint32_t line)
{
}

#endif
