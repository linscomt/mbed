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
#include "i2c_api.h"

#if DEVICE_I2C

#include "cmsis.h"
#include "pinmap.h"
#include "error.h"

#if defined(TARGET_LPC1768) || defined(TARGET_LPC2368)
static const PinMap PinMap_I2C_SDA[] = {
    {P0_0 , I2C_1, 3},
    {P0_10, I2C_2, 2},
    {P0_19, I2C_1, 3},
    {P0_27, I2C_0, 1},
    {NC   , NC   , 0}
};

static const PinMap PinMap_I2C_SCL[] = {
    {P0_1 , I2C_1, 3},
    {P0_11, I2C_2, 2},
    {P0_20, I2C_1, 3},
    {P0_28, I2C_0, 1},
    {NC   , NC,    0}
};

#define I2C_CONSET(x)       (x->i2c->I2CONSET)
#define I2C_CONCLR(x)       (x->i2c->I2CONCLR)
#define I2C_STAT(x)         (x->i2c->I2STAT)
#define I2C_DAT(x)          (x->i2c->I2DAT)
#define I2C_SCLL(x, val)    (x->i2c->I2SCLL = val)
#define I2C_SCLH(x, val)    (x->i2c->I2SCLH = val)

#elif defined(TARGET_LPC11U24)
static const PinMap PinMap_I2C_SDA[] = {
    {P0_5, I2C_0, 1},
    {NC  , NC   , 0}
};

static const PinMap PinMap_I2C_SCL[] = {
    {P0_4, I2C_0, 1},
    {NC  , NC,    0}
};

#define I2C_CONSET(x)       (x->i2c->CONSET)
#define I2C_CONCLR(x)       (x->i2c->CONCLR)
#define I2C_STAT(x)         (x->i2c->STAT)
#define I2C_DAT(x)          (x->i2c->DAT)
#define I2C_SCLL(x, val)    (x->i2c->SCLL = val)
#define I2C_SCLH(x, val)    (x->i2c->SCLH = val)

#elif defined(TARGET_LPC812)
static const SWM_Map SWM_I2C_SDA[] = {
    {7, 24},
};

static const SWM_Map SWM_I2C_SCL[] = {
    {8, 0},
};

static uint8_t repeated_start = 0;

#define I2C_DAT(x)          (x->i2c->MSTDAT)
#define I2C_STAT(x)         ((x->i2c->STAT >> 1) & (0x07))

#elif defined(TARGET_LPC4088)
static const PinMap PinMap_I2C_SDA[] = {
    {P0_0 , I2C_1, 3},
    {P0_10, I2C_2, 2},
    {P0_19, I2C_1, 3},
    {P0_27, I2C_0, 1},
    {P1_15, I2C_2, 3},
    {P1_30, I2C_0, 4},
    {P2_14, I2C_1, 2},
    {P2_30, I2C_2, 2},
    {P4_20, I2C_2, 4},
    {P5_2,  I2C_0, 5},
    {NC   , NC   , 0}
};

static const PinMap PinMap_I2C_SCL[] = {
    {P0_1 , I2C_1, 3},
    {P0_11, I2C_2, 2},
    {P0_20, I2C_1, 3},
    {P0_28, I2C_0, 1},
    {P1_31, I2C_0, 4},
    {P2_15, I2C_1, 2},
    {P2_31, I2C_2, 2},
    {P4_21, I2C_2, 2},
    {P4_29, I2C_2, 4},
    {P5_3,  I2C_0, 5},
    {NC   , NC,    0}
};

#define I2C_CONSET(x)       (x->i2c->CONSET)
#define I2C_CONCLR(x)       (x->i2c->CONCLR)
#define I2C_STAT(x)         (x->i2c->STAT)
#define I2C_DAT(x)          (x->i2c->DAT)
#define I2C_SCLL(x, val)    (x->i2c->SCLL = val)
#define I2C_SCLH(x, val)    (x->i2c->SCLH = val)

#endif


#if defined(TARGET_LPC1768) || defined(TARGET_LPC11U24) || defined(TARGET_LPC2368) || defined(TARGET_LPC4088)
static const uint32_t I2C_addr_offset[2][4] = {
    {0x0C, 0x20, 0x24, 0x28},
    {0x30, 0x34, 0x38, 0x3C}
};

