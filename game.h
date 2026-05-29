//======================================================================================================================
// Title:       game.h
// Author:      nathan ramos
// Created:     5/15/2026
// Description: Header file for game module.
//======================================================================================================================

#ifndef GAME_H_
#define GAME_H_

//======================================================================================================================
//                                                     Libraries
//======================================================================================================================
#include <avr/io.h>
#include <stdint.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <avr/eeprom.h>
//======================================================================================================================
//                                                     Types
//======================================================================================================================
typedef enum { RUNNING, WAITING, PAUSED } GameStage;
typedef struct {
	char     initials[4];
	uint32_t score;
} HighScore;
typedef struct {
	uint16_t delta_and_lane; // Lower 14 bits = relative delta time, Upper 2 bits = lane (0-2)
} Beat;
//======================================================================================================================
//                                                   Definitions
//======================================================================================================================
#define EEPROM_MAGIC        0xBE                           //magic byte to detect valid data
#define EEPROM_MAGIC_ADDR   0                              //address 0: magic byte
#define EEPROM_SCORES_ADDR  1                              //address 1: start of scores array
#define COMBO_THRESHOLD     20
#define BAUD                115200
#define MYUBRR              (F_CPU/8/BAUD-1)
#define MAX_BEATS           475
#define LED_DDR             DDRD |= (1<<DDD6)
#define LED_DDR_RED         DDRD |= (1<<DDD7)
#define BLUE_ON             PORTD |= (1<<PORTD6)
#define BLUE_OFF            PORTD &= ~(1<<PORTD6)
#define RED_ON              PORTD |= (1<<PORTD7)
#define RED_OFF             PORTD &= ~(1<<PORTD7)
#define SCROLL_TIME         800                             //ms for a beat to travel from top to bottom
#define TICK_INTERVAL       100                             //ms between each scroll step
//======================================================================================================================
//                                               Global Variables
//======================================================================================================================
extern volatile GameStage stage;
extern volatile uint8_t   combo;
extern volatile uint8_t   multiplier;
extern volatile uint8_t   power_up_ready;
extern volatile uint8_t   power_up_active;
extern volatile uint8_t   game_over;
extern volatile uint8_t   high_score_achieved;
extern volatile uint32_t  response_time;
extern volatile uint32_t  high_score;
extern volatile uint32_t  current_score;
extern volatile uint8_t   current_lane;
extern volatile uint32_t  beat_start_ms;
extern char               high_score_initials[4];
extern HighScore          top_scores[9];
extern uint16_t           beat_count;
extern uint8_t            game_screen[8][3];
//======================================================================================================================
//                                                   Functions
//======================================================================================================================
void setup(void);
void mainMenu(void);
void startGame(void);
void gameOver(void);
void measuring(void);
void recordBeats(void);
void transmitBeatMap(void);
void receiveBeatMap(void);
void waitForEnter(void);
void sensorDebug(void);

#endif /* GAME_H_ */
//======================================================================================================================
//                                                 End of File
//======================================================================================================================