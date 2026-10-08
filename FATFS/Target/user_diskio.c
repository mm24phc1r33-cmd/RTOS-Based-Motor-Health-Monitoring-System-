/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    user_diskio.c
  * @brief   SD Card SPI Disk I/O Driver
  *
  * STM32F401RE
  * SPI2:
  *   PB13 -> SCK
  *   PB14 -> MISO
  *   PB15 -> MOSI
  *   PB12 -> CS
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/

#include "main.h"
#include "ff_gen_drv.h"
#include "diskio.h"

#include <stdint.h>


/* -------------------------------------------------------------------------- */
/* SPI HANDLE                                                                 */
/* -------------------------------------------------------------------------- */

extern SPI_HandleTypeDef hspi2;


/* -------------------------------------------------------------------------- */
/* SD CARD STATUS                                                             */
/* -------------------------------------------------------------------------- */

static volatile DSTATUS Stat = STA_NOINIT;


/* -------------------------------------------------------------------------- */
/* SD CARD TYPE                                                               */
/* -------------------------------------------------------------------------- */

#define SD_TYPE_UNKNOWN      0x00
#define SD_TYPE_SDSC         0x01
#define SD_TYPE_SDHC         0x02
#define SD_TYPE_SDV2         0x04

static uint8_t CardType = SD_TYPE_UNKNOWN;


/* -------------------------------------------------------------------------- */
/* SD CARD COMMANDS                                                           */
/* -------------------------------------------------------------------------- */

#define CMD0     (0)
#define CMD1     (1)
#define CMD8     (8)
#define CMD9     (9)
#define CMD10    (10)
#define CMD12    (12)
#define CMD16    (16)
#define CMD17    (17)
#define CMD18    (18)
#define CMD23    (23)
#define CMD24    (24)
#define CMD25    (25)
#define CMD55    (55)
#define CMD58    (58)
#define CMD59    (59)

#define ACMD13   (13)
#define ACMD23   (23)
#define ACMD41   (41)


/* -------------------------------------------------------------------------- */
/* SD CARD RESPONSE                                                           */
/* -------------------------------------------------------------------------- */

#define TOKEN_START_BLOCK   0xFE
#define TOKEN_MULTI_WRITE   0xFC
#define TOKEN_STOP_TRAN     0xFD


/* -------------------------------------------------------------------------- */
/* FUNCTION PROTOTYPES                                                        */
/* -------------------------------------------------------------------------- */

DSTATUS USER_initialize(BYTE pdrv);

DSTATUS USER_status(BYTE pdrv);

DRESULT USER_read(
    BYTE pdrv,
    BYTE *buff,
    DWORD sector,
    UINT count
);

#if _USE_WRITE == 1

DRESULT USER_write(
    BYTE pdrv,
    const BYTE *buff,
    DWORD sector,
    UINT count
);

#endif


#if _USE_IOCTL == 1

DRESULT USER_ioctl(
    BYTE pdrv,
    BYTE cmd,
    void *buff
);

#endif


/* -------------------------------------------------------------------------- */
/* DRIVER STRUCTURE                                                           */
/* -------------------------------------------------------------------------- */

Diskio_drvTypeDef USER_Driver =
{
    USER_initialize,
    USER_status,
    USER_read,

#if _USE_WRITE
    USER_write,
#endif

#if _USE_IOCTL
    USER_ioctl,
#endif
};


/* -------------------------------------------------------------------------- */
/* SPI LOW LEVEL FUNCTIONS                                                    */
/* -------------------------------------------------------------------------- */

static uint8_t SD_SPI_TxRx(uint8_t data)
{
    uint8_t rx = 0xFF;

    HAL_SPI_TransmitReceive(
        &hspi2,
        &data,
        &rx,
        1,
        100
    );

    return rx;
}


/* -------------------------------------------------------------------------- */
/* CHIP SELECT                                                                */
/* -------------------------------------------------------------------------- */

static void SD_CS_Low(void)
{
    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_RESET
    );
}


/* -------------------------------------------------------------------------- */

static void SD_CS_High(void)
{
    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_SET
    );
}


/* -------------------------------------------------------------------------- */
/* SEND CLOCKS                                                                */
/* -------------------------------------------------------------------------- */

static void SD_SendClock(uint8_t count)
{
    while (count--)
    {
        SD_SPI_TxRx(0xFF);
    }
}


/* -------------------------------------------------------------------------- */
/* WAIT READY                                                                 */
/* -------------------------------------------------------------------------- */

static uint8_t SD_WaitReady(uint32_t timeout)
{
    uint8_t response;

    uint32_t start = HAL_GetTick();


    do
    {
        response = SD_SPI_TxRx(0xFF);

        if (response == 0xFF)
        {
            return 1;
        }

    } while ((HAL_GetTick() - start) < timeout);


    return 0;
}


