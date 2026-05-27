//======================================================================================================================
// Title:       main.c
// Author:      nathan ramos
// Created:     5/15/2026
// Description: Main file for Beat Tap -- a rhythm game for the ATmega328P.
//======================================================================================================================
//======================================================================================================================
//                                                 System Settings
//======================================================================================================================
#define F_CPU                  16000000UL                          //needed for delays.
//======================================================================================================================
//                                                    Libraries
//======================================================================================================================
#include <avr/io.h>
#include <stdint.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
//======================================================================================================================
//                                                     Imports
//======================================================================================================================
#include "adc.h"
#include "button.h"
#include "display.h"
#include "game.h"
#include "lcd.h"
#include "timer.h"
#include "tilt.h"
#include "USART.h"
//======================================================================================================================
//                                                  Main Function
//======================================================================================================================
int main(void)
{
    setup();                                                       //initializes all hardware and displays start message.
    mainMenu();                                                    //displays main menu and waits for input.

    while (1)
    {
        checkLight();                                              //checks photoresistor for pause/resume.
        checkTilt();                                               //checks accelerometer for power-up.

        if (left_button_pressed)
        {
            left_button_pressed = 0;

            switch (stage)
            {
                case RUNNING:
                    USART_TransmitStr("Left lane hit.");
                    measuring();
                    break;

                case PAUSED:
                    break;

                default:
                    break;
            }
        }

        if (middle_button_pressed)
        {
            middle_button_pressed = 0;

            switch (stage)
            {
                case RUNNING:
                    USART_TransmitStr("Middle lane hit.");
                    measuring();
                    break;

                case PAUSED:
                    break;

                default:
                    break;
            }
        }

        if (right_button_pressed)
        {
            right_button_pressed = 0;

            switch (stage)
            {
                case RUNNING:
                    USART_TransmitStr("Right lane hit.");
                    measuring();
                    break;

                case PAUSED:
                    break;

                default:
                    break;
            }
        }

        if (game_over)
        {
            game_over = 0;
            gameOver();                                            //handles end of game logic.
            mainMenu();                                            //returns to main menu.
        }
    }
}