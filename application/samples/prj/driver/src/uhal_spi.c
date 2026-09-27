#include "uhal_spi.h"

void spi_init(spi_bus_t spi_bus,bool dma_state)
{
    spi_attr_t config = { 0 };
    spi_extra_attr_t ext_config = { 0 };

    config.is_slave = false;
    config.slave_num = 1;
    config.bus_clk = SPI_CLK_FREQ;
    config.freq_mhz = 2;
    config.clk_polarity = SPI_CFG_CLK_CPOL_0;
    config.clk_phase = SPI_CFG_CLK_CPHA_0;
    config.frame_format = SPI_CFG_FRAME_FORMAT_MOTOROLA_SPI;
    config.spi_frame_format = HAL_SPI_FRAME_FORMAT_STANDARD;
    config.frame_size = HAL_SPI_FRAME_FORMAT_STANDARD;
    config.tmod = HAL_SPI_TRANS_MODE_TX;
    config.sste = SPI_CFG_SSTE_DISABLE;

    ext_config.qspi_param.wait_cycles = 0x10;

    uapi_spi_init(spi_bus, &config, &ext_config);

    if (dma_state==true)
    {
        uapi_dma_init();
        uapi_dma_open();
        spi_dma_config_t dma_cfg = {
        .src_width = 0,
        .dest_width = 0,
        .burst_length = 0,
        .priority = 0
        };
        if (uapi_spi_set_dma_mode(spi_bus, true, &dma_cfg) != ERRCODE_SUCC) {
            osal_printk("spi master set dma mode fail!\r\n");
        }
    }
    
}

uint8_t spi_wr_byte(spi_bus_t spi_bus,uint8_t txdata)
{
    uint8_t rxdata;
    spi_xfer_data_t data={
        .tx_buff=&txdata,
        .tx_bytes=1,
        .rx_buff=&rxdata,
        .rx_bytes=1,
    };
    if (uapi_spi_master_writeread(spi_bus, &data, 0xFFFFFFFF) == ERRCODE_SUCC) {
        osal_printk("spi%d master send succ!\r\n", spi_bus);
    }
    return rxdata;
}

errcode_t spi_write_buf(spi_bus_t spi_bus,uint8_t *txdata,uint32_t txlen)
{
    spi_xfer_data_t data={
        .tx_buff=txdata,
        .tx_bytes=txlen,
    };
    return uapi_spi_master_write(spi_bus, &data, 0xFFFFFFFF);
}