/* -------------------------------------------------------------------------- */
/* SEND COMMAND                                                               */
/* -------------------------------------------------------------------------- */

static uint8_t SD_SendCommand(
    uint8_t cmd,
    uint32_t arg
)
{
    uint8_t response;

    uint8_t crc = 0x01;


    /*
     * CMD0 and CMD8 require valid CRC
     */

    if (cmd == CMD0)
    {
        crc = 0x95;
    }

    if (cmd == CMD8)
    {
        crc = 0x87;
    }


    /*
     * CMD12 needs one extra byte before command.
     */

    if (cmd == CMD12)
    {
        SD_SPI_TxRx(0xFF);
    }


    /*
     * Command packet
     */

    SD_SPI_TxRx(
        0x40 | cmd
    );

    SD_SPI_TxRx(
        (uint8_t)(arg >> 24)
    );

    SD_SPI_TxRx(
        (uint8_t)(arg >> 16)
    );

    SD_SPI_TxRx(
        (uint8_t)(arg >> 8)
    );

    SD_SPI_TxRx(
        (uint8_t)arg
    );

    SD_SPI_TxRx(crc);


    /*
     * Wait for response.
     */

    for (uint8_t n = 0; n < 10; n++)
    {
        response = SD_SPI_TxRx(0xFF);

        if (!(response & 0x80))
        {
            return response;
        }
    }


    return 0xFF;
}


/* -------------------------------------------------------------------------- */
/* INITIALIZE SD CARD                                                         */
/* -------------------------------------------------------------------------- */

DSTATUS USER_initialize(BYTE pdrv)
{
    uint8_t response;

    uint8_t ocr[4];

    uint32_t start;


    (void)pdrv;


    Stat = STA_NOINIT;

    CardType = SD_TYPE_UNKNOWN;


    /*
     * CS HIGH
     */

    SD_CS_High();


    /*
     * SD card requires at least 74 clock cycles
     * with CS HIGH.
     */

    SD_SendClock(10);


    /*
     * Select card
     */

    SD_CS_Low();


    /*
     * CMD0
     */

    response = SD_SendCommand(
        CMD0,
        0
    );


    if (response != 0x01)
    {
        SD_CS_High();

        SD_SendClock(1);

        return Stat;
    }


    /*
     * CMD8
     *
     * Check SD version.
     */

    response = SD_SendCommand(
        CMD8,
        0x000001AA
    );


    if (response == 0x01)
    {
        /*
         * SD V2 card
         */

        ocr[0] = SD_SPI_TxRx(0xFF);
        ocr[1] = SD_SPI_TxRx(0xFF);
        ocr[2] = SD_SPI_TxRx(0xFF);
        ocr[3] = SD_SPI_TxRx(0xFF);


        if ((ocr[2] != 0x01) ||
            (ocr[3] != 0xAA))
        {
            SD_CS_High();

            SD_SPI_TxRx(0xFF);

            return Stat;
        }


        /*
         * ACMD41
         */

        start = HAL_GetTick();


        do
        {
            SD_SendCommand(
                CMD55,
                0
            );

            response =
                SD_SendCommand(
                    ACMD41,
                    0x40000000
                );

            if ((HAL_GetTick() - start) > 2000)
            {
                SD_CS_High();

                SD_SPI_TxRx(0xFF);

                return Stat;
            }

        } while (response != 0x00);


        /*
         * CMD58
         *
         * Read OCR.
         */

        response =
            SD_SendCommand(
                CMD58,
                0
            );


        if (response != 0x00)
        {
            SD_CS_High();

            SD_SPI_TxRx(0xFF);

            return Stat;
        }


        ocr[0] = SD_SPI_TxRx(0xFF);
        ocr[1] = SD_SPI_TxRx(0xFF);
        ocr[2] = SD_SPI_TxRx(0xFF);
        ocr[3] = SD_SPI_TxRx(0xFF);


        /*
         * CCS bit
         *
         * OCR byte 0 bit 6
         */

        if (ocr[0] & 0x40)
        {
            CardType =
                SD_TYPE_SDHC;
        }
        else
        {
            CardType =
                SD_TYPE_SDV2;
        }
    }
    else
    {
        /*
         * SD V1 / MMC
         */

        /*
         * Try ACMD41.
         */

        start = HAL_GetTick();


        do
        {
            SD_SendCommand(
                CMD55,
                0
            );

            response =
                SD_SendCommand(
                    ACMD41,
                    0
                );


            if ((HAL_GetTick() - start) > 2000)
            {
                break;
            }

        } while (response != 0x00);


        if (response == 0x00)
        {
            /*
             * SD V1
             */

            CardType =
                SD_TYPE_SDSC;
        }
        else
        {
            /*
             * Try MMC CMD1
             */

            start = HAL_GetTick();


            do
            {
                response =
                    SD_SendCommand(
                        CMD1,
                        0
                    );


                if ((HAL_GetTick() - start) > 2000)
                {
                    break;
                }

            } while (response != 0x00);


            if (response != 0x00)
            {
                SD_CS_High();

                SD_SPI_TxRx(0xFF);

                return Stat;
            }


            CardType =
                SD_TYPE_SDSC;
        }
    }


    /*
     * SDSC cards use byte addressing.
     * Set block length = 512 bytes.
     */

    if (CardType == SD_TYPE_SDSC ||
        CardType == SD_TYPE_SDV2)
    {
        response =
            SD_SendCommand(
                CMD16,
                512
            );


        if (response != 0x00)
        {
            SD_CS_High();

            SD_SPI_TxRx(0xFF);

            return Stat;
        }
    }


    /*
     * Deselect card.
     */

    SD_CS_High();

    SD_SPI_TxRx(0xFF);


    /*
     * Initialization successful.
     */

    Stat &= ~STA_NOINIT;


    return Stat;
}


