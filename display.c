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

void displayStartMessage(void)
{
	USART_TransmitStr_P(PSTR("========================================"));
	USART_TransmitStr_P(PSTR("         Welcome to Beat Down!          "));
	USART_TransmitStr_P(PSTR("========================================"));
}

void displayMainMenu(void)
{
	USART_TransmitStr_P(PSTR("\n--- Main Menu ---"));
	USART_TransmitStr_P(PSTR("1. Start Game"));
	USART_TransmitStr_P(PSTR("2. View High Scores"));
	USART_TransmitStr_P(PSTR("3. View Difficulty Level"));
	USART_TransmitStr_P(PSTR("4. Exit"));
	USART_TransmitStr_P(PSTR("5. Record Custom Beatmap"));
	USART_TransmitStr_P(PSTR("Enter your choice (1-5): "));
}

void displayExitMessage(void)
{
	USART_TransmitStr_P(PSTR("Goodbye! Thanks for playing Beat Down."));
}

void displayDifficulty(uint16_t pot_value)
{
	char val_buf[12]; // Fixed: Changed from 'char' to array buffer
	
	USART_TransmitStr_P(PSTR("Current Difficulty Configuration:"));
	USART_TransmitStr_P(PSTR("Raw Potentiometer Value: "));
	
	// Converts integer to string directly into a small reusable local buffer
	itoa(pot_value, val_buf, 10);
	USART_TransmitStr(val_buf);
}

void displayHighScores (void)
{
	char val_buf[12];
	char line_buf[160];

	USART_TransmitStr_P(PSTR(" ___________________________________________________________________________________________________________________________________________________"));
	USART_TransmitStr_P(PSTR("|                                                                                                                                                   |"));
	USART_TransmitStr_P(PSTR("|                                                                                                                                                   |"));
	USART_TransmitStr_P(PSTR("|                                           _____________________________________________________________                                           |"));
	USART_TransmitStr_P(PSTR("|                                         /|>>>>>>>>>>>>>>>>>>>>>> Leaderboard <<<<<<<<<<<<<<<<<<<<<<<<<<|                                          |"));
	USART_TransmitStr_P(PSTR("|                                        |/|-------------------------------------------------------------|                                          |"));

	for (uint8_t i = 0; i < 10; i++)
	{
		ltoa((long)top_scores[i].score, val_buf, 10);

		snprintf(
		line_buf,
		sizeof(line_buf),
		"|                                        |/|   #%u   %-3s                         Score: %8s          |                                          |",
		i + 1,
		top_scores[i].initials,
		val_buf
		);

		USART_TransmitStr(line_buf);
	}

	USART_TransmitStr_P(PSTR("|                                        |/|_____________________________________________________________|                                          |"));
	USART_TransmitStr_P(PSTR("|                                        |///////////////////////////////////////////////////////////////                                           |"));
	USART_TransmitStr_P(PSTR("|                                                                                                                                                   |"));
	USART_TransmitStr_P(PSTR("|___________________________________________________________________________________________________________________________________________________|"));
}

/*
void displayHighScores(void)
{
	char val_buf[12]; // Fixed: Changed from 'char' to array buffer
	
	USART_TransmitStr_P(PSTR("\n======================================="));
	USART_TransmitStr_P(PSTR("             LEADERBOARD               "));
	USART_TransmitStr_P(PSTR("======================================="));
	
	for (uint8_t i = 0; i < 10; i++)
	{
		// Transmit index item prefix safely from flash memory
		USART_Transmit('[');
		itoa(i + 1, val_buf, 10);
		USART_TransmitStr(val_buf);
		USART_TransmitStr_P(PSTR("] Initials: "));
		
		// Output player identity strings safely
		USART_TransmitStr(top_scores[i].initials);
		USART_TransmitStr_P(PSTR(" | Score: "));
		
		// Efficient base-10 numerical conversion using 0 working stack bytes
		ltoa((long)top_scores[i].score, val_buf, 10);
		USART_TransmitStr(val_buf);
	}
	USART_TransmitStr_P(PSTR("======================================="));
}
*/

