#include "i2c.h"
#include <stm32.h>
#include <gpio.h>
#include "ssd.h"

#define I2C_SPEED_HZ 100000
#define PCLK1_MHZ 16

void i2cInit(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    /* Linia SCL */
    GPIOafConfigure(GPIOB, 8, GPIO_OType_OD,
                    GPIO_Low_Speed, GPIO_PuPd_NOPULL,
                    GPIO_AF_I2C1);

    /* Linia SDA */
    GPIOafConfigure(GPIOB, 9, GPIO_OType_OD,
                    GPIO_Low_Speed, GPIO_PuPd_NOPULL,
                    GPIO_AF_I2C1);

    /* Konfiguruj szynę w wersji podstawowej */
    I2C1->CR1 = 0;

    /* Konfiguruj częstotliwość taktowania szyny */
    I2C1->CR2 = PCLK1_MHZ;
    I2C1->CCR = (PCLK1_MHZ * 1000000) / (I2C_SPEED_HZ << 1);
    I2C1->TRISE = PCLK1_MHZ + 1;

    /* Włącz I2C */
    I2C1->CR1 |= I2C_CR1_PE;
}

/* Zapisz m bajtów i odczytaj n bajtów do/z urządzenia o adresie addr. */
void i2cWriteRead(uint8_t addr,
                  const uint8_t *txBuf, uint32_t m,
                  uint8_t *rxBuf, uint32_t n)
{
    uint32_t i = 0; // bytes written
    uint32_t j = 0; // bytes read

    if (m == 0) {
        goto READ;
    }

    /** ZAPISYWANIE **/

    /* Zainicjuj transmisję sygnału START */
    I2C1->CR1 |= I2C_CR1_START;

    /* Czekaj na zakończenie transmisji bitu START */
    while (!(I2C1->SR1 & I2C_SR1_SB));

    /* Zainicjuj wysyłanie 7-bitowego adresu slave’a, tryb MT */
    I2C1->DR = addr << 1;

    /* Czekaj na zakończenie transmisji adresu */
    while (!(I2C1->SR1 & I2C_SR1_ADDR));

    /* Skasuj bit ADDR przez odczytanie rejestru SR2 po odczytaniu rejestru SR1 */
    I2C1->SR2;

    /* Zainicjuj wysłanie bajtu */
    I2C1->DR = txBuf[i++];

    while (i < m) {
        /* Czekaj na opróżnienie kolejki nadawczej */
        while(!(I2C1->SR1 & I2C_SR1_TXE));

        I2C1->DR = txBuf[i++];
    }

    /* Czekaj na zakończenie transmisji */
    while (!(I2C1->SR1 & I2C_SR1_BTF));

    if (n == 0) {
        /* Zainicjuj transmisję sygnału STOP */
        I2C1->CR1 |= I2C_CR1_STOP;
        return;
    }

    /** CZYTANIE **/

READ:
    I2C1->CR1 |= I2C_CR1_START;

    while (!(I2C1->SR1 & I2C_SR1_SB));

    /* Zainicjuj wysyłanie 7-bitowego adresu slave’a, tryb MR */
    I2C1->DR = (addr << 1) | 1U;

    if (n == 1) {
        /* Ponieważ ma być odebrany tylko jeden bajt, ustaw wysłanie sygnału NACK */
        I2C1->CR1 &= ~I2C_CR1_ACK;
    } else {
        I2C1->CR1 |= I2C_CR1_ACK;
    }

    while (!(I2C1->SR1 & I2C_SR1_ADDR));

    I2C1->SR2;

    while (j < n) {
        if (j + 1 == n) {
            /* Po odebraniu przedostatniego bajtu ustawiamy
             * wysłanie sygnału NACK i STOP */
            I2C1->CR1 &= ~I2C_CR1_ACK;
            I2C1->CR1 |= I2C_CR1_STOP;
        }

        while (!(I2C1->SR1 & I2C_SR1_RXNE));

        rxBuf[j++] = I2C1->DR;
    }
}
