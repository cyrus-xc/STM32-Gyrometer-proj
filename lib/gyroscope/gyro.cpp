#include "gyro.h"

SPI spi(SPI_MOSI_PIN, SPI_MISO_PIN, SPI_SCK_PIN, GYRO_SPI_CS, use_gpio_ssel);

void GYRO_init() {
  spi.format(8,SPI_MODE); 
  spi.frequency(SPI_SPEED_1MHz); 
  GYRO_write_reg(I3G_CTRL_REG1, GYRO_CTRL_REG1_CONFIG);
  GYRO_write_reg(I3G_CTRL_REG4, GYRO_CTRL_REG4_CONFIG);

}

uint8_t GYRO_read_reg(uint8_t reg) {
  uint8_t spi_wbuffer[] = {0x80 | reg, 0xff};
  uint8_t spi_rbuffer[] = {0x00, 0x00};
  spi.transfer(spi_wbuffer, 2, spi_rbuffer, 2, spi_cb);
  flags.wait_all(SPI_FLAG);
  return spi_rbuffer[1];
}

void GYRO_write_reg(uint8_t reg, uint8_t data) {
  uint8_t spi_wbuffer[] = {reg, data};
  uint8_t spi_rbuffer[] = {0x00, 0x00};
  spi.transfer(spi_wbuffer, 2, spi_rbuffer, 2, spi_cb);
  flags.wait_all(SPI_FLAG);
  return;
}

int16_t GYRO_X() {
  uint16_t data = 0;
  data = GYRO_read_reg(I3G_OUT_X_H) << 8;
  data |= GYRO_read_reg(I3G_OUT_X_L);
  return (int16_t)data;
}

int16_t GYRO_Y() {
  uint16_t data = 0;
  data = GYRO_read_reg(I3G_OUT_Y_H) << 8;
  data |= GYRO_read_reg(I3G_OUT_Y_L);
  return (int16_t)data;
}

int16_t GYRO_Z() {
  uint16_t data = 0;
  data = GYRO_read_reg(I3G_OUT_Z_H) << 8;
  data |= GYRO_read_reg(I3G_OUT_Z_L);
  return (int16_t)data;
}