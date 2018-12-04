#ifndef I2C_H
#define I2C_H

#include <stdint.h>

void i2cInit();

void i2cWriteRead(uint8_t addr,
                  const uint8_t *txBuf, uint32_t m,
                  uint8_t *rxBuf, uint32_t n);

#endif // I2C_H