static inline void i2c_conclr(i2c_t *obj, int start, int stop, int interrupt, int acknowledge) {
    I2C_CONCLR(obj) = (start << 5)
                    | (stop << 4)
                    | (interrupt << 3)
                    | (acknowledge << 2);
}

static inline void i2c_conset(i2c_t *obj, int start, int stop, int interrupt, int acknowledge) {
    I2C_CONSET(obj) = (start << 5)
                    | (stop << 4)
                    | (interrupt << 3)
                    | (acknowledge << 2);
}

// Clear the Serial Interrupt (SI)
static inline void i2c_clear_SI(i2c_t *obj) {
    i2c_conclr(obj, 0, 0, 1, 0);
}
#endif


static inline int i2c_status(i2c_t *obj) {
    return I2C_STAT(obj);
}

// Wait until the Serial Interrupt (SI) is set
static int i2c_wait_SI(i2c_t *obj) {
    int timeout = 0;
#if defined(TARGET_LPC1768) || defined(TARGET_LPC11U24) || defined(TARGET_LPC2368) || defined(TARGET_LPC4088)
    while (!(I2C_CONSET(obj) & (1 << 3))) {
#elif defined(TARGET_LPC812)
    while (!(obj->i2c->STAT & (1 << 0))) {
#endif
        timeout++;
        if (timeout > 100000) return -1;
    }
    return 0;
}

static inline void i2c_interface_enable(i2c_t *obj) {
#if defined(TARGET_LPC1768) || defined(TARGET_LPC11U24) || defined(TARGET_LPC2368) || defined(TARGET_LPC4088)
    I2C_CONSET(obj) = 0x40;
#elif defined(TARGET_LPC812)
    obj->i2c->CFG |= (1 << 0);
#endif
}

static inline void i2c_power_enable(i2c_t *obj) {
#if defined(TARGET_LPC1768) || defined(TARGET_LPC2368) || defined(TARGET_LPC4088)
    switch ((int)obj->i2c) {
        case I2C_0: LPC_SC->PCONP |= 1 << 7; break;
        case I2C_1: LPC_SC->PCONP |= 1 << 19; break;
        case I2C_2: LPC_SC->PCONP |= 1 << 26; break;
    }
#elif defined(TARGET_LPC11U24)
    LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 5);
    LPC_SYSCON->PRESETCTRL |= 1 << 1;
#elif defined(TARGET_LPC812)
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<5);	
    LPC_SYSCON->PRESETCTRL &= ~(0x1<<6);
    LPC_SYSCON->PRESETCTRL |= (0x1<<6);
#endif
}

void i2c_init(i2c_t *obj, PinName sda, PinName scl) {
#if defined(TARGET_LPC1768) || defined(TARGET_LPC2368) || defined(TARGET_LPC11U24) || defined(TARGET_LPC4088)
    // determine the SPI to use
    I2CName i2c_sda = (I2CName)pinmap_peripheral(sda, PinMap_I2C_SDA);
    I2CName i2c_scl = (I2CName)pinmap_peripheral(scl, PinMap_I2C_SCL);
#if defined(TARGET_LPC1768) || defined(TARGET_LPC2368) || defined(TARGET_LPC4088)
    obj->i2c = (LPC_I2C_TypeDef *)pinmap_merge(i2c_sda, i2c_scl);
#elif defined(TARGET_LPC11U24)
    obj->i2c = (LPC_I2C_Type *)pinmap_merge(i2c_sda, i2c_scl);
#endif
    
    if ((int)obj->i2c == NC) {
        error("I2C pin mapping failed");
    }

    // enable power
    i2c_power_enable(obj);

    // set default frequency at 100k
    i2c_frequency(obj, 100000);
    i2c_conclr(obj, 1, 1, 1, 1);
    i2c_interface_enable(obj);

    pinmap_pinout(sda, PinMap_I2C_SDA);
    pinmap_pinout(scl, PinMap_I2C_SCL);
    
#elif defined (TARGET_LPC812)
    obj->i2c = (LPC_I2C_TypeDef *)LPC_I2C;
    
    const SWM_Map *swm;
    uint32_t regVal;
    
    swm = &SWM_I2C_SDA[0];
    regVal = LPC_SWM->PINASSIGN[swm->n] & ~(0xFF << swm->offset);
    LPC_SWM->PINASSIGN[swm->n] = regVal |  (sda   << swm->offset);
    
    swm = &SWM_I2C_SCL[0];
    regVal = LPC_SWM->PINASSIGN[swm->n] & ~(0xFF << swm->offset);
    LPC_SWM->PINASSIGN[swm->n] = regVal |  (scl   << swm->offset);
    
    // enable power
    i2c_power_enable(obj);
    // set default frequency at 100k
    i2c_frequency(obj, 100000);
    i2c_interface_enable(obj);
#endif

}

