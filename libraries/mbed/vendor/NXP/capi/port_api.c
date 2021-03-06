/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "port_api.h"
#include "pinmap.h"
#include "gpio_api.h"

#if DEVICE_PORTIN || DEVICE_PORTOUT

PinName port_pin(PortName port, int pin_n) {
#if defined(TARGET_LPC1768) || defined(TARGET_LPC2368) || defined(TARGET_LPC4088)
    return (PinName)(LPC_GPIO0_BASE + ((port << PORT_SHIFT) | pin_n));
#elif defined(TARGET_LPC11U24)
    return (PinName)((port << PORT_SHIFT) | pin_n);
#endif
}

void port_init(port_t *obj, PortName port, int mask, PinDirection dir) {
    obj->port = port;
    obj->mask = mask;

#if defined(TARGET_LPC1768) || defined(TARGET_LPC2368)
    LPC_GPIO_TypeDef *port_reg = (LPC_GPIO_TypeDef *)(LPC_GPIO0_BASE + ((int)port * 0x20));

    // Do not use masking, because it prevents the use of the unmasked pins
    // port_reg->FIOMASK = ~mask;

    obj->reg_out = &port_reg->FIOPIN;
    obj->reg_in  = &port_reg->FIOPIN;
    obj->reg_dir  = &port_reg->FIODIR;

#elif defined(TARGET_LPC11U24)
    LPC_GPIO->MASK[port] = ~mask;

    obj->reg_mpin = &LPC_GPIO->MPIN[port];
    obj->reg_dir = &LPC_GPIO->DIR[port];
#elif defined(TARGET_LPC4088)
    LPC_GPIO_TypeDef *port_reg = (LPC_GPIO_TypeDef *)(LPC_GPIO0_BASE + ((int)port * 0x20));

    port_reg->MASK = ~mask;

    obj->reg_out = &port_reg->PIN;
    obj->reg_in  = &port_reg->PIN;
    obj->reg_dir  = &port_reg->DIR;

#endif
    uint32_t i;
    // The function is set per pin: reuse gpio logic
    for (i=0; i<32; i++) {
        if (obj->mask & (1<<i)) {
            gpio_set(port_pin(obj->port, i));
        }
    }

    port_dir(obj, dir);
}

void port_mode(port_t *obj, PinMode mode) {
    uint32_t i;
    // The mode is set per pin: reuse pinmap logic
    for (i=0; i<32; i++) {
        if (obj->mask & (1<<i)) {
            pin_mode(port_pin(obj->port, i), mode);
        }
    }
}

void port_dir(port_t *obj, PinDirection dir) {
    switch (dir) {
        case PIN_INPUT : *obj->reg_dir &= ~obj->mask; break;
        case PIN_OUTPUT: *obj->reg_dir |=  obj->mask; break;
    }
}

void port_write(port_t *obj, int value) {
#if defined(TARGET_LPC11U24)
    *obj->reg_mpin = value;
#elif defined(TARGET_LPC1768) || defined(TARGET_LPC2368) || defined(TARGET_LPC4088)
    *obj->reg_out = (*obj->reg_in & ~obj->mask) | (value & obj->mask);
#endif
}

int port_read(port_t *obj) {
#if defined(TARGET_LPC11U24)
    return (*obj->reg_mpin);
#elif defined(TARGET_LPC1768) || defined(TARGET_LPC2368) || defined(TARGET_LPC4088)
    return (*obj->reg_in & obj->mask);
#endif
}

#endif
