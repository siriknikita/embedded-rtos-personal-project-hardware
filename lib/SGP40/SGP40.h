/**
  ******************************************************************************
  * @file    SGP40.h
  * @author  Waveshare Team
  * @version V1.0
  * @date    Dec-2021
  * @brief

  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, WAVESHARE SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2021 Waveshare</center></h2>
  ******************************************************************************
  */
#ifndef __SGP40_H__
#define __SGP40_H__

#include "DEV_Config.h"

#include "sensirion_arch_config.h"
#include "sensirion_voc_algorithm.h"
/***********  SGP40_TEST  ****************/

#define SGP40_ADDR (0x59)

uint8_t SGP40_init(void);
uint16_t SGP40_MeasureRaw(float temp, float humi);
uint32_t SGP40_MeasureVOC(float temp, float humi);
/***********  END  ****************/

#endif