inline int i2c_start(i2c_t *obj) {
    int status = 0;
#if defined(TARGET_LPC1768) || defined(TARGET_LPC2368) || defined(TARGET_LPC11U24) || defined(TARGET_LPC4088)
    // 8.1 Before master mode can be entered, I2CON must be initialised to:
    //  - I2EN STA STO SI AA - -
    //  -  1    0   0   0  x - -
    // if AA = 0, it can't enter slave mode
    i2c_conclr(obj, 1, 1, 1, 1);

    // The master mode may now be entered by setting the STA bit
    // this will generate a start condition when the bus becomes free
    i2c_conset(obj, 1, 0, 0, 1);

    i2c_wait_SI(obj);
    status = i2c_status(obj);

    // Clear start bit now transmitted, and interrupt bit
    i2c_conclr(obj, 1, 0, 0, 0);
#elif defined(TARGET_LPC812)
    if (repeated_start) {
        obj->i2c->MSTCTL = (1 << 1) | (1 << 0);
        repeated_start = 0;
    } else {
        obj->i2c->MSTCTL = (1 << 1);
    }
#endif
    return status;
}

inline void i2c_stop(i2c_t *obj) {
#if defined(TARGET_LPC1768) || defined(TARGET_LPC2368) || defined(TARGET_LPC11U24) || defined(TARGET_LPC4088)
    // write the stop bit
    i2c_conset(obj, 0, 1, 0, 0);
    i2c_clear_SI(obj);

    // wait for STO bit to reset
    while(I2C_CONSET(obj) & (1 << 4));
#elif defined(TARGET_LPC812)
    obj->i2c->MSTCTL = (1 << 2) | (1 << 0);
    while ((obj->i2c->STAT & ((1 << 0) | (7 << 1))) != ((1 << 0) | (0 << 1)));
#endif
}


static inline int i2c_do_write(i2c_t *obj, int value, uint8_t addr) {
    // write the data
    I2C_DAT(obj) = value;
    
#if defined(TARGET_LPC1768) || defined(TARGET_LPC2368) || defined(TARGET_LPC11U24) || defined(TARGET_LPC4088)
    // clear SI to init a send
    i2c_clear_SI(obj);
#elif defined(TARGET_LPC812)
    if (!addr)
        obj->i2c->MSTCTL = (1 << 0);
#endif

    // wait and return status
    i2c_wait_SI(obj);
    return i2c_status(obj);
}

static inline int i2c_do_read(i2c_t *obj, int last) {
#if defined(TARGET_LPC1768) || defined(TARGET_LPC2368) || defined(TARGET_LPC11U24) || defined(TARGET_LPC4088)
    // we are in state 0x40 (SLA+R tx'd) or 0x50 (data rx'd and ack)
    if(last) {
        i2c_conclr(obj, 0, 0, 0, 1); // send a NOT ACK
    } else {
        i2c_conset(obj, 0, 0, 0, 1); // send a ACK
    }

    // accept byte
    i2c_clear_SI(obj);
#endif

    // wait for it to arrive
    i2c_wait_SI(obj);

#if defined(TARGET_LPC812)
    if (!last)
        obj->i2c->MSTCTL = (1 << 0);
#endif

    // return the data
    return (I2C_DAT(obj) & 0xFF);
}