/* -------------------------------------------------------------------------- */
/* GET STATUS                                                                 */
/* -------------------------------------------------------------------------- */

DSTATUS USER_status(BYTE pdrv)
{
    (void)pdrv;

    return Stat;
}


/* -------------------------------------------------------------------------- */
/* READ SECTORS                                                               */
/* -------------------------------------------------------------------------- */

DRESULT USER_read(
    BYTE pdrv,
    BYTE *buff,
    DWORD sector,
    UINT count
)
{
    uint8_t response;

    (void)pdrv;


    if (count == 0)
    {
        return RES_PARERR;
    }


    if (Stat & STA_NOINIT)
    {
        return RES_NOTRDY;
    }


    /*
     * Convert sector address.
     *
     * SDHC uses block address.
     * SDSC uses byte address.
     */

    if (CardType == SD_TYPE_SDHC)
    {
        /* sector remains unchanged */
    }
    else
    {
        sector *= 512;
    }


    SD_CS_Low();


    /*
     * Single block read
     */

    if (count == 1)
    {
        response =
            SD_SendCommand(
                CMD17,
                sector
            );


        if (response != 0x00)
        {
            SD_CS_High();

            SD_SPI_TxRx(0xFF);

            return RES_ERROR;
        }


        /*
         * Wait for data token.
         */

        uint32_t start =
            HAL_GetTick();


        do
        {
            response =
                SD_SPI_TxRx(0xFF);


            if ((HAL_GetTick() - start) > 500)
            {
                SD_CS_High();

                SD_SPI_TxRx(0xFF);

                return RES_ERROR;
            }

        } while (response != TOKEN_START_BLOCK);


        /*
         * Read 512 bytes
         */

        for (uint16_t i = 0; i < 512; i++)
        {
            buff[i] =
                SD_SPI_TxRx(0xFF);
        }


        /*
         * Read CRC
         */

        SD_SPI_TxRx(0xFF);

        SD_SPI_TxRx(0xFF);
    }


    /*
     * Multiple block read
     */

    else
    {
        response =
            SD_SendCommand(
                CMD18,
                sector
            );


        if (response != 0x00)
        {
            SD_CS_High();

            SD_SPI_TxRx(0xFF);

            return RES_ERROR;
        }


        while (count--)
        {
            uint32_t start =
                HAL_GetTick();


            do
            {
                response =
                    SD_SPI_TxRx(0xFF);


                if ((HAL_GetTick() - start) > 500)
                {
                    SD_SendCommand(
                        CMD12,
                        0
                    );

                    SD_CS_High();

                    SD_SPI_TxRx(0xFF);

                    return RES_ERROR;
                }

            } while (response != TOKEN_START_BLOCK);


            for (uint16_t i = 0; i < 512; i++)
            {
                *buff++ =
                    SD_SPI_TxRx(0xFF);
            }


            SD_SPI_TxRx(0xFF);
            SD_SPI_TxRx(0xFF);
        }


        /*
         * Stop transmission.
         */

        SD_SendCommand(
            CMD12,
            0
        );
    }


    SD_CS_High();

    SD_SPI_TxRx(0xFF);


    return RES_OK;
}


/* -------------------------------------------------------------------------- */
/* WRITE SECTORS                                                              */
/* -------------------------------------------------------------------------- */

#if _USE_WRITE == 1

