//======================================================================================================================
// Title:       adc.c
// Author:      nathan ramos
// Created:     5/15/2026
// Description: ADC module for reading potentiometer and photoresistor.
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
#include "adc.h"
#include "game.h"
#include "USART.h"
#include "lcd.h"
//======================================================================================================================
//                                                    Functions
//======================================================================================================================
void ADC_init (void)
{
    ADMUX = (1 << REFS0);                     // Sets 5V voltage reference.
    ADCSRA |= (1 << ADEN);                    // Enables ADC.
    ADCSRA |= (1 << ADPS2)
           | (1 << ADPS1)
           | (1 << ADPS0);                    // Sets prescaler to 128 (16MHz / 128 = 125kHz).
}

uint16_t ADC_read (uint8_t channel)
{
    channel &= 0x07U;                         // Keeps channel range between 0-7.
    ADMUX = (ADMUX & 0xF0U) | channel;        // Clears old channel selection, applies new one.

    ADCSRA |= (1U << ADSC);                   // Starts the conversion.
    while (ADCSRA & (1U << ADSC)) { }         // Waits for conversion to complete.

    return ADC;                               // Returns the 10-bit value (0-1023)
}

void checkLight (void)
{
    uint16_t raw = ADC_read(PHOTO_CHANNEL);   //reads photoresistor.

    if (raw < DARK_THRESHOLD && stage == RUNNING)       //if covered during game:
    {
        stage = PAUSED;                       //pause the game.
        USART_TransmitStr_P(PSTR("PAUSE"));           //signals Python to pause song.
        RED_ON;                               //red LED -- paused state.
        BLUE_OFF;
        lcd_putcmd(LCD_SET_CURSOR | FIRST_ROW);
        lcd_puts((uint8_t *)"  -- PAUSED --  ");
        lcd_putcmd(LCD_SET_CURSOR | SECOND_ROW);
        lcd_puts((uint8_t *)"  Cover sensor  ");
    }
    else if (raw > LIGHT_THRESHOLD && stage == PAUSED)  //if uncovered while paused:
    {
        stage = RUNNING;                      //resume the game.
        USART_TransmitStr_P(PSTR("RESUME"));          //signals Python to resume song.
        RED_OFF;
        BLUE_ON;                              //blue LED -- normal state.
        lcd_putcmd(LCD_SET_CURSOR | FIRST_ROW);
        lcd_puts((uint8_t *)"  -- RUNNING -- ");
        lcd_putcmd(LCD_SET_CURSOR | SECOND_ROW);
        lcd_puts((uint8_t *)"                ");
    }
}