void i2c_frequency(i2c_t *obj, int hz) {
#if defined(TARGET_LPC1768) || defined(TARGET_LPC2368)
    // [TODO] set pclk to /4
    uint32_t PCLK = SystemCoreClock / 4;
#elif defined(TARGET_LPC11U24) || defined(TARGET_LPC812)
    // No peripheral clock divider on the M0
    uint32_t PCLK = SystemCoreClock;

#elif defined(TARGET_LPC4088)
    uint32_t PCLK = PeripheralClock;
#endif
    
#if defined(TARGET_LPC1768) || defined(TARGET_LPC2368) || defined(TARGET_LPC11U24) || defined(TARGET_LPC4088)
    uint32_t pulse = PCLK / (hz * 2);

    // I2C Rate
    I2C_SCLL(obj, pulse);
    I2C_SCLH(obj, pulse);
#elif defined(TARGET_LPC812)
    uint32_t clkdiv = PCLK / (hz * 4) - 1;
    
    obj->i2c->DIV = clkdiv;
    obj->i2c->MSTTIME = 0;
#endif
}

// The I2C does a read or a write as a whole operation
// There are two types of error conditions it can encounter
//  1) it can not obtain the bus
//  2) it gets error responses at part of the transmission
//
// We tackle them as follows:
//  1) we retry until we get the bus. we could have a "timeout" if we can not get it
//      which basically turns it in to a 2)
//  2) on error, we use the standard error mechanisms to report/debug
//
// Therefore an I2C transaction should always complete. If it doesn't it is usually
// because something is setup wrong (e.g. wiring), and we don't need to programatically
// check for that

