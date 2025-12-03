#include "stm32h7xx.h"

// Mostly copied from OpenRTX rcc.cpp
void start_pll() {
    PWR->CR3 &= ~PWR_CR3_SCUEN;
    PWR->D3CR = PWR_D3CR_VOS_1 | PWR_D3CR_VOS_0;
    while((PWR->D3CR & PWR_D3CR_VOSRDY) == 0) ; // Wait

    // Enable HSE
    RCC->CR |= RCC_CR_HSEON;
    while((RCC->CR & RCC_CR_HSERDY) == 0) ; // Wait

    // Start PLL1
    RCC->PLLCKSELR =
        (RCC->PLLCKSELR & ~(RCC_PLLCKSELR_DIVM1 | RCC_PLLCKSELR_PLLSRC))
        | (5 << RCC_PLLCKSELR_DIVM1_Pos) // M=5 (25MHz/5)5MHz
        | RCC_PLLCKSELR_PLLSRC_HSE;      // HSE selected as PLL source
    RCC->PLL1DIVR = (2 - 1) << 24 // R=2
                  | (8 - 1) << 16 // Q=8
                  | (2 - 1) << 9  // P=2
                  | (160 - 1);    // N=160
    RCC->PLLCFGR |= RCC_PLLCFGR_PLL1RGE_2 // PLL ref clock between 8 and 16MHz
                 |  RCC_PLLCFGR_DIVP1EN   // Enable output P
                 |  RCC_PLLCFGR_DIVQ1EN;  // Enable output Q
    RCC->CR |= RCC_CR_PLL1ON;
    while((RCC->CR & RCC_CR_PLL1RDY) == 0) ; // Wait

    // Set clock scalers
    RCC->D1CFGR = RCC_D1CFGR_D1CPRE_DIV1   // CPU clock /1
                | RCC_D1CFGR_D1PPRE_DIV2   // D1 APB3   /2
                | RCC_D1CFGR_HPRE_DIV2;    // D1 AHB    /2
    RCC->D2CFGR = RCC_D2CFGR_D2PPRE2_DIV2  // D2 APB2   /2
                | RCC_D2CFGR_D2PPRE1_DIV2; // D2 APB1   /2
    RCC->D3CFGR = RCC_D3CFGR_D3PPRE_DIV2;  // D3 APB4   /2

    // Flash wait states
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_2WS;

    // Switch system clock to source PLL1
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL1;
    while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL1) ; // Wait

    // Start PLL3
    RCC->PLLCKSELR =
        (RCC->PLLCKSELR & ~RCC_PLLCKSELR_DIVM3)
        | (5 << RCC_PLLCKSELR_DIVM3_Pos); // M=5 (25MHz/5)5MHz
    RCC->PLLCKSELR |= RCC_PLLCKSELR_DIVM3_0
                   |  RCC_PLLCKSELR_DIVM3_2; // M=5 (25MHz/5)5MHz
    RCC->PLL3DIVR = (2 - 1) << 24 // R=2
                  | (5 - 1) << 16 // Q=5
                  | (2 - 1) << 9  // P=2
                  | (48 - 1);     // N=48
    RCC->PLLCFGR |= RCC_PLLCFGR_PLL3RGE_2 // PLL ref clock between 4 and 8 MHz
                 |  RCC_PLLCFGR_DIVQ3EN;  // Enable output Q

    RCC->CR |= RCC_CR_PLL3ON;
    while((RCC->CR & RCC_CR_PLL3RDY) == 0) ; // Wait

    // Configure USB clock source to use PLL3Q
    RCC->D2CCIP2R = (RCC->D2CCIP2R & ~RCC_D2CCIP2R_USBSEL) | RCC_D2CCIP2R_USBSEL_1;
}
