//======================================================================================================================
// Title:       display.c
// Author:      nathan ramos
// Created:     5/15/2026
// Description: Optimized display module for USART and LCD output (Zero-RAM Framework).
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

/*
void displayStartMessage(void)
{
	USART_TransmitStr_P(PSTR("STATUS:START_SCREEN"));
}
*/

void displayMainMenu(void)
{
	USART_TransmitStr_P(PSTR("SHOW_MENU"));
}

void displayExitMessage(void)
{
	USART_TransmitStr_P(PSTR("Goodbye! Thanks for playing Beat Down."));
}

void displayDifficulty(uint16_t pot_value)
{
	char val_buf[48];
	
	USART_TransmitStr_P(PSTR("Current Difficulty Configuration:"));
	
	snprintf(val_buf, sizeof(val_buf), "Raw Potentiometer Value: %u", pot_value);
	USART_TransmitStr(val_buf);
	
	lcd_putcmd(LCD_SET_CURSOR | SECOND_ROW);
	lcd_puts((uint8_t *)"Press any key..");
}

void displayHighScores (void)
{
	char score_buf[12];
	char line_buf[64];

	// Tell Python we are starting the leaderboard dump
	USART_TransmitStr_P(PSTR("LEADERBOARD_START"));

	for (uint8_t i = 0; i < 9; i++)
	{
		ltoa((long)top_scores[i].score, score_buf, 10);

		// Pack row number, initials, and score into a tight token
		snprintf(
		line_buf,
		sizeof(line_buf),
		"SCORE_ROW:%u:%s:%s",
		i + 1,
		top_scores[i].initials,
		score_buf
		);

		USART_TransmitStr(line_buf);
	}

	// Tell Python we are finished sending data
	USART_TransmitStr_P(PSTR("LEADERBOARD_END"));
	
	lcd_putcmd(LCD_SET_CURSOR | SECOND_ROW);
	lcd_puts((uint8_t *)"Press any key..");
}

void displayGameOver(uint32_t final_score, uint8_t is_new_high_score)
{
	char line_buf[64];
	char score_buf[12];
	
	ltoa((long)final_score, score_buf, 10);
	
	snprintf(line_buf, sizeof(line_buf), "GAME_OVER:%s:%u", score_buf, is_new_high_score);
	
	USART_TransmitStr(line_buf);
	
	lcd_putcmd(LCD_SET_CURSOR | SECOND_ROW);
	lcd_puts((uint8_t *)"Press any key..");
}

void displayGameScreen (uint8_t objects_matrix[8][3])
{
	USART_Transmit('G');
	USART_Transmit('R');
	USART_Transmit('I');
	USART_Transmit('D');
	USART_Transmit(':');
	
	for (uint8_t r = 0; r < 8; r++)
	{
		uint8_t packed_row = (objects_matrix[r][0] << 2) |
		(objects_matrix[r][1] << 1) |
		(objects_matrix[r][2] );
		
		USART_Transmit(packed_row + '0');
		
		if (r < 7) {
			USART_Transmit(',');
		}
	}
	
	USART_Transmit('\r');
	USART_Transmit('\n');
}