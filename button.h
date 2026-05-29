//======================================================================================================================
// Title:       button.h
// Author:      nathan ramos
// Created:     5/15/2026
// Description: The header file for my button module.
//======================================================================================================================

#ifndef BUTTON_H_
#define BUTTON_H_
//======================================================================================================================
//                                                    Libraries
//======================================================================================================================
#include <avr/io.h>
#include <stdint.h>                                                                //for uint variables.
#include <stdlib.h>                                                                //for use of NULL.
#include <avr/interrupt.h>                                                         //for interrupts.
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
//                                            Public Global Variables
//======================================================================================================================
extern volatile uint8_t left_button_pressed;
extern volatile uint8_t right_button_pressed;
extern volatile uint8_t middle_button_pressed;
//======================================================================================================================
//                                               Public Functions
//======================================================================================================================
void button_init(void);

#endif /* BUTTON_H_ */
//======================================================================================================================
//                                                 End of File
//======================================================================================================================