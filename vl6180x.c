#include <stddef.h>
#include <stm32.h>
#include <gpio.h>
#include "i2c.h"
#include "timer.h"

#define VL6180X_ADDR 0x29

#define VL6180X_GPIO1_GPIO GPIOA
#define VL6180X_GPIO1_PIN 6

static void WriteByte(uint16_t reg, uint8_t data) {
    uint8_t bytes[3];

    bytes[0] = (reg >> 8) & 0xFF;
    bytes[1] = reg & 0xFF;
    bytes[2] = data;

    i2cWriteRead(VL6180X_ADDR, bytes, 3, NULL, 0);
}

static uint8_t ReadByte(uint16_t reg) {
    uint8_t wbytes[2];
    uint8_t rbytes[1];

    wbytes[0] = (reg >> 8) & 0xFF;
    wbytes[1] = reg & 0xFF;

    i2cWriteRead(VL6180X_ADDR, wbytes, 2, rbytes, 1);
    return rbytes[0];
}

static void loadInitSettings(void) {
    // Mandatory : private registers
    WriteByte(0x0207, 0x01);
    WriteByte(0x0208, 0x01);
    WriteByte(0x0096, 0x00);
    WriteByte(0x0097, 0xfd);
    WriteByte(0x00e3, 0x00);
    WriteByte(0x00e4, 0x04);
    WriteByte(0x00e5, 0x02);
    WriteByte(0x00e6, 0x01);
    WriteByte(0x00e7, 0x03);
    WriteByte(0x00f5, 0x02);
    WriteByte(0x00d9, 0x05);
    WriteByte(0x00db, 0xce);
    WriteByte(0x00dc, 0x03);
    WriteByte(0x00dd, 0xf8);
    WriteByte(0x009f, 0x00);
    WriteByte(0x00a3, 0x3c);
    WriteByte(0x00b7, 0x00);
    WriteByte(0x00bb, 0x3c);
    WriteByte(0x00b2, 0x09);
    WriteByte(0x00ca, 0x09);
    WriteByte(0x0198, 0x01);
    WriteByte(0x01b0, 0x17);
    WriteByte(0x01ad, 0x00);
    WriteByte(0x00ff, 0x05);
    WriteByte(0x0100, 0x05);
    WriteByte(0x0199, 0x05);
    WriteByte(0x01a6, 0x1b);
    WriteByte(0x01ac, 0x3e);
    WriteByte(0x01a7, 0x1f);
    WriteByte(0x0030, 0x00);

    // Recommended : Public registers - See data sheet for more detail

    /* Enable GPIO1 interrupt output. Active-low. */
    WriteByte(0x0011, 0x10);

    WriteByte(0x010a, 0x30); // Set the averaging sample period
                             // (compromise between lower noise and
                             // increased execution time)
    WriteByte(0x003f, 0x46); // Sets the light and dark gain (upper
                             // nibble). Dark gain should not be
                             // changed.
    WriteByte(0x0031, 0xFF); // sets the # of range measurements after
                             // which auto calibration of system is
                             // performed
    WriteByte(0x0040, 0x63); // Set ALS integration time to 100ms
    WriteByte(0x002e, 0x01); // perform a single temperature calibration
                             // of the ranging sensor

    // Optional: Public registers - See data sheet for more detail
    WriteByte(0x003e, 0x31); // Set default ALS inter-measurement period
                             // to 500ms

    /* Minimal safe ranging inter-measurement period: 30ms */
    WriteByte(0x001b, 0x02);
    /* Interrupt mode for Range readings: Level Low (value < tresh_low) */
    WriteByte(0x0014, 0x01);
    /* Low Treshold value for ranging comparison: 255 */
    WriteByte(0x001a, 0xFF);
    /* High Treshold value for ranging comparison: 254 */
    WriteByte(0x0019, 0xFE);
}

static void vl6180xInit() {
    uint8_t reset;
    reset = ReadByte(0x016);

    if (reset == 1) { // check to see has it been initialised already
        loadInitSettings();
        WriteByte(0x016, 0x00); //change fresh out of set status to 0
    }
}

static void setLowTresholdInterrupt(void) {
    /* Interrupt mode for Range readings: Level Low (value < tresh_low) */
    WriteByte(0x0014, 0x01);
}

static void setHighTresholdInterrupt(void) {
    /* Interrupt mode for Range readings: Level High (value > tresh_high) */
    WriteByte(0x0014, 0x02);
}

static void startRangeContinuous() {
    WriteByte(0x018, 0x03);
}

#if 0
// Read range result (mm)
static uint8_t readRange() {
    return ReadByte(0x062);
}
#endif

static void clearInterrupts() {
    WriteByte(0x015, 0x07);
}

static void initGPIOInterrupt(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    __NOP();
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    /* Konfiguruj wyprowadzenie podłączone do VL1680X_GPIO1.
     * Przerwania są sygnalizowane poziomem niskim.
     */
    GPIOinConfigure(
        VL6180X_GPIO1_GPIO,
        VL6180X_GPIO1_PIN,
        GPIO_PuPd_UP,
        EXTI_Mode_Interrupt,
        EXTI_Trigger_Falling);

    NVIC_EnableIRQ(EXTI9_5_IRQn);
}

void EXTI9_5_IRQHandler(void) {
    uint8_t mode;
    EXTI->PR = 1U << VL6180X_GPIO1_PIN;

    mode = ReadByte(0x0014);

    if (mode == 0x01) { // low treshold mode
        timerTrigger();
        setHighTresholdInterrupt();
    } else {
        setLowTresholdInterrupt();
    }

    clearInterrupts();
}

/* Continuous range/ALS operation
 * A continuous range or ALS measurement is performed as follows:
 * • Write 0x03 to the SYSRANGE__START or SYSALS__START registers.
 *   In both cases, bit 1 of the register sets the mode to continuous
 * • When a measurement is completed either bit 2 or bit 5 of
 *   RESULT__INTERRUPT_STATUS_GPIO{0x4F} will be set.
 * • Results are read from RESULT__RANGE_VAL{0x62} or RESULT__ALS_VAL{0x50}.
 * • Error codes are indicated in bits [7:4] of the status registers
 *   RESULT__RANGE_STATUS{0x4D} and RESULT__ALS_STATUS{0x4E}
 * • Interrupt status flags are cleared by writing a ‘1’ to the appropriate bit of
 *   SYSTEM__INTERRUPT_CLEAR{0x15}.
 * • Thereafter, measurements will be scheduled according to the relevant inter- measurement period
 *   (see SYSRANGE__INTERMEASUREMENT_PERIOD{0x1B} or SYSALS__INTERMEASUREMENT_PERIOD{0X3E}).
 * • Continuous mode operation can be stopped by writing 0 to either START register.
 *   Continuous operation will be halted immediately and any pending measurement will be aborted.
 */

int main(void)
{
    i2cInit();
    vl6180xInit();
    initGPIOInterrupt();
    timerInit();
    startRangeContinuous();

    return 0;
}
