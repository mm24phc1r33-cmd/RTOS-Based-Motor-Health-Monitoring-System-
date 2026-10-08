/*
 * DHT11.c
 *
 * Created on: Oct 4, 2026
 * Author: Monika
 */

#include "DHT11.h"

/*
 * STM32F401RE
 * System Clock = 16 MHz HSI
 *
 * DHT11 communication requires microsecond timing.
 * We use Cortex-M4 DWT cycle counter.
 */

/* =========================================================
 * DWT INITIALIZATION
 * ========================================================= */

static void DHT11_DWT_Init(void)
{
    /* Enable trace unit */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* Reset cycle counter */
    DWT->CYCCNT = 0;

    /* Enable cycle counter */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}


/* =========================================================
 * MICROSECOND DELAY
 * ========================================================= */

static void DHT11_Delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;

    /*
     * SystemCoreClock = 16 MHz
     *
     * 16 clock cycles = 1 us
     */

    uint32_t cycles =
            us * (SystemCoreClock / 1000000U);

    while ((DWT->CYCCNT - start) < cycles)
    {
        /* Wait */
    }
}


/* =========================================================
 * CONFIGURE DHT11 PIN AS OUTPUT
 * ========================================================= */

static void DHT11_SetOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT11_DATA_Pin;

    /*
     * Open Drain is important because
     * DHT11 and STM32 both communicate on
     * the same DATA line.
     */

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;

    GPIO_InitStruct.Pull = GPIO_PULLUP;

    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(DHT11_DATA_GPIO_Port,
                  &GPIO_InitStruct);
}


/* =========================================================
 * CONFIGURE DHT11 PIN AS INPUT
 * ========================================================= */

static void DHT11_SetInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT11_DATA_Pin;

    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;

    GPIO_InitStruct.Pull = GPIO_PULLUP;

    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(DHT11_DATA_GPIO_Port,
                  &GPIO_InitStruct);
}


/* =========================================================
 * WAIT FOR GPIO STATE
 * ========================================================= */

static uint8_t DHT11_WaitForState(GPIO_PinState state,
                                  uint32_t timeout_us)
{
    while (HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port,
                            DHT11_DATA_Pin) != state)
    {
        if (timeout_us == 0)
        {
            return 0;
        }

        DHT11_Delay_us(1);

        timeout_us--;
    }

    return 1;
}


/* =========================================================
 * READ ONE BIT FROM DHT11
 * ========================================================= */

static uint8_t DHT11_ReadBit(void)
{
    /*
     * Every DHT11 bit starts with LOW pulse
     * followed by HIGH pulse.
     *
     * HIGH ~26 us  -> bit 0
     * HIGH ~70 us  -> bit 1
     */

    /*
     * Wait until DHT11 pulls DATA HIGH.
     */

    if (!DHT11_WaitForState(GPIO_PIN_SET, 100))
    {
        return 0;
    }

    /*
     * Wait approximately 40 us.
     *
     * If DATA is still HIGH after 40 us,
     * it is bit 1.
     *
     * Otherwise it is bit 0.
     */

    DHT11_Delay_us(40);

    if (HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port,
                         DHT11_DATA_Pin) == GPIO_PIN_SET)
    {
        return 1;
    }

    return 0;
}


/* =========================================================
 * DHT11 READ FUNCTION
 * ========================================================= */

HAL_StatusTypeDef DHT11_Read(DHT11_Data_t *data)
{
    uint8_t buffer[5] = {0};

    /* Check pointer */
    if (data == NULL)
    {
        return HAL_ERROR;
    }


    /* -----------------------------------------------------
     * Initialize DWT
     * ----------------------------------------------------- */

    DHT11_DWT_Init();


    /* -----------------------------------------------------
     * STEP 1:
     * STM32 sends START signal
     * ----------------------------------------------------- */

    DHT11_SetOutput();

    /*
     * Pull DATA LOW
     */

    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port,
                      DHT11_DATA_Pin,
                      GPIO_PIN_RESET);

    /*
     * DHT11 requires minimum 18 ms LOW.
     *
     * We use 20 ms.
     */

    DHT11_Delay_us(20000);


    /*
     * Release DATA line HIGH.
     */

    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port,
                      DHT11_DATA_Pin,
                      GPIO_PIN_SET);

    /*
     * Wait 20-40 us before switching to input.
     */

    DHT11_Delay_us(30);


    /* -----------------------------------------------------
     * STEP 2:
     * STM32 releases DATA line
     * ----------------------------------------------------- */

    DHT11_SetInput();


    /* -----------------------------------------------------
     * STEP 3:
     * Wait for DHT11 response
     *
     * DHT11 response:
     *
     * LOW  ~80 us
     * HIGH ~80 us
     * LOW  ~50 us
     * ----------------------------------------------------- */


    /*
     * Wait for DHT11 LOW
     */

    if (!DHT11_WaitForState(GPIO_PIN_RESET, 100))
    {
        DHT11_SetOutput();

        HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port,
                          DHT11_DATA_Pin,
                          GPIO_PIN_SET);

        return HAL_TIMEOUT;
    }


    /*
     * Wait for DHT11 HIGH
     */

    if (!DHT11_WaitForState(GPIO_PIN_SET, 100))
    {
        DHT11_SetOutput();

        HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port,
                          DHT11_DATA_Pin,
                          GPIO_PIN_SET);

        return HAL_TIMEOUT;
    }


    /*
     * Wait for DHT11 LOW
     */

    if (!DHT11_WaitForState(GPIO_PIN_RESET, 100))
    {
        DHT11_SetOutput();

        HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port,
                          DHT11_DATA_Pin,
                          GPIO_PIN_SET);

        return HAL_TIMEOUT;
    }


    /* -----------------------------------------------------
     * STEP 4:
     * Read 40 bits
     *
     * 5 bytes:
     *
     * buffer[0] = Humidity integer
     * buffer[1] = Humidity decimal
     * buffer[2] = Temperature integer
     * buffer[3] = Temperature decimal
     * buffer[4] = Checksum
     * ----------------------------------------------------- */

    for (uint8_t i = 0; i < 40; i++)
    {
        uint8_t bit;

        bit = DHT11_ReadBit();

        /*
         * Shift previous bits left
         */

        buffer[i / 8] <<= 1;

        /*
         * Add newly received bit
         */

        buffer[i / 8] |= bit;
    }


    /* -----------------------------------------------------
     * STEP 5:
     * CHECKSUM
     * ----------------------------------------------------- */

    /*
     * Checksum =
     *
     * Humidity integer
     * + Humidity decimal
     * + Temperature integer
     * + Temperature decimal
     */

    uint8_t checksum =
            (uint8_t)(buffer[0]
                    + buffer[1]
                    + buffer[2]
                    + buffer[3]);


    if (checksum != buffer[4])
    {
        /*
         * Communication/data error
         */

        DHT11_SetOutput();

        HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port,
                          DHT11_DATA_Pin,
                          GPIO_PIN_SET);

        return HAL_ERROR;
    }


    /* -----------------------------------------------------
     * STEP 6:
     * STORE SENSOR DATA
     * ----------------------------------------------------- */

    data->humidity = buffer[0];

    data->temperature = buffer[2];


    /* -----------------------------------------------------
     * STEP 7:
     * Return DATA line to idle HIGH
     * ----------------------------------------------------- */

    DHT11_SetOutput();

    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port,
                      DHT11_DATA_Pin,
                      GPIO_PIN_SET);


    /* Successful reading */

    return HAL_OK;
}
