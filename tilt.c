//======================================================================================================================
// Title:       tilt.c
// Author:      nathan ramos
// Created:     5/15/2026
// Description: Accelerometer module for tilt-based power-up activation.
//======================================================================================================================
//======================================================================================================================
//                                                 System Settings
//======================================================================================================================
#define F_CPU                  16000000UL
//======================================================================================================================
//                                                    Libraries
//======================================================================================================================
#include <avr/io.h>
#include <stdint.h>
//======================================================================================================================
//                                                     Imports
//======================================================================================================================
#include "tilt.h"
#include "USART.h"
#include "lcd.h"
//======================================================================================================================
//                                                   Definitions
//======================================================================================================================
#define TILT_THRESHOLD         200                     //minimum acceleration to register tilt.
#define WRITE_ADDRESS          0xA6                    //ADXL345 write address.
#define READ_ADDRESS           0xA7                    //ADXL345 read address.
#define X_AXIS_0               0x32                    //X axis low byte register.
#define POWER_CTL_ADDR         0x2D                    //power control register.
#define SCL_FREQ               100000UL                //I2C clock frequency (100kHz).
#define TWI_PRESCALER          1                       //TWI prescaler value.
#define TWBR_VALUE             (((F_CPU / SCL_FREQ) - 16) / (2 * TWI_PRESCALER))  //TWBR register value.
/*-------------------------------------------- TWI Macros ----------------------------------------------------*/
#define TWI_WAIT               while (!(TWCR & (1<<TWINT)))
#define TWI_START              TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN); TWI_WAIT
#define TWI_STOP               TWCR = (1<<TWINT)|(1<<TWSTO)|(1<<TWEN)
#define TWI_READ_ACK           TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWEA); TWI_WAIT
#define TWI_READ_NACK          TWCR = (1<<TWINT)|(1<<TWEN); TWI_WAIT
#define TWI_WRITE(data)        TWDR = data; TWCR = (1<<TWINT)|(1<<TWEN); TWI_WAIT
//======================================================================================================================
//                                                    Functions
//======================================================================================================================
static int16_t readAccelerometer (void)
{
    uint8_t high, low;

    TWI_START;
    TWI_WRITE(WRITE_ADDRESS);
    TWI_WRITE(X_AXIS_0);
    TWI_START;
    TWI_WRITE(READ_ADDRESS);
    TWI_READ_ACK;
    low  = TWDR;
    TWI_READ_NACK;
    high = TWDR;
    TWI_STOP;

    return (int16_t)((high << 8) | low);
}

void tilt_init (void)
{
    TWBR = TWBR_VALUE;                        //sets I2C clock speed.

    TWI_START;
    TWI_WRITE(WRITE_ADDRESS);
    TWI_WRITE(POWER_CTL_ADDR);
    TWI_WRITE(0x08);                          //puts ADXL345 into measurement mode.
    TWI_STOP;
}

void checkTilt (void)
{
    int16_t x = readAccelerometer();

    if (x < 0) x = -x;                       //absolute value without stdlib.

    if (x > TILT_THRESHOLD && stage == RUNNING && power_up_ready)
    {
        multiplier     = 2;                   //activates 2x score multiplier.
        power_up_ready = 0;                   //consumes the power up.

        USART_TransmitStr("*** 2X MULTIPLIER ACTIVATED ***");

        lcd_putcmd(LCD_SET_CURSOR | SECOND_ROW);
        lcd_puts((uint8_t *)"  ** 2X ACTIVE **   ");
    }
}
//======================================================================================================================
//                                                    End of File
//======================================================================================================================