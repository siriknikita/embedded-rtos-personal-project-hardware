/*****************************************************************************
* | File        :   OLED_1in5.h
* | Author      :   
* | Function    :   1.5inch OLEDDrive function
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2022-12-21
* | Info        :
* -----------------------------------------------------------------------------
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
******************************************************************************/
#ifndef __OLED_1IN5_H
#define __OLED_1IN5_H		

#include "DEV_Config.h"

/********************************************************************************
function:	
		Define the full screen height length of the display
********************************************************************************/

#define IIC_CMD				(0X00)
#define IIC_RAM				(0X40)

#define OLED_1in5_ADDR		(0X3D)
#define OLED_1in5_WIDTH		(128)//OLED width
#define OLED_1in5_HEIGHT	(128) //OLED height


void OLED_1in5_Init(void);
void OLED_1in5_Clear(uint8_t color);
void OLED_1in5_Display(uint8_t *Image);

#endif  
	 
