#include "init.h"
#include "gpio.h"
#include "tusb.h"
#include "pinmap.h"

#include "stm32h7xx.h"

void usb_init(void) {
    gpio_setMode(USB_DM, ALTERNATE | ALTERNATE_FUNC(10));
    gpio_setMode(USB_DP, ALTERNATE | ALTERNATE_FUNC(10));
    gpio_setOutputSpeed(USB_DM, HIGH);
    gpio_setOutputSpeed(USB_DP, HIGH);

    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_USB2OTGFSEN;
    __DSB();

    PWR->CR3 |= PWR_CR3_USB33DEN; // HAL_PWREx_EnableUSBVoltageDetector
    while((PWR->CR3 & PWR_CR3_USB33RDY) == 0) ; // Wait

    USB_OTG_FS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN
                        | USB_OTG_GOTGCTL_BVALOVAL;
    USB_OTG_FS->GUSBCFG |= USB_OTG_GUSBCFG_FDMOD;

    NVIC_SetPriority(OTG_FS_IRQn, 14);
    tusb_init();
}

void irq_init(void) {
    NVIC_SetPriorityGrouping(7); // This should disable interrupt nesting
    SysTick_Config(399999); // 1ms tick
    __enable_irq();
}

void gpio_init() {
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOBEN
                 |  RCC_AHB4ENR_GPIOCEN | RCC_AHB4ENR_GPIODEN
                 |  RCC_AHB4ENR_GPIOEEN | RCC_AHB4ENR_GPIOHEN;
    __DSB();

    GPIOA->OSPEEDR=0xaaaaaaaa; //Default to 50MHz speed for all GPIOS
    GPIOB->OSPEEDR=0xaaaaaaaa;
    GPIOC->OSPEEDR=0xaaaaaaaa;
    GPIOD->OSPEEDR=0xaaaaaaaa;
    GPIOE->OSPEEDR=0xaaaaaaaa;
    GPIOH->OSPEEDR=0xaaaaaaaa;
}