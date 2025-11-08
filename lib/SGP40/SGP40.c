#include "SGP40.h"

uint8_t SGP40_CMD_FEATURE_SET[] = {0x20, 0x2F};
uint8_t SGP40_CMD_MEASURE_TEST[] = {0X28, 0X0E};
uint8_t SGP40_CMD_SOFT_RESET[] = {0X00, 0X06};
uint8_t SGP40_CMD_HEATER_OFF[] = {0X36, 0X15};
uint8_t SGP40_CMD_MEASURE_RAW[] = {0X26, 0X0F};
uint8_t CRC_TABLE[] = {
    0, 49, 98, 83, 196, 245, 166, 151, 185, 136, 219, 234, 125, 76, 31, 46,
    67, 114, 33, 16, 135, 182, 229, 212, 250, 203, 152, 169, 62, 15, 92, 109,
    134, 183, 228, 213, 66, 115, 32, 17, 63, 14, 93, 108, 251, 202, 153, 168,
    197, 244, 167, 150, 1, 48, 99, 82, 124, 77, 30, 47, 184, 137, 218, 235,
    61, 12, 95, 110, 249, 200, 155, 170, 132, 181, 230, 215, 64, 113, 34, 19,
    126, 79, 28, 45, 186, 139, 216, 233, 199, 246, 165, 148, 3, 50, 97, 80,
    187, 138, 217, 232, 127, 78, 29, 44, 2, 51, 96, 81, 198, 247, 164, 149,
    248, 201, 154, 171, 60, 13, 94, 111, 65, 112, 35, 18, 133, 180, 231, 214,
    122, 75, 24, 41, 190, 143, 220, 237, 195, 242, 161, 144, 7, 54, 101, 84,
    57, 8, 91, 106, 253, 204, 159, 174, 128, 177, 226, 211, 68, 117, 38, 23,
    252, 205, 158, 175, 56, 9, 90, 107, 69, 116, 39, 22, 129, 176, 227, 210,
    191, 142, 221, 236, 123, 74, 25, 40, 6, 55, 100, 85, 194, 243, 160, 145,
    71, 118, 37, 20, 131, 178, 225, 208, 254, 207, 156, 173, 58, 11, 88, 105,
    4, 53, 102, 87, 192, 241, 162, 147, 189, 140, 223, 238, 121, 72, 27, 42,
    193, 240, 163, 146, 5, 52, 103, 86, 120, 73, 26, 43, 188, 141, 222, 239,
    130, 179, 224, 209, 70, 119, 36, 21, 59, 10, 89, 104, 255, 206, 157, 172};

// Without_humidity_compensation
// sgp40_measure_raw + 2*humi + CRC + 2*temp + CRC
uint8_t WITHOUT_HUM_COMP[] = {0X26, 0X0F, 0X80, 0X00, 0XA2, 0X66, 0X66, 0X93}; // default Temperature=25 Humidity=50
uint8_t WITH_HUM_COMP[] = {0x26, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};    // Manual input

/******************************************************************************
  function:	Send one byte of data to  I2C dev
  parameter:
            Addr: Register address
            Value: Write to the value of the register
  Info:
******************************************************************************/

void SGP40_Write_Byte(uint8_t RegAddr, uint8_t value)
{
    DEV_I2C_Write_Byte(SENSOR_I2C_PORT, SGP40_ADDR, RegAddr, value);
    return;
}

void SGP40_Write_NByte(uint8_t *RegAddr, uint8_t value)
{
    DEV_I2C_Write_nByte(SENSOR_I2C_PORT, SGP40_ADDR, RegAddr, value);
    return;
}
/******************************************************************************
  function:	 read one byte of data to  I2C dev
  parameter:
            Addr: Register address
  Info:
******************************************************************************/
static uint16_t SGP40_ReadByte()
{

    uint8_t Rbuf[3];
    i2c_read_blocking(SENSOR_I2C_PORT, SGP40_ADDR, Rbuf, 3, false);
    return (Rbuf[0] << 8) | Rbuf[1];
}

uint8_t crc_value(uint8_t msb, uint8_t lsb)
{
    uint8_t crc = 0xff;
    crc ^= msb;
    crc = CRC_TABLE[crc];
    if (lsb != 0)
    {
        crc ^= lsb;
        crc = CRC_TABLE[crc];
    }
    return crc;
}

/******************************************************************************
  function:	TSL2591 Initialization
  parameter:
  Info:
******************************************************************************/
VocAlgorithmParams voc_algorithm_params;
uint8_t SGP40_init(void)
{
    SGP40_Write_Byte(SGP40_CMD_FEATURE_SET[0], SGP40_CMD_FEATURE_SET[1]);
    DEV_Delay_ms(250);
    // printf("%x\n",I2C_ReadByte(SGP40_ADDR,0,3));
    if (SGP40_ReadByte() != 0x3220) // 0x4B00 is failed,0xD400 pass
        printf("Self test failed");
    SGP40_Write_Byte(SGP40_CMD_MEASURE_TEST[0], SGP40_CMD_MEASURE_TEST[1]);
    DEV_Delay_ms(250);
    // printf("%x\n",I2C_ReadByte(SGP40_ADDR,0,3));
    if (SGP40_ReadByte() != 0xD400) // 0x4B00 is failed,0xD400 pass
        printf("Self test failed");
    VocAlgorithm_init(&voc_algorithm_params);
}

uint16_t SGP40_MeasureRaw(float temp, float humi)
{

    uint16_t h = humi * 0xffff / 100;
    uint8_t paramh[2] = {h >> 8, h & 0xff};
    uint8_t crch = crc_value(paramh[0], paramh[1]);
    // printf("%d   %d \n",paramh[0],paramh[1]);
    uint16_t t = (temp + 45) * 0xffff / 175;
    uint8_t paramt[2] = {t >> 8, t & 0xff};
    uint8_t crct = crc_value(paramt[0], paramt[1]);
    // printf("%d   %d \n",paramt[0],paramt[1]);
    WITH_HUM_COMP[2] = paramh[0];
    WITH_HUM_COMP[3] = paramh[1];
    WITH_HUM_COMP[4] = (int)crch;
    WITH_HUM_COMP[5] = paramt[0];
    WITH_HUM_COMP[6] = paramt[1];
    WITH_HUM_COMP[7] = (int)crct;
    SGP40_Write_NByte(WITH_HUM_COMP, 8);
    DEV_Delay_ms(31);
    return SGP40_ReadByte();
}

    
uint32_t SGP40_MeasureVOC(float temp, float humi)
{


    int32_t voc_index;

    uint16_t sraw = SGP40_MeasureRaw(temp, humi);
    // printf("sraw = %d\r\n",sraw);
    
    VocAlgorithm_process(&voc_algorithm_params, sraw, &voc_index);

    return voc_index;
}
