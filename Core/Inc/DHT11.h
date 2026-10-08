/*
 * DHT11.h
 *
 *  Created on: Oct 4, 2026
 *      Author: Monika
 */

#ifndef DHT11_H
#define DHT11_H

#include "main.h"

typedef struct
{
    uint8_t temperature;
    uint8_t humidity;
} DHT11_Data_t;

HAL_StatusTypeDef DHT11_Read(DHT11_Data_t *data);

#endif
