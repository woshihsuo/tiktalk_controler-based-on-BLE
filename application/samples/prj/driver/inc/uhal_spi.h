#ifndef UHAL_SPI
#define UHAL_SPI

#include "pinctrl.h"
#include "spi.h"
#include "soc_osal.h"
#include "app_init.h"

void spi_init(spi_bus_t spi_bus,bool dma_state);
uint8_t spi_wr_byte(spi_bus_t spi_bus,uint8_t txdata);\
errcode_t spi_write_buf(spi_bus_t spi_bus,uint8_t *txdata,uint32_t txlen);

#endif