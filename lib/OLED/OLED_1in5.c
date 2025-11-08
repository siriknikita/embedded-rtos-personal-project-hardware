/*****************************************************************************
* | File        :   OLED_1in5.c
* | Author      :
* | Function    :   1.3inch OLED  Drive function
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2021-03-16
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
#include "OLED_1in5.h"
#include "stdio.h"

/*******************************************************************************
function:
            Hardware reset
*******************************************************************************/
static void I2C_Write_Byte(uint8_t reg,uint8_t Value)
{
    DEV_I2C_Write_Byte(OLED_I2C_PORT,OLED_1in5_ADDR, reg, Value);
}


/*******************************************************************************
function:
            Write register address and data
*******************************************************************************/
static void OLED_WriteReg(uint8_t Reg)
{
    I2C_Write_Byte(IIC_CMD,Reg);
}

static void OLED_WriteData(uint8_t Data)
{
    I2C_Write_Byte(IIC_RAM,Data);
}

/*******************************************************************************
function:
            Common register initialization
*******************************************************************************/
static void OLED_InitReg(void)
{
    // 
    // Initialize dispaly
    //

    OLED_WriteReg(0xae); // turn off oled panel

    OLED_WriteReg(0x15); // set column address
    OLED_WriteReg(0x00); // start column   0
    OLED_WriteReg(0x7f); // end column   127

    OLED_WriteReg(0x75); // set row address
    OLED_WriteReg(0x00); // start row   0
    OLED_WriteReg(0x7f); // end row   127

    OLED_WriteReg(0x81); // set contrast control
    OLED_WriteReg(0x80);

    OLED_WriteReg(0xa0); // gment remap
    OLED_WriteReg(0x51);

    OLED_WriteReg(0xa1); // start line
    OLED_WriteReg(0x00);

    OLED_WriteReg(0xa2); // display offset
    OLED_WriteReg(0x00);

    OLED_WriteReg(0xa4); // rmal display
    OLED_WriteReg(0xa8); // set multiplex ratio
    OLED_WriteReg(0x7f);

    OLED_WriteReg(0xb1); // set phase leghth
    OLED_WriteReg(0xf1);

    OLED_WriteReg(0xb3); // set dclk
    OLED_WriteReg(0x00); // 80Hz:0xc1 90Hz:0xe1   100Hz:0x00   110Hz:0x30 120Hz:0x50   130Hz:0x70     01

    OLED_WriteReg(0xab);
    OLED_WriteReg(0x01);

    OLED_WriteReg(0xb6); // set phase leghth
    OLED_WriteReg(0x0f);

    OLED_WriteReg(0xbe);
    OLED_WriteReg(0x0f);

    OLED_WriteReg(0xbc);
    OLED_WriteReg(0x08);

    OLED_WriteReg(0xd5);
    OLED_WriteReg(0x62);

    OLED_WriteReg(0xfd);
    OLED_WriteReg(0x12);

    DEV_Delay_ms(200);
    OLED_WriteReg(0xAF); //--turn on oled panel
}

/********************************************************************************
function:
            initialization
********************************************************************************/
void OLED_1in5_Init()
{
    // Set the initialization register
    OLED_InitReg();
}
/********************************************************************************
function:
            Set Windows
********************************************************************************/
void OLED_1in5_SetWindows(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend)
{
    if ((Xstart >= OLED_1in5_WIDTH) || (Ystart >= OLED_1in5_HEIGHT) ||
        (Xend >= OLED_1in5_WIDTH) || (Yend >= OLED_1in5_HEIGHT))
        return;
    OLED_WriteReg(0x15);
    OLED_WriteReg(Xstart / 2);
    OLED_WriteReg(Xend / 2);
    OLED_WriteReg(0x75);
    OLED_WriteReg(Ystart);
    OLED_WriteReg(Yend);
}

/********************************************************************************
function:
            Clear screen
********************************************************************************/
void OLED_1in5_Clear(uint8_t color)
{
    OLED_1in5_SetWindows(0, 0, OLED_1in5_WIDTH - 1, OLED_1in5_HEIGHT - 1);
    for (uint16_t i = 0; i < OLED_1in5_WIDTH * OLED_1in5_HEIGHT / 2; i++)
    {
        OLED_WriteData(0x11 * color);
    }
}
/********************************************************************************
function:
            Update all memory to OLED
********************************************************************************/
void OLED_1in5_Display(uint8_t *Image)
{
    uint8_t image [(OLED_1in5_WIDTH/2)+1];
    image[0]=0x40;
    OLED_1in5_SetWindows(0, 0, OLED_1in5_WIDTH - 1, OLED_1in5_HEIGHT - 1);

    for (uint16_t i = 0; i < OLED_1in5_HEIGHT; i++)
    {
        for(uint16_t j= 0;j<(OLED_1in5_WIDTH /2);j++)
        {
            image[1+j]=Image[j+i*(OLED_1in5_WIDTH/2)];
        }
            DEV_I2C_Write_nByte(OLED_I2C_PORT,OLED_1in5_ADDR,image, (OLED_1in5_WIDTH / 2)+1);
    }
}
