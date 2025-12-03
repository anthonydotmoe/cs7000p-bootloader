#include "boot_jump.h"
#include "clock.h"
#include "delay.h"
#include "gpio.h"
#include "pinmap.h"
#include "init.h"

#include "tusb.h"

#define BOOT_MAGIC_GO_APP 0xDEADB007u

static void backup_init(void) {
    // ENable APB4 RTC access and unlock backup domain
    RCC->APB4ENR |= RCC_APB4ENR_RTCAPBEN;
    PWR->CR1 |= PWR_CR1_DBP;
    while (!(PWR->CR1 & PWR_CR1_DBP)) ; // Wait
}

static void bootflag_set(uint32_t v) {
    backup_init();
    RTC->BKP0R = v;
}

static uint32_t bootflag_get(void) {
    backup_init();
    return RTC->BKP0R;
}

static void bootflag_clear(void) {
    bootflag_set(0);
}

void cdc_task(void);
void led_blinking_task(void);
void printUnsignedInt(unsigned int x);

volatile unsigned int g_tickCount = 0;

void SysTick_Handler() {
    g_tickCount++;
}

void OTG_FS_IRQHandler(void) {
    tud_int_handler(0);
}

void reboot_into_application(void) {
    bootflag_set(BOOT_MAGIC_GO_APP);
    NVIC_SystemReset();
}

bool button_read(void) {
    return gpio_readPin(ALARM_KEY) == 1;
}

void main(void) {
    // At this point: Reset_Handler and SystemInit have run
    // CPU is on HSI, PLLs are in reset state, peripherals in reset state

    uint32_t magic = bootflag_get();

    if (magic == BOOT_MAGIC_GO_APP) {
        bootflag_clear();
        jump_to_application();
    }

    // Enter bootloader
    gpio_init();

    // Check to see if bootloader button is pressed
    delayMs(1);
    gpio_setMode(ALARM_KEY, INPUT);
    delayMs(1);
    if (gpio_readPin(ALARM_KEY) == 1) {
        // Button not pressed
        reboot_into_application();
    }

    // Button pressed, engage high speed, USB, etc.
    start_pll();
    gpioShiftReg_init();
    usb_init();
    irq_init();

    while (1) {
        tud_task();
        led_blinking_task();
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
        printUnsignedInt(g_tickCount);
    }
    btn_prev = btn;
}

void led_write(bool state) {
    if (state) {
        gpioDev_set(RED_LED);
    } else {
        gpioDev_clear(RED_LED);
    }
}

void led_blinking_task(void) {
    const uint32_t blink_interval_ms = 250;
    static uint32_t start_ms  = 0;
    static bool     led_state = false;

    // Blink every interval ms
    if (g_tickCount - start_ms < blink_interval_ms) {
        return; // not enough time
    }
    start_ms += blink_interval_ms;

    led_write(led_state);
    led_state = !led_state;
}
