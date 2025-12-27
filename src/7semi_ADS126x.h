/*************************************************************************
//    Arduino library for the ADS1262 32-bit ADC
//    Copyright (c) 2025 7semi
//
//    This library provides support for the Texas Instruments ADS1262 ADC,
//    enabling differential and single-ended voltage measurements over SPI.
//
//    Features:
//    - Supports differential and single-ended readings
//    - Internal reference and AINCOM bias (VBIAS)
//    - SPI communication with customizable pin setup
//
//    This software is licensed under the MIT License
//    https://opensource.org/licenses/MIT
*************************************************************************/

#ifndef _7SEMI_ADS126X_H_
#define _7SEMI_ADS126X_H_

#include <Arduino.h>
#include <SPI.h>

// SPI Dummy Byte
#define CONFIG_SPI_MASTER_DUMMY   0xFF

// Register Map
#define ID_REG     0x00
#define POWER      0x01
#define INTERFACE  0x02
#define MODE0      0x03
#define MODE1      0x04
#define MODE2      0x05
#define INPMUX     0x06
#define OFCAL0     0x07
#define OFCAL1     0x08
#define OFCAL2     0x09
#define FSCAL0     0x0A
#define FSCAL1     0x0B
#define FSCAL2     0x0C
#define IDACMUX    0x0D
#define IDACMAG    0x0E
#define REFMUX     0x0F
#define TDACP      0x10
#define TDACN      0x11
#define GPIOCON    0x12
#define GPIODIR    0x13
#define GPIODAT    0x14
#define ADC2CFG    0x15
#define ADC2MUX    0x16
#define ADC2OFC0   0x17
#define ADC2OFC1   0x18
#define ADC2FSC0   0x19
#define ADC2FSC1   0x1A

// Commands
#define WREG       0x40
#define RREG       0x20
#define START      0x08
#define STOP       0x0A

#define DEVICE_ID  0x00  // Expected ID_REG upper nibble

class ADS126x_7semi {
public:
  // Constructor with optional custom pin assignments
  ADS126x_7semi(uint8_t drdy = 6, uint8_t cs = 7, uint8_t start = 5, uint8_t pwdn = 4)
    : _drdy(drdy), _cs(cs), _start(start), _pwdn(pwdn) {}

  // Initialize ADS1262/3 with default setup and optional custom SPI pins
  bool begin(int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1);

  // Read last voltage conversion
  float readVoltage();

  // Read single-ended voltage from AINx (referenced to AINCOM)
  float readSingleEnded(uint8_t ain_pos);

  // Read differential voltage between AIN+ and AIN-
  float readDifferential(uint8_t ain_pos, uint8_t ain_neg);

  // Read/write register and command methods
  uint8_t readRegister(uint8_t reg);
  void writeRegister(uint8_t addr, uint8_t data);
  void sendCommand(uint8_t cmd);
  char* readData();

  // Control methods
  void reset();
  void enableStart();
  void disableStart();
  void hardStop();

private:
  const float _vref = 2.5; // Internal reference voltage (V)
  uint8_t _drdy, _cs, _start, _pwdn;
};

#endif
