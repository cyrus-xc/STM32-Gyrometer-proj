#pragma once


#ifndef GYRO_H
#define GYRO_H

#include <mbed.h>

/*  Gyroscope I3G4250D driver on STM32's 32F429IDISCOVERY board
 *  This driver is adapted from the code 
 *  https://github.com/wilicw/I3G4250D/tree/main
 **/

/* Use SPI Interface to communicate with MCU.
 * SPI Pin Definition
 */
#define SPI_MOSI_PIN    PF_9
#define SPI_MISO_PIN    PF_8
#define SPI_SCK_PIN     PF_7
#define GYRO_SPI_CS     PC_1

/* I3G4250D Registers*/
#define I3G_WHO_AM_I    0x0F
#define I3G_CTRL_REG1   0x20
#define I3G_CTRL_REG2   0x21
#define I3G_CTRL_REG3   0x22
#define I3G_CTRL_REG4   0x23
#define I3G_CTRL_REG5   0x24
#define I3G_REFERENCE   0x25
#define I3G_OUT_TEMP    0x26
#define I3G_STATUS_REG  0x27
#define I3G_OUT_X_L     0x28
#define I3G_OUT_X_H     0x29
#define I3G_OUT_Y_L     0x2A
#define I3G_OUT_Y_H     0x2B
#define I3G_OUT_Z_L     0x2C
#define I3G_OUT_Z_H     0x2D
#define I3G_FIFO_CTRL_REG   0x2E
#define I3G_FIFO_SRC_REG    0x2F
#define I3G_INT1_CFG    0x30
#define I3G_INT1_SRC    0x31
#define I3G_INT1_THS_XH 0x32
#define I3G_INT1_THS_XL 0x33
#define I3G_INT1_THS_YH 0x34
#define I3G_INT1_THS_YL 0x35
#define I3G_INT1_THS_ZH 0x36
#define I3G_INT1_THS_ZL 0x37
#define I3G_INT1_DURATION   0x38

#define SPI_FLAG    1
#define SPI_MODE    3   //CS Idle on high, data transmitted on rising edge
#define SPI_SPEED_1MHz  1'000'000   
/* CTRL_REG1 configuration: DR[1:0]'BW[1:0]'PD'Xen'Yen'Zen
DR[1:0] = 0b01, ODR = 200Hz
BW[1:0] = 0b10, Cutoff = 50Hz
PD = 1, Normal mode
Xen = 1, X axis enabled
Yen = 1, Y axis enabled
Zen = 1, Z axis enabled
*/
#define GYRO_CTRL_REG1_CONFIG   0b01'10'1'1'1'1

/* CTRL_REG4 configuration: 0'BLE'FS[1:0]'-'ST[1:0]'SIM
BLE = 0, LSB @ lower address
FS[1:0] = 0b00, 245 dps
ST[1:0] = 0b00, nomal mode, self test disabled
SIM = 0, 4 wire SPI interface 
*/
#define GYRO_CTRL_REG4_CONFIG   0b0'0'00'0'00'0

typedef struct {
  SPI_HandleTypeDef *spi_handler;
  GPIO_TypeDef *CS_port;
  uint16_t CS_pin;
} gyroscope_t;

typedef struct {
  float x, y, z;
} vang_t;

extern void spi_cb(int event);
extern EventFlags flags;

void GYRO_init();
uint8_t GYRO_read_reg(uint8_t);
void GYRO_write_reg(uint8_t, uint8_t);

/* GYRO_X, _Y, _Z read range -32768~32768
 * corresponding to the scale defined in CTRL_REG4
 * current setting Full Scale = +-500 degree per sec 
 */
int16_t GYRO_X();
int16_t GYRO_Y();
int16_t GYRO_Z();


#endif