void displayGameOver(uint32_t final_score, uint8_t is_new_high_score)
{
	char val_buf[12]; // Fixed: Changed from 'char' to array buffer

	USART_TransmitStr_P(PSTR(" ___________________________________________________________________________________________________________________________________________________ "));
	USART_TransmitStr_P(PSTR("|                                                                                                                                                   |"));
	USART_TransmitStr_P(PSTR("|                                                                                                                                                   |"));
	USART_TransmitStr_P(PSTR("|                                           _____________________________________________________________                                           |"));
	USART_TransmitStr_P(PSTR("|                                         /|>>>>>>>>>>>>>>>>>>>>>>>>>> Results <<<<<<<<<<<<<<<<<<<<<<<<<<|                                          |"));
	USART_TransmitStr_P(PSTR("|                                        |/|-------------------------------------------------------------|                                          |"));

	// Segmented layout output streams entirely skip large text formatting operations
	USART_TransmitStr_P(PSTR("Final Score: "));
	ltoa((long)final_score, val_buf, 10);
	USART_TransmitStr(val_buf);
	USART_TransmitStr_P(PSTR(" pts"));

	if (is_new_high_score)
	{
		USART_TransmitStr_P(PSTR("|                                        |/|                NEW HIGH SCORE! GREAT JOB!                   |                                          |"));
	}
	else
	{
		char score_line[128];

		ltoa((long)current_score, val_buf, 10);

		snprintf(
		score_line,
		sizeof(score_line),
		                     "|                                        |/|     Your Score: %s (%s)                                 |                                          |",
		val_buf,
		high_score_initials
		);

		USART_TransmitStr(score_line);
	}
	
	USART_TransmitStr_P(PSTR("|                                        |/|_____________________________________________________________|                                          |"));
	USART_TransmitStr_P(PSTR("|                                        |///////////////////////////////////////////////////////////////                                           |"));
	USART_TransmitStr_P(PSTR("|                                                                                                                                                   |"));
	USART_TransmitStr_P(PSTR("|___________________________________________________________________________________________________________________________________________________|"));
}

void displayGameScreen (uint8_t objects_matrix[8][3])
{
	// Print a distinct prefix so Python knows this isn't standard log text
	USART_TransmitStr("GRID:");
	
	for (uint8_t r = 0; r < 8; r++)
	{
		// Compress 3 lanes into 3 bits of a single number (value 0-7)
		uint8_t packed_row = (objects_matrix[r][0] << 2) |
		                     (objects_matrix[r][1] << 1) |
		                     (objects_matrix[r][2]);
		
		// Transmit the number as ASCII text character
		USART_Transmit(packed_row + '0');
		
		// Add commas between rows, except for the last one
		if (r < 7) {
			USART_Transmit(',');
		}
	}
	
	// Terminate line with \r\n so your Python's ser.readline() catches it perfectly
	USART_Transmit('\r');
	USART_Transmit('\n');
}

/*
void displayGameScreen (uint8_t objects_matrix[8][3])
{
	char row_buffer[32];

	USART_TransmitStr_P(PSTR("\033[2J\033[H"));        // clear terminal screen and move cursor to top
	
	USART_TransmitStr_P(PSTR("+-------+-------+-------+"));
	USART_TransmitStr_P(PSTR("|  LEFT |  MID  | RIGHT |"));
	USART_TransmitStr_P(PSTR("+-------+-------+-------+"));

	for (uint8_t r = 0; r < 8; r++)
	{
		row_buffer[0]  = '|';
		row_buffer[1]  = ' ';
		row_buffer[2]  = ' ';
		row_buffer[3]  = ' ';
		row_buffer[4]  = objects_matrix[r][0] ? 'O' : ' ';
		row_buffer[5]  = ' ';
		row_buffer[6]  = ' ';
		row_buffer[7]  = ' ';
		row_buffer[8]  = '|';
		row_buffer[9]  = ' ';
		row_buffer[10] = ' ';
		row_buffer[11] = ' ';
		row_buffer[12] = objects_matrix[r][1] ? 'O' : ' ';
		row_buffer[13] = ' ';
		row_buffer[14] = ' ';
		row_buffer[15] = ' ';
		row_buffer[16] = '|';
		row_buffer[17] = ' ';
		row_buffer[18] = ' ';
		row_buffer[19] = ' ';
		row_buffer[20] = objects_matrix[r][2] ? 'O' : ' ';
		row_buffer[21] = ' ';
		row_buffer[22] = ' ';
		row_buffer[23] = ' ';
		row_buffer[24] = '|';
		row_buffer[25] = '\0';
		USART_TransmitStr(row_buffer);
	}

	USART_TransmitStr_P(PSTR("+-------+-------+-------+"));
	USART_TransmitStr_P(PSTR("| [TAP] | [TAP] | [TAP] |"));
	USART_TransmitStr_P(PSTR("+-------+-------+-------+"));
}
*/
