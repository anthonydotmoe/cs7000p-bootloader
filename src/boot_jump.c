#include "stm32h7xx.h"

#define APP_BASE_ADDR 0x08100000U

typedef void (*app_entry_t)(void);

__attribute__((noreturn))
void jump_to_application(void) {
    uint32_t app_msp   = *(__IO uint32_t *)APP_BASE_ADDR;
    uint32_t app_reset = *(__IO uint32_t *)(APP_BASE_ADDR + 4U) ;

    if ((app_msp == 0xFFFFFFFFU) || (app_reset == 0xFFFFFFFFU)) {
        // No valid app, handle error?
        //while (1) { }
    }

    // 1. Disable interrupts
    __disable_irq();

    // 2. Stop SysTick
    SysTick->CTRL = 0;
    SysTick->VAL  = 0;

    // 3. Disable all NVIC interrupts & clear pending
    for (uint32_t i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    // 4. Set vector table to application base
    SCB->VTOR = APP_BASE_ADDR;

    // 5. Set MSP to app's initial stack pointer
    __set_MSP(app_msp);

    __DSB();
    __ISB();

    // 6. Jump to app's ResetHandler
    app_entry_t app_reset_handler = (app_entry_t)app_reset;
    app_reset_handler();

    // Should never reach here
    while (1) { }
}
