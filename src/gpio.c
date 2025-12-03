#include <stddef.h>
#include <string.h>

#include "gpio.h"
#include "pinmap.h"

static inline void setGpioAf(GPIO_TypeDef *port, uint8_t pin, const uint8_t af)
{
    if(pin < 8)
    {
        port->AFR[0] &= ~(0x0F << (pin*4));
        port->AFR[0] |=  (af   << (pin*4));
    }
    else
    {
        pin -= 8;
        port->AFR[1] &= ~(0x0F << (pin*4));
        port->AFR[1] |=  (af   << (pin*4));
    }
}

void gpio_setMode(const void *port, const uint8_t pin, const uint16_t mode)
{
    GPIO_TypeDef *p = (GPIO_TypeDef *)(port);
    uint8_t      af = (mode >> 8) & 0x0F;

    p->MODER  &= ~(3 << (pin*2));
    p->OTYPER &= ~(1 << pin);
    p->PUPDR  &= ~(3 << (pin*2));

    switch(mode & 0xFF)
    {
        case INPUT:
            // (MODE=00 TYPE=0 PUP=00)
            p->MODER  |= 0x00 << (pin*2);
            p->OTYPER |= 0x00 << pin;
            p->PUPDR  |= 0x00 << (pin*2);
            break;

        case INPUT_PULL_UP:
            // (MODE=00 TYPE=0 PUP=01)
            p->MODER  |= 0x00 << (pin*2);
            p->OTYPER |= 0x00 << pin;
            p->PUPDR  |= 0x01 << (pin*2);
            break;

        case INPUT_PULL_DOWN:
            // (MODE=00 TYPE=0 PUP=10)
            p->MODER  |= 0x00 << (pin*2);
            p->OTYPER |= 0x00 << pin;
            p->PUPDR  |= 0x02 << (pin*2);
            break;

        case ANALOG:
            // (MODE=11 TYPE=0 PUP=00)
            p->MODER  |= 0x03 << (pin*2);
            p->OTYPER |= 0x00 << pin;
            p->PUPDR  |= 0x00 << (pin*2);
            break;

        case OUTPUT:
            // (MODE=01 TYPE=0 PUP=00)
            p->MODER  |= 0x01 << (pin*2);
            p->OTYPER |= 0x00 << pin;
            p->PUPDR  |= 0x00 << (pin*2);
            break;

        case OPEN_DRAIN:
            // (MODE=01 TYPE=1 PUP=00)
            p->MODER  |= 0x01 << (pin*2);
            p->OTYPER |= 0x01 << pin;
            p->PUPDR  |= 0x00 << (pin*2);
            break;

        case OPEN_DRAIN_PU:
            // (MODE=01 TYPE=1 PUP=01)
            p->MODER  |= 0x01 << (pin*2);
            p->OTYPER |= 0x01 << pin;
            p->PUPDR  |= 0x00 << (pin*2);
            break;

        case ALTERNATE:
            // (MODE=10 TYPE=0 PUP=00)
            p->MODER  |= 0x02 << (pin*2);
            p->OTYPER |= 0x00 << pin;
            p->PUPDR  |= 0x00 << (pin*2);
            setGpioAf(p, pin, af);
            break;

        case ALTERNATE_OD:
            // (MODE=10 TYPE=1 PUP=00)
            p->MODER  |= 0x02 << (pin*2);
            p->OTYPER |= 0x01 << pin;
            p->PUPDR  |= 0x00 << (pin*2);
            setGpioAf(p, pin, af);
            break;

        case ALTERNATE_OD_PU:
            // (MODE=10 TYPE=1 PUP=01)
            p->MODER  |= 0x02 << (pin*2);
            p->OTYPER |= 0x01 << pin;
            p->PUPDR  |= 0x01 << (pin*2);
            setGpioAf(p, pin, af);
            break;

        default:
            // Default to INPUT mode
            p->MODER  |= 0x00 << (pin*2);
            p->OTYPER |= 0x00 << pin;
            p->PUPDR  |= 0x00 << (pin*2);
            break;
    }
}

void gpio_setOutputSpeed(const void *port, const uint8_t pin, const enum Speed spd)
{
    ((GPIO_TypeDef *)(port))->OSPEEDR &= ~(3 << (pin*2));   // Clear old value
    ((GPIO_TypeDef *)(port))->OSPEEDR |= spd << (pin*2);    // Set new value
}

static int gpioApi_mode(const struct gpioDev *dev, const uint8_t pin,
                        const uint16_t mode)
{
    if(pin > 15)
        return -1;

    gpio_setMode((void *) dev->priv, pin, mode);
    return 0;
}

