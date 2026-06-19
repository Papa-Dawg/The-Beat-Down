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
//                                                     Imports
//======================================================================================================================
#include "tilt.h"
#include "USART.h"
#include "lcd.h"
#include "game.h"
//======================================================================================================================
//                                                   Definitions
//======================================================================================================================
#define TILT_THRESHOLD         100                                                //minimum reading to register tilt.
#define TILT_RELEASE           50                                                 //maximum reading to register tilt over.
#define WRITE_ADDRESS          0xA6                                               //ADXL345 write address.
#define READ_ADDRESS           0xA7                                               //ADXL345 read address.
#define X_AXIS_0               0x32                                               //X axis low byte register.
#define POWER_CTL_ADDR         0x2D                                               //power control register.
#define SCL_FREQ               100000UL                                           //TWI clock frequency (100kHz).
#define TWI_PRESCALER          1                                                  //TWI prescaler value.
#define TWBR_VALUE             (((F_CPU / SCL_FREQ) - 16) / (2 * TWI_PRESCALER))  //TWBR register value.
/*------------------------------------------------------- TWI Macros ----------------------------------------------------------*/
/*------------- TWI: Two-Wire Interface. Uses a serial clock and serial channel to communicate with accelerometer. --------------
.-------------------------------------------------------------------------------------------------------------------------------.
|                                               Two-Wire Control Register (TWCR)                                                |
|-------------------------------------------------------------------------------------------------------------------------------|
| TWINT |     TWI Interrupt Flag     | Toggled to start an action, and polled to see when an action finishes.                   |
|-------|----------------------------|------------------------------------------------------------------------------------------|
| TWSTA |   TWI START Condition Bit  | Tells the hardware to generate a START condition.                                        |
|-------|----------------------------|------------------------------------------------------------------------------------------|
| TWSTO |   TWI STOP Condition Bit   | Tells the hardware to generate a STOP condition.                                         |
|-------|----------------------------|------------------------------------------------------------------------------------------|
| TWEN  |       TWI Enable Bit       | Turns on the TWI hardware circuitry on the chip.                                         |
|-------|----------------------------|------------------------------------------------------------------------------------------|
| TWEA  | TWI Enable Acknowledge Bit | Tells the hardware to pull the SDA line low to send an "ACK" bit after it receives data. |
'-------------------------------------------------------------------------------------------------------------------------------' 
*/
#define TWI_WAIT               while (!(TWCR & (1<<TWINT)))                       //loops until current TWI operation finishes.
#define TWI_START              TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN); TWI_WAIT   //starts TWI protocol.
#define TWI_STOP               TWCR = (1<<TWINT)|(1<<TWSTO)|(1<<TWEN)             //stops TWI protocol.
#define TWI_READ_ACK           TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWEA); TWI_WAIT    //reads a byte then sends ACK for more.
#define TWI_READ_NACK          TWCR = (1<<TWINT)|(1<<TWEN); TWI_WAIT              //reads final byte then sends NACK to terminate.
#define TWI_WRITE(data)        TWDR = data; TWCR = (1<<TWINT)|(1<<TWEN); TWI_WAIT //loads data register, transmits byte, waits.
//======================================================================================================================
//                                                    Functions
//======================================================================================================================
static int16_t readAccelerometer (void)
{
    uint8_t high, low;                                  //variables to hold sensor data.

    TWI_START;                                          //opens up connection.
    TWI_WRITE(WRITE_ADDRESS);                           //addresses the accelerometer (0b0101001) and sets it to write mode (0b1010010).
    TWI_WRITE(X_AXIS_0);                                //points to address of X-axis data.
    TWI_START;                                          //switches from write to read mode.
    TWI_WRITE(READ_ADDRESS);                            //addresses the accelerometer and sets it to read mode (0b1010011).
    TWI_READ_ACK;                                       //reads first 8 bytes and requests more.
    low  = TWDR;                                        //stores first 8 bytes to low variable.
    TWI_READ_NACK;                                      //reads next 8 bytes and stops reading.
    high = TWDR;                                        //stores second 8 bytes to high variable.
    TWI_STOP;                                           //closes the connection.

    return (int16_t)((high << 8) | low);                //combines low and high values into a 16 bit integer and returns it.
}

int16_t getTiltX(void)                                  //non-static version of the function.
{
	return readAccelerometer();
}

void tilt_init (void)
{
    TWBR = TWBR_VALUE;                                  //sets TWI clock speed.

    TWI_START;                                          //opens up connection.
    TWI_WRITE(WRITE_ADDRESS);                           //sets to write mode.
    TWI_WRITE(POWER_CTL_ADDR);                          //points to power control register.
    TWI_WRITE(0x08);                                    //puts accelerometer into measurement mode.
    TWI_STOP;                                           //closes the connection.
}

void checkTilt (void)
{
    int16_t x = readAccelerometer();

    if (x < 0) x = -x;                                  //absolute value without stdlib.

    static uint8_t tilt_lock = 0;
	
    if (x < TILT_RELEASE)
    {
	    tilt_lock = 0;
    }
	
    if (x > TILT_THRESHOLD && (stage == WAITING || stage == RUNNING) && power_up_ready && !tilt_lock && !power_up_active)
    {
	    tilt_lock       = 1;
        power_up_ready  = 0;                           //consumes the power up.
		power_up_active = 1;                           //sets power_up to active.

        lcd_putcmd(LCD_SET_CURSOR | SECOND_ROW);
        lcd_puts((uint8_t *)"  ** 2X ACTIVE **   ");
    }
}
//======================================================================================================================
//                                                    End of File
//======================================================================================================================