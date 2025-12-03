#include "delay.h"
#include "gpio.h"
#include "pinmap.h"

#include "tusb.h"

void cdc_task(void);

void OTG_FS_IRQHandler(void) {
    tud_int_handler(0);
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

    gpio_setMode(ALARM_KEY, INPUT);
    gpio_setMode(LCD_BACKLIGHT, OUTPUT);
    gpio_setPin(LCD_BACKLIGHT);
}

void usb_init() {
    gpio_setMode(USB_DM, ALTERNATE | ALTERNATE_FUNC(10));
    gpio_setMode(USB_DP, ALTERNATE | ALTERNATE_FUNC(10));
    gpio_setOutputSpeed(USB_DM, HIGH);
    gpio_setOutputSpeed(USB_DP, HIGH);

    // Enable HSI48 oscillator
    RCC->CR |= RCC_CR_HSI48ON;
    while((RCC->CR & RCC_CR_HSI48RDY) == 0) ; // Wait

    // Configure OTG2 to use HSI48
    RCC->D2CCIP2R = (RCC->D2CCIP2R & ~RCC_D2CCIP2R_USBSEL) | RCC_D2CCIP2R_USBSEL;

    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_USB2OTGFSEN;
    __DSB();

    PWR->CR3 |= PWR_CR3_USB33DEN; // HAL_PWREx_EnableUSBVolatageDetector
    while((PWR->CR3 & PWR_CR3_USB33RDY) == 0) ; // Wait

    USB_OTG_FS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN
                        | USB_OTG_GOTGCTL_BVALOVAL;
    USB_OTG_FS->GUSBCFG |= USB_OTG_GUSBCFG_FDMOD;

    NVIC_SetPriority(OTG_FS_IRQn, 14);

    tusb_init();
}

bool button_read(void) {
    return gpio_readPin(ALARM_KEY) == 1;
}

void main(void) {
    gpio_init();

    usb_init();

    while(1) {
        tud_task();
        cdc_task();
    }

}

void printUnsignedInt(unsigned int x) {
    static const char hexdigits[]="0123456789ABCDEF";
    char result[] = "0x........\r\n";
    for(int i = 9; i >= 2; i--) {
        result[i] = hexdigits[x & 0xf];
        x >>= 4;
    }

    tud_cdc_write(result, sizeof(result) - 1);
    tud_cdc_write_flush();
}

void cdc_task(void) {
    if (tud_cdc_available()) {
        char buf[64];
        uint32_t count = tud_cdc_read(buf, sizeof(buf));
        (void)count;

        // Echo back
        tud_cdc_write(buf, count);
        tud_cdc_write_flush();
    }

    static bool btn_prev = 0;
    const  bool btn      = button_read();

    if ((btn_prev == false) && (btn != false)) {
        SystemCoreClockUpdate();
        printUnsignedInt(SystemCoreClock);

        printUnsignedInt(RCC->CFGR);
        printUnsignedInt(RCC->PLLCKSELR);
        printUnsignedInt(RCC->PLL1DIVR);
        printUnsignedInt(RCC->PLLCFGR);

    }
    btn_prev = btn;
}