static void gpioApi_set(const struct gpioDev *dev, const uint8_t pin)
{
    gpio_setPin((void *) dev->priv, pin);
}

static void gpioApi_clear(const struct gpioDev *dev, const uint8_t pin)
{
    gpio_clearPin((void *) dev->priv, pin);
}

static bool gpioApi_read(const struct gpioDev *dev, const uint8_t pin)
{
    uint8_t val = gpio_readPin(dev->priv, pin);
    return (val != 0) ? true : false;
}

static const struct gpioApi gpioApi =
{
    .mode  = gpioApi_mode,
    .set   = gpioApi_set,
    .clear = gpioApi_clear,
    .read  = gpioApi_read
};

const struct gpioDev GpioA = { .api = &gpioApi, .priv = GPIOA };
const struct gpioDev GpioB = { .api = &gpioApi, .priv = GPIOB };
const struct gpioDev GpioC = { .api = &gpioApi, .priv = GPIOC };
const struct gpioDev GpioD = { .api = &gpioApi, .priv = GPIOD };
const struct gpioDev GpioE = { .api = &gpioApi, .priv = GPIOE };

static uint8_t srData_extGpio[3];

static int gpioShiftReg_mode(const struct gpioDev *dev, const uint8_t pin, const uint16_t mode) {
    (void) dev;
    (void) pin;
    (void) mode;

    return -1;
}

static void spiSr_send(const uint8_t *txbuf, const size_t size) {
    for (size_t i = 0; i < size; i++) {
        uint8_t value = txbuf[i];

        for (uint8_t cnt = 0; cnt < 8; cnt++) {
            GPIOE->BSRR = (1 << 23);   // Clear PE7 (CLK)

            if (value & (0x80 >> cnt))
                GPIOE->BSRR = 1 << 9;  // Set PE9 (MOSI)
            else
                GPIOE->BSRR = 1 << 25; // Clear PE9 (MOSI)
            
            // ~70ns delay
            asm volatile("           mov   r1, #5     \n"
                         "___loop_2: cmp   r1, #0     \n"
                         "           itt   ne         \n"
                         "           subne r1, r1, #1 \n"
                         "           bne   ___loop_2  \n":::"r1");

            GPIOE->BSRR = (1 << 7);                 // Set PE7 (CLK)
    
            // ~70ns delay
            asm volatile("           mov   r1, #6     \n"
                         "___loop_3: cmp   r1, #0     \n"
                         "           itt   ne         \n"
                         "           subne r1, r1, #1 \n"
                         "           bne   ___loop_3  \n":::"r1");
        }
    }
}

void gpioShiftReg_init() {
    gpio_setMode(GPIOEXT_STR, OUTPUT);
    gpio_setMode(GPIOEXT_CLK, OUTPUT);
    gpio_setMode(GPIOEXT_DAT, OUTPUT);
    gpio_clearPin(GPIOEXT_STR);

    memset(srData_extGpio, 0x00, 3);
    spiSr_send(srData_extGpio, 3);
    gpio_setPin(GPIOEXT_STR);
}

static inline void gpioShiftReg_modify(const struct gpioDev *dev, const uint8_t pin, bool state) {
    const size_t nBytes = 3;
    const size_t byte = (24 - 1 - pin) / 8;
    const size_t bit = pin % 8;

    if (pin > 24)
        return;
    
    __disable_irq();

    if (state == true) {
        srData_extGpio[byte] |= (1 << bit);
    } else {
        srData_extGpio[byte] &= ~(1 << bit);
    }

    gpio_clearPin(GPIOEXT_STR);
    spiSr_send(srData_extGpio, nBytes);
    gpio_setPin(GPIOEXT_STR);

    __enable_irq();
}

static void gpioShiftReg_set(const struct gpioDev *dev, const uint8_t pin) {
    gpioShiftReg_modify(dev, pin, true);
}

static void gpioShiftReg_clear(const struct gpioDev *dev, const uint8_t pin) {
    gpioShiftReg_modify(dev, pin, false);
}

static bool gpioShiftReg_read(const struct gpioDev *dev, const uint8_t pin) {
    const size_t byte = (24 - 1 - pin) / 8;
    const size_t bit = pin % 8;

    if (pin > 24)
        return false;
    
    return ((srData_extGpio[byte] & (1 << bit)) != 0) ? true : false;
}

const struct gpioApi gpioShiftReg_ops = {
    .mode  = &gpioShiftReg_mode,
    .set   = &gpioShiftReg_set,
    .clear = &gpioShiftReg_clear,
    .read  = &gpioShiftReg_read
};

const struct gpioDev extGpio = {
    .api  = &gpioShiftReg_ops,
    .priv = 0,
};
