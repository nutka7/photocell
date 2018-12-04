#include <stm32.h>
#include <gpio.h>
#include "ssd.h"

/* Wyświetlacz 7-segmentowy:
 * @ siedem diod świecących wyświetlających cyfry
 * @ ósma dioda wyświetlająca punkt dziesiętny
 * @ wspólna anoda
 */

typedef struct SSDOut {
    GPIO_TypeDef * const gpio;
    uint32_t pin;
} SSDOut;


/* KATODY */

static const SSDOut CATH_OUT[] =
{                  // SEGMENT
    { GPIOB, 6  }, // A
    { GPIOC, 7  }, // B
    { GPIOA, 9  }, // C
    { GPIOA, 8  }, // D
    { GPIOB, 10 }, // E
    { GPIOB, 4  }, // F
    { GPIOB, 5  }, // G
    { GPIOB, 3  }, // DP
};

#define SEGMENT_NUM ((sizeof CATH_OUT) / (sizeof *CATH_OUT))

/* ANODY */

static const SSDOut ANOD_OUT[] =
{                 // DIGIT
    { GPIOB, 0 }, // 1.
    { GPIOA, 4 }, // 2.
    { GPIOA, 1 }, // 3.
    { GPIOA, 0 }, // 4.
};

#define DIGIT_NUM ((sizeof ANOD_OUT) / (sizeof *ANOD_OUT))

static volatile uint8_t DIGIT[DIGIT_NUM];

static void timInit(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;

    /* Częstotliwość taktowania to 16 MHz. */
    TIM4->PSC = 16 - 1;
    TIM4->ARR = 2500 - 1;
    /* To daje 400 zdarzeń uaktualnienia na sekundę.
     * Mniejsza częstotliwość jest nieprzyjemna dla oka. */

    /* Ustawienie bitu TIM_EGR_UG powoduje:
     * @ zainicjowanie rejestru TIMx->CNT na
     *   @@ TIMx->ARR, jeśli jest ustawiony bit TIM_CR1_DIR
     *   @@ 0, w przeciwnym razie
     * @ wyzerowanie wewnętrznego rejestru preskalera
     * @ wygenerowanie zdarzenia uaktualnienia,
     *   o ile nie jest ustawiony bit TIM_CR1_UDIS, które powoduje
     *   @@ zainicjowanie rejestru TIMx->PSC, ew. TIMx->ARR i TIMx->CCRy
     * @ ustawienie znacznika TIM_SR_UIF (i ew. wygenerowanie przerwania),
     *   o ile nie jest ustawiony bit TIM_CR1_URS
     *
     * Bit TIM_EGR_UG jest automatycznie zerowany sprzętowo.
     */
    TIM4->EGR = TIM_EGR_UG;

    /* Rejestry sprzętowe licznika --> zdarzenia zgodności */
    TIM4->CCR1 = 2000;
    /* 80% -  większa wartość powoduje cienie na sąsiednich cyfrach */

    /* Wyzeruj flagi zdarzeń uaktualnienia i zgodności (rejestr 1.). */
    TIM4->SR = ~(TIM_SR_UIF | TIM_SR_CC1IF);

    /* Włącz ustawianie flag dla zdarzeń uaktualnienia i zgodności (rejestr 1.). */
    TIM4->DIER = TIM_DIER_UIE | TIM_DIER_CC1IE;

    NVIC_EnableIRQ(TIM4_IRQn);

    /* Włączamy licznik w trybie zliczania w górę z buforowaniem rejestru TIM4->ARR. */
    TIM4->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}

void ssdDisplay(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4) {
    DIGIT[0] = d1;
    DIGIT[1] = d2;
    DIGIT[2] = d3;
    DIGIT[3] = d4;
}

static inline void outHigh(SSDOut out)
{
    out.gpio->BSRRL = 1 << out.pin;
}

static inline void outLow(SSDOut out)
{
    out.gpio->BSRRH = 1 << out.pin;
}

static void off(void)
{
    /* Anody i katody - aktywny poziom niski. */

    for (uint32_t i = 0; i < SEGMENT_NUM; ++i) {
        outHigh(CATH_OUT[i]);
    }

    for (uint32_t i = 0; i < DIGIT_NUM; ++i) {
        outHigh(ANOD_OUT[i]);
    }
}

static void displayDigit(uint8_t digit, uint8_t segments)
{
    /* Anody i katody - aktywny poziom niski. */

    outLow(ANOD_OUT[digit]);

    for (uint32_t i = 0; i < SEGMENT_NUM; ++i) {
        if (segments & 1U << i) {
            outLow(CATH_OUT[i]);
        }
    }
}


void TIM4_IRQHandler(void) {
    static uint8_t digit = 0;

    uint32_t it_status = TIM4->SR & TIM4->DIER;

    /* Zdarzenie uaktualnienia */
    if (it_status & TIM_SR_UIF) {
        TIM4->SR = ~TIM_SR_UIF;
        displayDigit(digit, DIGIT[digit]);
        digit = (digit + 1) % DIGIT_NUM;
    }

    /* Zdarzenie zgodności */
    if (it_status & TIM_SR_CC1IF) {
        TIM4->SR = ~TIM_SR_CC1IF;
        off();
    }
}

void ssdInit(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN
                    | RCC_AHB1ENR_GPIOBEN
                    | RCC_AHB1ENR_GPIOCEN;
    __NOP();

    for (uint32_t i = 0; i < SEGMENT_NUM; i++) {
        GPIOoutConfigure(
            CATH_OUT[i].gpio,
            CATH_OUT[i].pin,
            GPIO_OType_OD,
            GPIO_Low_Speed,
            GPIO_PuPd_NOPULL);
    }

    for (uint32_t i = 0; i < DIGIT_NUM; i++) {
        GPIOoutConfigure(
            ANOD_OUT[i].gpio,
            ANOD_OUT[i].pin,
            GPIO_OType_OD,
            GPIO_Low_Speed,
            GPIO_PuPd_NOPULL);
    }

    off();
    timInit();
}
