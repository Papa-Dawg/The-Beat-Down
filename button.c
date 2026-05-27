//======================================================================================================================
// Title:       button.c
// Author:      nathan ramos
// Created:     5/15/2026
// Description: My button module, for handling the button pressing logic.
//======================================================================================================================
//======================================================================================================================
//                                                 System Settings
//======================================================================================================================
#define F_CPU                              16000000UL                              //needed for delays.
//======================================================================================================================
//                                                    Libraries
//======================================================================================================================
#include <avr/io.h>
#include <stdint.h>                                                                //for uint variables.
#include <stdlib.h>                                                                //for use of NULL.
#include <avr/interrupt.h>                                                         //for interrupts.
//======================================================================================================================
//                                                     Imports
//======================================================================================================================
#include "button.h"                                                                //button header file
#include "game.h"
//======================================================================================================================
//                                                   Definitions
//======================================================================================================================
/*------------------------------------------------ Button Macros -----------------------------------------------------*/
#define LEFT_BUTTON_PIN                    PIND2                                   //left button pin.
#define RIGHT_BUTTON_PIN                   PIND3                                   //right button pin.
#define MIDDLE_BUTTON_PIN                  PIND4                                   //middle button pin.
#define STATE_HIGH(pin)                    (PIND & (1<<pin))                       //checking input is HIGH.                              
#define STATE_LOW(pin)                     (!(PIND & (1<<pin)))                    //checking input is LOW.                            
#define BUTTON_PRESSED(pin, lock_state)    (STATE_LOW(pin) && (lock_state == 0))   //button is pressed.
#define BUTTON_RELEASED(pin, lock_state)   (STATE_HIGH(pin) && (lock_state == 2))  //button is released.
#define ENABLE_INT                         EIMSK |= (1<<INT0)|(1<<INT1)            //INT enabled.
#define ENABLE_PCINT                       PCMSK2 |= (1<<PCINT20); \
                                           PCICR  |= (1<<PCIE2)                    //enables pin change interrupt.
#define DISABLE_INT                        EIMSK &= ~((1<<INT0)|(1<<INT1))         //INT disabled.
#define DISABLE_PCINT                      PCMSK2 &= ~(1<<PCINT20)                 //disables pin change interrupt.
#define INT_ON_CHANGE                      EICRA |= (1<<ISC00)|(1<<ISC10)          //INT triggered on change.
/*------------------------------------------------ Timer2 Macros -----------------------------------------------------*/
#define DELAY                              0.015                                   //second(s)
#define PRESCALER                          1024                                    //prescaler
#define TIMER                              ((F_CPU / PRESCALER * DELAY) - 1)       //function for OCR2A value.
#define ENABLE_CTC                         TCCR2A |= (1<<WGM21)                    //enables CTC mode.
#define ENABLE_TIMER2                      TIMSK2 |= (1<<OCIE2A)                   //enables timer 2 interrupts.
#define DISABLE_TIMER2                     TIMSK2 &= ~(1<<OCIE2A)                  //disables timer 2 interrupts.
#define START_TIMER2                       TCCR2B |= (1<<CS22)|(1<<CS21)|(1<<CS20) //prescaling--starting timer 2.
#define STOP_TIMER2                        TCCR2B &= ~((1<<CS22)|(1<<CS21)|(1<<CS20)) //stops timer 2.
#define RESET_TIMER2                       TCNT2 = 0                               //resets timer 2.
/*--------------------------------------------------- Constants ------------------------------------------------------*/
#define UNLOCKED_FOR_PRESS                 0                                       //can check for button press.
#define PRESS_LOCKED                       1                                       //INT locked after button press.
#define UNLOCKED_FOR_RELEASE               2                                       //can now check for button release.
#define RELEASE_LOCKED                     3                                       //INT locked after button release.
//======================================================================================================================
//                                                 Global Variables
//======================================================================================================================
static volatile uint8_t left_lock = UNLOCKED_FOR_PRESS;                            //for tracking left button state.
static volatile uint8_t right_lock = UNLOCKED_FOR_PRESS;                           //for tracking right button state.
static volatile uint8_t middle_lock = UNLOCKED_FOR_PRESS;                          //for tracking middle button state.
volatile uint8_t left_button_pressed = 0;                                          //for use in game logic.
volatile uint8_t right_button_pressed = 0;                                         //ditto.
volatile uint8_t middle_button_pressed = 0;                                        //ditto.
static volatile uint8_t * volatile active_lock = NULL;                             //pointer variable for active lock.
//======================================================================================================================
//                                                    Functions
//======================================================================================================================
void button_init (void)
{
	DDRD &= ~((1<<LEFT_BUTTON_PIN)|(1<<MIDDLE_BUTTON_PIN)|(1<<RIGHT_BUTTON_PIN));  //sets to input (for buttons).
	PORTD |= (1<<LEFT_BUTTON_PIN)|(1<<MIDDLE_BUTTON_PIN)|(1<<RIGHT_BUTTON_PIN);    //sets input to HIGH (for pull-up)
	INT_ON_CHANGE;                                                                 //INT0 and INT1 triggered on change.
	ENABLE_INT;                                                                    //INT0 and INT1 enabled.
	ENABLE_PCINT;                                                                  //PCINT enabled (for middle button.)
    ENABLE_CTC;                                                                    //enables CTC mode.
	OCR2A = TIMER;                                                                 //for delaying interrupts.
}

