/*************************************************************************
//    Arduino library for the ADS1262 32-bit ADC
//    Copyright (c) 2025 7semi
//
//    This library is licensed under the MIT License
//    https://opensource.org/licenses/MIT
//
//    ADS1262 features:
//    - Differential and single-ended readings
//    - Internal reference and bias support
//    - SPI interface
*************************************************************************/

#include "7semi_ADS126x.h"

// Constructor with optional pin configuration handled in header

/**
 * @brief Initialize ADS1262/3 and SPI interface
 */
bool ADS126x_7semi::begin(int8_t sck, int8_t miso, int8_t mosi) {
  // DRDY is open-drain; enable pull-up so it idles HIGH
  pinMode(_drdy, INPUT_PULLUP);
  pinMode(_cs, OUTPUT);
  pinMode(_start, OUTPUT);
  pinMode(_pwdn, OUTPUT);

  // Allow custom SPI pins on ESP32 if provided
  if (sck != -1 || miso != -1 || mosi != -1) {
    SPI.begin(sck, miso, mosi, _cs);
  } else {
    SPI.begin();
  }
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE1);
  SPI.setClockDivider(SPI_CLOCK_DIV4);  // faster SPI for throughput

  int8_t device_id = readRegister(ID_REG);
  if ((device_id >> 4) != DEVICE_ID)
    return false;

  reset();
  delay(100);
  hardStop();
  delay(350);

  // Power config: Enable internal reference (bit 0) and VBIAS (bit 1)
  writeRegister(POWER, 0x11);

  writeRegister(INTERFACE, 0x00);  // CRC off, no status byte (shortest frame)
  // MODE0 DR=0x09 (~1.2 kSPS on ADS1263 ADC1). Bump to 0x0A/0x0B if you need faster.
  writeRegister(MODE0, 0x09);
  writeRegister(MODE1, 0x00);      // SINC1 (fastest, minimal digital filtering)
  writeRegister(MODE2, 0x06);      // PGA gain = 1
  writeRegister(INPMUX, 0x01);     // Default input mux
  writeRegister(OFCAL0, 0x00);     // Offset calibration
  writeRegister(OFCAL1, 0x00);
  writeRegister(OFCAL2, 0x00);
  writeRegister(FSCAL0, 0x00);     // Full-scale calibration
  writeRegister(FSCAL1, 0x00);
  writeRegister(FSCAL2, 0x40);
  writeRegister(IDACMUX, 0xBB);    // Disable all IDACs
  writeRegister(IDACMAG, 0x00);
  writeRegister(REFMUX, 0x00);     // Internal reference
  writeRegister(GPIOCON, 0x00);    // GPIO disabled
  writeRegister(GPIODIR, 0x00);
  writeRegister(GPIODAT, 0x00);
  writeRegister(ADC2CFG, 0x00);    // ADC2 disabled
  writeRegister(ADC2MUX, 0x01);
  writeRegister(ADC2OFC0, 0x00);
  writeRegister(ADC2OFC1, 0x00);
  writeRegister(ADC2FSC0, 0x00);
  writeRegister(ADC2FSC1, 0x40);

  enableStart();
  return true;
}

/**
 * @brief Perform device reset through PWDN pin
 */
void ADS126x_7semi::reset() {
  digitalWrite(_pwdn, HIGH);
  delay(100);
  digitalWrite(_pwdn, LOW);
  delay(100);
  digitalWrite(_pwdn, HIGH);
  delay(100);
}

/**
 * @brief Enable START pin to begin conversions
 */
void ADS126x_7semi::enableStart() {
  digitalWrite(_start, HIGH);
  delay(20);
}

/**
 * @brief Disable START pin to stop conversions
 */
void ADS126x_7semi::disableStart() {
  digitalWrite(_start, LOW);
  delay(20);
}

/**
 * @brief Hard stop by pulling START pin low
 */
void ADS126x_7semi::hardStop() {
  digitalWrite(_start, LOW);
  delay(100);
}

/**
 * @brief Write 1 byte to a register
 */
void ADS126x_7semi::writeRegister(uint8_t addr, uint8_t data) {
  digitalWrite(_cs, LOW);
  SPI.transfer(WREG | addr);
  SPI.transfer(0x00);  // Write 1 byte
  SPI.transfer(data);
  digitalWrite(_cs, HIGH);
  delay(2);
}

/**
 * @brief Send SPI command to ADS1262
 */
void ADS126x_7semi::sendCommand(uint8_t cmd) {
  digitalWrite(_cs, LOW);
  delay(2);
  SPI.transfer(cmd);
  delay(2);
  digitalWrite(_cs, HIGH);
}

/**
 * @brief Read ADC result (4 data bytes, status disabled)
 */
char* ADS126x_7semi::readData() {
  static char buffer[4];
  digitalWrite(_cs, LOW);
  for (int i = 0; i < 4; ++i) {
    buffer[i] = SPI.transfer(CONFIG_SPI_MASTER_DUMMY);
  }
  digitalWrite(_cs, HIGH);
  return buffer;
}

/**
 * @brief Read raw voltage from last conversion
 */
float ADS126x_7semi::readVoltage() {
  // Wait briefly for DRDY to drop; avoid missing short pulses at higher data rates
  unsigned long start = micros();
  while (digitalRead(_drdy) != LOW) {
    if (micros() - start > 2000) {
      return NAN;
    }
  }

  char* raw = readData();
  long result = ((uint32_t)(uint8_t)raw[0] << 24) |
                ((uint32_t)(uint8_t)raw[1] << 16) |
                ((uint32_t)(uint8_t)raw[2] << 8) |
                (uint8_t)raw[3];

  float resolution = _vref / pow(2, 31);
  return (float)(int32_t)result * resolution;
}

/**
 * @brief Perform a differential reading between two AIN pins
 */
float ADS126x_7semi::readDifferential(uint8_t ain_pos, uint8_t ain_neg) {
  if (ain_pos > 15 || ain_neg > 15) return NAN;

  writeRegister(INPMUX, (ain_pos << 4) | ain_neg);
  sendCommand(START);
  delay(100);

  unsigned long start = micros();
  while (digitalRead(_drdy) != LOW) {
    if (micros() - start > 2000) {
      return NAN;
    }
  }

  char* raw = readData();
  long result = ((uint32_t)(uint8_t)raw[0] << 24) |
                ((uint32_t)(uint8_t)raw[1] << 16) |
                ((uint32_t)(uint8_t)raw[2] << 8) |
                (uint8_t)raw[3];

  float resolution = _vref / pow(2, 31);
  return (float)(int32_t)result * resolution;
}

/**
 * @brief Perform single-ended reading against AINCOM (AIN10)
 */
float ADS126x_7semi::readSingleEnded(uint8_t ain_pos) {
  return readDifferential(ain_pos, 10);  // AIN10 = AINCOM
}

/**
 * @brief Read one byte from a register
 */
uint8_t ADS126x_7semi::readRegister(uint8_t reg) {
  digitalWrite(_cs, LOW);
  delayMicroseconds(2);

  SPI.transfer(0x20 | reg);
  SPI.transfer(0x00);  // Read 1 byte
  uint8_t value = SPI.transfer(0xFF);

  digitalWrite(_cs, HIGH);
  return value;
}