int i2c_read(i2c_t *obj, int address, char *data, int length, int stop) {
    int count, status;

    status = i2c_start(obj);

#if defined(TARGET_LPC1768) || defined(TARGET_LPC11U24) || defined(TARGET_LPC2368) || defined(TARGET_LPC4088)
    if ((status != 0x10) && (status != 0x08)) {
        i2c_stop(obj);
        return status;
    }
#endif

    status = i2c_do_write(obj, (address | 0x01), 1);
#if defined(TARGET_LPC1768) || defined(TARGET_LPC11U24) || defined(TARGET_LPC2368) || defined(TARGET_LPC4088)
    if (status != 0x40) {
#elif defined(TARGET_LPC812)
    if (status != 0x01) {
#endif
        i2c_stop(obj);
        return status;
    }

    // Read in all except last byte
    for (count = 0; count < (length - 1); count++) {
        int value = i2c_do_read(obj, 0);
        status = i2c_status(obj);
#if defined(TARGET_LPC1768) || defined(TARGET_LPC11U24) || defined(TARGET_LPC2368) || defined(TARGET_LPC4088)
        if (status != 0x50) {
#elif defined(TARGET_LPC812)
        if (status != 0x00) {
#endif
            i2c_stop(obj);
            return status;
        }
        data[count] = (char) value;
    }

    // read in last byte
    int value = i2c_do_read(obj, 1);
    status = i2c_status(obj);
#if defined(TARGET_LPC1768) || defined(TARGET_LPC11U24) || defined(TARGET_LPC2368) || defined(TARGET_LPC4088)
    if (status != 0x58) {
#elif defined(TARGET_LPC812)
    if (status != 0x01) {
#endif
        i2c_stop(obj);
        return status;
    }

    data[count] = (char) value;

    // If not repeated start, send stop.
    if (stop) {
        i2c_stop(obj);
    }
#if defined(TARGET_LPC812)
    else {
        repeated_start = 1;
    }
#endif

    return 0;
}

int i2c_write(i2c_t *obj, int address, const char *data, int length, int stop) {
    int i, status;

    status = i2c_start(obj);

#if defined(TARGET_LPC1768) || defined(TARGET_LPC11U24) || defined(TARGET_LPC2368) || defined(TARGET_LPC4088)
    if ((status != 0x10) && (status != 0x08)) {
        i2c_stop(obj);
        return status;
    }
#endif

    status = i2c_do_write(obj, (address & 0xFE), 1);
#if defined(TARGET_LPC1768) || defined(TARGET_LPC11U24) || defined(TARGET_LPC2368) || defined(TARGET_LPC4088)
    if (status != 0x18) {
#elif defined(TARGET_LPC812)
    if (status != 0x02) {
#endif
        i2c_stop(obj);
        return status;
    }

    for (i=0; i<length; i++) {
        status = i2c_do_write(obj, data[i], 0);
#if defined(TARGET_LPC1768) || defined(TARGET_LPC11U24) || defined(TARGET_LPC2368) || defined(TARGET_LPC4088)
        if(status != 0x28) {
#elif defined(TARGET_LPC812)
        if (status != 0x02) {
#endif
            i2c_stop(obj);
            return status;
        }
    }

#if defined(TARGET_LPC1768) || defined(TARGET_LPC11U24) || defined(TARGET_LPC2368) || defined(TARGET_LPC4088)
    i2c_clear_SI(obj);
#endif

    // If not repeated start, send stop.
    if (stop) {
        i2c_stop(obj);
    }
#if defined(TARGET_LPC812)
    else {
        repeated_start = 1;
    }
#endif

    return 0;
}

void i2c_reset(i2c_t *obj) {
    i2c_stop(obj);
}

int i2c_byte_read(i2c_t *obj, int last) {
    return (i2c_do_read(obj, last) & 0xFF);
}

int i2c_byte_write(i2c_t *obj, int data) {
    int ack;
    int status = i2c_do_write(obj, (data & 0xFF), 0);

    switch(status) {
#if defined(TARGET_LPC1768) || defined(TARGET_LPC11U24) || defined(TARGET_LPC2368) || defined(TARGET_LPC4088)
        case 0x18: case 0x28:       // Master transmit ACKs
            ack = 1;
            break;
        case 0x40:                  // Master receive address transmitted ACK
            ack = 1;
            break;
        case 0xB8:                  // Slave transmit ACK
            ack = 1;
            break;
#elif defined(TARGET_LPC812)
        case 2:
            ack = 1;
            break;
#endif
        default:
            ack = 0;
            break;
    }

    return ack;
}

#if DEVICE_I2CSLAVE
void i2c_slave_mode(i2c_t *obj, int enable_slave) {
    if (enable_slave != 0) {
        i2c_conclr(obj, 1, 1, 1, 0);
        i2c_conset(obj, 0, 0, 0, 1);
    } else {
        i2c_conclr(obj, 1, 1, 1, 1);
    }
}

int i2c_slave_receive(i2c_t *obj) {
    int status;
    int retval;

    status = i2c_status(obj);
    switch(status) {
        case 0x60: retval = 3; break;
        case 0x70: retval = 2; break;
        case 0xA8: retval = 1; break;
        default  : retval = 0; break;
    }

    return(retval);
}

int i2c_slave_read(i2c_t *obj, char *data, int length) {
    int count = 0;
    int status;

    do {
        i2c_clear_SI(obj);
        i2c_wait_SI(obj);
        status = i2c_status(obj);
        if((status == 0x80) || (status == 0x90)) {
            data[count] = I2C_DAT(obj) & 0xFF;
        }
        count++;
    } while (((status == 0x80) || (status == 0x90) ||
            (status == 0x060) || (status == 0x70)) && (count < length));

    if(status != 0xA0) {
        i2c_stop(obj);
    }

    i2c_clear_SI(obj);

    return (count - 1);
}

int i2c_slave_write(i2c_t *obj, const char *data, int length) {
    int count = 0;
    int status;

    if(length <= 0) {
        return(0);
    }

    do {
        status = i2c_do_write(obj, data[count], 0);
        count++;
    } while ((count < length) && (status == 0xB8));

    if((status != 0xC0) && (status != 0xC8)) {
        i2c_stop(obj);
    }

    i2c_clear_SI(obj);

    return(count);
}

void i2c_slave_address(i2c_t *obj, int idx, uint32_t address, uint32_t mask) {
    uint32_t addr;

    if ((idx >= 0) && (idx <= 3)) {
        addr = ((uint32_t)obj->i2c) + I2C_addr_offset[0][idx];
        *((uint32_t *) addr) = address & 0xFF;
#if defined(TARGET_LPC1768) || defined(TARGET_LPC4088)
        addr = ((uint32_t)obj->i2c) + I2C_addr_offset[1][idx];
        *((uint32_t *) addr) = mask & 0xFE;
#endif
    }
}
#endif

#endif
