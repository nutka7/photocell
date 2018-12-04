#include <stdint.h>
#include <stm32.h>
#include "timer.h"
#include "ssd.h"

enum
{
    TIMER_STATE_ZERO,
    TIMER_STATE_RUNNING,
    TIMER_STATE_STOPPED,
};

static volatile uint32_t timerState;
static volatile uint32_t timerTime;


static void displayTime(void) {
    static const uint8_t DIGIT[] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66,
        0x6D, 0x7D, 0x07, 0x7F, 0x6F,
    };
    static const uint8_t DIGIT_POINT = 1U << 7;

    uint8_t d1, d2, d3, d4;
    uint8_t seg1, seg2, seg3, seg4;

    d1 = (timerTime / 1000) % 10;
    d2 = (timerTime / 100) % 10;
    d3 = (timerTime / 10) % 10;
    d4 = timerTime % 10;

    seg1 = DIGIT[d1];
    seg2 = DIGIT[d2];
    seg3 = DIGIT[d3] | DIGIT_POINT;
    seg4 = DIGIT[d4];

    if (!d1) seg1 = 0U;
    if (!d1 && !d2) seg2 = 0U;

    ssdDisplay(seg1, seg2, seg3, seg4);
}

static void run(void) {
    TIM3->CR1 |= TIM_CR1_CEN; // Włącz licznik w trybie zliczania w górę
    timerState = TIMER_STATE_RUNNING;
}

static void stop(void) {
    TIM3->CR1 &= ~TIM_CR1_CEN; // Wyłącz licznik
    TIM3->SR = ~TIM_SR_UIF; // Wyzeruj flagę zdarzenia uaktualnienia
    timerState = TIMER_STATE_STOPPED;
}

static void zero(void) {
    timerTime = 0;

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
    TIM3->EGR = TIM_EGR_UG;  // Wyzeruj licznik

    displayTime();
    timerState = TIMER_STATE_ZERO;
}


void TIM3_IRQHandler(void) {
    /* Zdarzenie uaktualnienia */
    if (TIM3->SR & TIM_SR_UIF) {
        TIM3->SR = ~TIM_SR_UIF;
        timerTime++;
        displayTime();
    }
}

void timerInit(void)
{
    ssdInit();

    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    /* Częstotliwość taktowania to 16 MHz. */
    TIM3->PSC = 16000 - 1;
    TIM3->ARR = 100 - 1;
    /* To daje 10 zdarzeń uaktualnienia na sekundę.
     * Większa częstotliwość nie ma sensu ze wględu na
     * czas trwania pomiaru fotokomórką.
     */

    /* Only counter overflow/underflow generates an update interrupt. */
    TIM3->CR1 = TIM_CR1_URS;

    /* Wyzeruj flagę zdarzenia uaktualnienia */
    TIM3->SR = ~TIM_SR_UIF;

    /* Włącz ustawianie flagi dla zdarzenia uaktualnienia */
    TIM3->DIER = TIM_DIER_UIE;

    NVIC_EnableIRQ(TIM3_IRQn);

    zero();
}

void timerTrigger(void) {
    switch (timerState) {
    case TIMER_STATE_ZERO:
        run();
        break;
    case TIMER_STATE_RUNNING:
        stop();
        break;
    case TIMER_STATE_STOPPED:
        zero();
        break;
    }
}