DRESULT USER_write(
    BYTE pdrv,
    const BYTE *buff,
    DWORD sector,
    UINT count
)
{
    uint8_t response;


    (void)pdrv;


    if (count == 0)
    {
        return RES_PARERR;
    }


    if (Stat & STA_NOINIT)
    {
        return RES_NOTRDY;
    }


    /*
     * Convert address.
     */

    if (CardType == SD_TYPE_SDHC)
    {
        /* block addressing */
    }
    else
    {
        sector *= 512;
    }


    SD_CS_Low();


    /*
     * Single block write
     */

    if (count == 1)
    {
        response =
            SD_SendCommand(
                CMD24,
                sector
            );


        if (response != 0x00)
        {
            SD_CS_High();

            SD_SPI_TxRx(0xFF);

            return RES_ERROR;
        }


        /*
         * Data token
         */

        SD_SPI_TxRx(
            TOKEN_START_BLOCK
        );


        /*
         * Send 512 bytes.
         */

        for (uint16_t i = 0; i < 512; i++)
        {
            SD_SPI_TxRx(
                buff[i]
            );
        }


        /*
         * Dummy CRC
         */

        SD_SPI_TxRx(0xFF);

        SD_SPI_TxRx(0xFF);


        /*
         * Data response token
         */

        response =
            SD_SPI_TxRx(0xFF);


        if ((response & 0x1F) != 0x05)
        {
            SD_CS_High();

            SD_SPI_TxRx(0xFF);

            return RES_ERROR;
        }


        /*
         * Wait until card is not busy.
         */

        if (!SD_WaitReady(500))
        {
            SD_CS_High();

            SD_SPI_TxRx(0xFF);

            return RES_ERROR;
        }
    }


    /*
     * Multiple block write
     */

    else
    {
        /*
         * Tell card number of blocks.
         */

        SD_SendCommand(
            CMD23,
            count
        );


        response =
            SD_SendCommand(
                CMD25,
                sector
            );


        if (response != 0x00)
        {
            SD_CS_High();

            SD_SPI_TxRx(0xFF);

            return RES_ERROR;
        }


        while (count--)
        {
            /*
             * Start multi-block token.
             */

            SD_SPI_TxRx(
                TOKEN_MULTI_WRITE
            );


            for (uint16_t i = 0; i < 512; i++)
            {
                SD_SPI_TxRx(
                    *buff++
                );
            }


            /*
             * Dummy CRC
             */

            SD_SPI_TxRx(0xFF);

            SD_SPI_TxRx(0xFF);


            /*
             * Data response
             */

            response =
                SD_SPI_TxRx(0xFF);


            if ((response & 0x1F) != 0x05)
            {
                SD_CS_High();

                SD_SPI_TxRx(0xFF);

                return RES_ERROR;
            }


            if (!SD_WaitReady(500))
            {
                SD_CS_High();

                SD_SPI_TxRx(0xFF);

                return RES_ERROR;
            }
        }


        /*
         * Stop multi-block write.
         */

        SD_SPI_TxRx(
            TOKEN_STOP_TRAN
        );


        if (!SD_WaitReady(500))
        {
            SD_CS_High();

            SD_SPI_TxRx(0xFF);

            return RES_ERROR;
        }
    }


    SD_CS_High();

    SD_SPI_TxRx(0xFF);


    return RES_OK;
}

#endif


/* -------------------------------------------------------------------------- */
/* IO CONTROL                                                                 */
/* -------------------------------------------------------------------------- */

#if _USE_IOCTL == 1

DRESULT USER_ioctl(
    BYTE pdrv,
    BYTE cmd,
    void *buff
)
{
    DRESULT res = RES_OK;


    (void)pdrv;


    if (Stat & STA_NOINIT)
    {
        return RES_NOTRDY;
    }


    switch (cmd)
    {
        /*
         * Flush disk cache.
         */

        case CTRL_SYNC:

            SD_CS_Low();

            if (!SD_WaitReady(500))
            {
                res = RES_ERROR;
            }

            SD_CS_High();

            SD_SPI_TxRx(0xFF);

            break;


        /*
         * Get sector count.
         *
         * For a simple logger, we return 0 here.
         * FatFs can still mount and use the card.
         */

        case GET_SECTOR_COUNT:

            *(DWORD *)buff = 0;

            res = RES_OK;

            break;


        /*
         * Sector size.
         */

        case GET_SECTOR_SIZE:

            *(WORD *)buff = 512;

            res = RES_OK;

            break;


        /*
         * Erase block size.
         */

        case GET_BLOCK_SIZE:

            *(DWORD *)buff = 1;

            res = RES_OK;

            break;


        default:

            res = RES_PARERR;

            break;
    }


    return res;
}

#endif
