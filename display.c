//======================================================================================================================
// Title:       display.c
// Author:      nathan ramos
// Created:     5/15/2026
// Description: Display Module for handling display logic.
//======================================================================================================================
//======================================================================================================================
//                                                 System Settings
//======================================================================================================================
#define F_CPU                  16000000UL
//======================================================================================================================
//                                                     Imports
//======================================================================================================================
#include "display.h"
#include "USART.h"
#include "lcd.h"
//======================================================================================================================
//                                                   Functions
//======================================================================================================================

void displayMainMenu(void)                                                          //tells song.py to show menu.
{
	USART_TransmitStr_P(PSTR("SHOW_MENU"));
}

void displayExitMessage(void)                                                       //tells song.py to end the program.
{
	USART_TransmitStr_P(PSTR("Goodbye! Thanks for playing Beat Down."));
}

void displayHighScores (void)                                                       //sends high scores to song.py.
{
	char score_buf[12];                                                             //for temp. storing each score.
	char line_buf[64];                                                              //for storing score + activator.

	
	USART_TransmitStr_P(PSTR("LEADERBOARD_START"));                                 //tells song.py to show high scores.

	for (uint8_t i = 0; i < 9; i++)                                                 //for each high score:
	{
		ltoa((long)top_scores[i].score, score_buf, 10);                             //stores one high score data to score_buf.

		snprintf(
		line_buf,
		sizeof(line_buf),
		"SCORE_ROW:%u:%s:%s",
		i + 1,
		top_scores[i].initials,
		score_buf
		);                                                                          //packs score into string with activation term "SCORE_ROW"

		USART_TransmitStr(line_buf);                                                //sends it to song.py
	}

	// Tell Python we are finished sending data
	USART_TransmitStr_P(PSTR("LEADERBOARD_END"));                                   //tels song.py all data is sent.
	
	lcd_putcmd(LCD_SET_CURSOR | SECOND_ROW);
	lcd_puts((uint8_t *)"Press any key..");                                         //shows the user to press a button to exit.
}

void displayGameOver(uint32_t final_score, uint8_t is_new_high_score)               //sends score data to song.py
{
	char line_buf[64];                                                              //the rest is the same as previous.
	char score_buf[12];
	
	ltoa((long)final_score, score_buf, 10);
	
	snprintf(
	line_buf, 
	sizeof(line_buf), 
	"GAME_OVER:%s:%u", 
	score_buf, 
	is_new_high_score
	);
	
	USART_TransmitStr(line_buf);
	
	lcd_putcmd(LCD_SET_CURSOR | SECOND_ROW);
	lcd_puts((uint8_t *)"Press any key..");
}

void displayGameScreen (uint8_t objects_matrix[8][3])                               //sends grid to song.py
{
	USART_Transmit('G');
	USART_Transmit('R');
	USART_Transmit('I');
	USART_Transmit('D');
	USART_Transmit(':');
	
	for (uint8_t r = 0; r < 8; r++)                                                 //for each row (7 of them):
	{
		uint8_t packed_row = (objects_matrix[r][0] << 2) |
		(objects_matrix[r][1] << 1) |
		(objects_matrix[r][2] );                                                    //packs row data.
		
		USART_Transmit(packed_row + '0');                                           //sends it.
		
		if (r < 7) {
			USART_Transmit(',');                                                    //with a comma if not the last.
		}
	}
	
	USART_Transmit('\r');
	USART_Transmit('\n');
}
//======================================================================================================================
//                                                 End of File
//======================================================================================================================