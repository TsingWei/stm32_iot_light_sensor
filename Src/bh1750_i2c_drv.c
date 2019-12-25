#include "bh1750_i2c_drv.h"
#include "i2c.h"

uint8_t	BH1750_Send_Cmd(BH1750_MODE cmd)
{
	return HAL_I2C_Master_Transmit(&hi2c2, BH1750_ADDR_WRITE, (uint8_t*)&cmd, 1, 0xFFFF);
}

uint8_t BH1750_Read_Dat(uint8_t* dat)
{
	return HAL_I2C_Master_Receive(&hi2c2, BH1750_ADDR_READ, dat, 2, 0xFFFF);
}

uint16_t BH1750_Dat_To_Lux(uint8_t* dat)
{
	uint16_t lux = 0;
	lux = dat[0];
	lux <<= 8;
	lux += dat[1];
	lux = (int)(lux / 1.2);

	return lux;
}