static void debounceStart (volatile uint8_t *current_lock)
{
    active_lock = current_lock;
	DISABLE_INT;                                                                   //INT0 and INT1 disabled.
	DISABLE_PCINT;                                                                 //PCINT disabled.
	RESET_TIMER2;                                                                  //timer reset.
	START_TIMER2;                                                                  //prescaler (starts timer).
	ENABLE_TIMER2;                                                                 //enables timer2 interrupts.
}

static void debounceOver (volatile uint8_t *current_lock)
{
    STOP_TIMER2;                                                                   //prescaling removed (stops timer)
	DISABLE_TIMER2;                                                                //disables timer2 interrupts.

	if (*current_lock == PRESS_LOCKED)                                             //if button press delay finished:
	{
		*current_lock = UNLOCKED_FOR_RELEASE;                                      //set-up for button release.
	}
	else if (*current_lock == RELEASE_LOCKED)                                      //if button release delay finished:
	{
		*current_lock = UNLOCKED_FOR_PRESS;                                        //button pressing unlocked.
	}

    ENABLE_INT;                                                                    //INT0/1 re-enabled.
	ENABLE_PCINT;                                                                  //PCINT re-enabled.
}

static void debounce (uint8_t pin, volatile uint8_t *current_lock, volatile uint8_t *flag)
{
    if (BUTTON_PRESSED(pin, *current_lock))                                        //if button pressed:
	{
		debounceStart(current_lock);                                               //debounce delay.
		*current_lock = PRESS_LOCKED;                                              //INT0/1 locked after press.
		
		*flag = 1;                                                                 //flags that button was pressed.
			
	}
	else if (BUTTON_RELEASED(pin, *current_lock))                                  //if button released:
	{
		debounceStart(current_lock);                                               //debounce delay
		*current_lock = RELEASE_LOCKED;                                            //INT0/1 locked after release.
	}
}
//======================================================================================================================
//                                             Interrupt Service Routines
//======================================================================================================================
ISR (TIMER2_COMPA_vect)
{
    if (active_lock != NULL)
    {
        debounceOver(active_lock);                                                 //ends debounce for current button.
        active_lock = NULL;
    }
}

ISR (INT0_vect)
{
	debounce(LEFT_BUTTON_PIN, &left_lock, &left_button_pressed);                   //handles debounce for left button.
	
	if (stage == WAITING)
	{
		measuring();
	}
}

ISR (INT1_vect)
{
	debounce(RIGHT_BUTTON_PIN, &right_lock, &right_button_pressed);                //handles debounce for right button.
	
	if (stage == WAITING)
	{
		measuring();
	}
}

ISR (PCINT2_vect)
{
    debounce(MIDDLE_BUTTON_PIN, &middle_lock, &middle_button_pressed);             //handles debounce for middle button.
	
	if (stage == WAITING)
	{
		measuring();
	}
}
//======================================================================================================================
//                                                    End of File
//======================================================================================================================
