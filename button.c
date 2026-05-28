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
//                                                     Imports
//======================================================================================================================
#include "button.h"                                                                //button header file.
#include "game.h"                                                                  //game header file.
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
		measuring();                                                               //for hit detection.
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
