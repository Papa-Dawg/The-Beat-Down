//======================================================================================================================
// Title:       game.c
// Author:      nathan ramos
// Created:     5/15/2026
// Description: Game logic for Beat Tap (Relative Delta Timeline Model).
//======================================================================================================================
//======================================================================================================================
//                                                 System Settings
//======================================================================================================================
#define F_CPU                  16000000UL
//======================================================================================================================
//                                                     Libraries
//======================================================================================================================
#include <avr/io.h>
#include <stdint.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <util/delay.h>
//======================================================================================================================
//                                                     Imports
//======================================================================================================================
#include "game.h"
#include "adc.h"
#include "button.h"
#include "display.h"
#include "lcd.h"
#include "timer.h"
#include "USART.h"
#include "tilt.h"
//======================================================================================================================
//                                                 Global Variables
//======================================================================================================================
volatile GameStage stage          = RUNNING;
volatile uint8_t   combo          = 0;
volatile uint8_t   multiplier     = 1;
volatile uint8_t   power_up_ready = 0;
volatile uint8_t   power_up_active= 0;
volatile uint8_t   game_over      = 0;
volatile uint8_t   high_score_achieved = 0;
volatile uint32_t  response_time  = 0;
volatile uint32_t  high_score     = 0;
volatile uint32_t  current_score  = 0;
volatile uint8_t   current_lane   = 0xFF;
volatile uint32_t  beat_start_ms  = 0;
char               high_score_initials[4] = "---";
HighScore          top_scores[9];
uint16_t           beat_count     = 0;
uint8_t            game_screen[8][3];                 // 8 rows, 3 lanes
//======================================================================================================================
//                                                   Structs
//======================================================================================================================
Beat beat_map[MAX_BEATS];
//======================================================================================================================
//                                                   Buffers
//======================================================================================================================
char game_conversion_buf[32];              //shared buffer for itoa/ltoa conversions.
//======================================================================================================================
//                                                   Functions
//======================================================================================================================
void setup (void)
{
	USART_Init(MYUBRR);
	lcd_init();
	button_init();
	timer_init();
	ADC_init();
	tilt_init();
	
	for (uint8_t i = 0; i < 9; i++)
	{
		top_scores[i].score = 0;

		top_scores[i].initials[0] = '-';
		top_scores[i].initials[1] = '-';
		top_scores[i].initials[2] = '-';
		top_scores[i].initials[3] = '\0';
	}

	srand(TCNT1);

	sei();

	LED_DDR;
	LED_DDR_RED;
	
	loadHighScores();
	receiveBeatMap();

	//displayStartMessage();
	
	waitForEnter();

	lcd_putcmd(LCD_SET_CURSOR | FIRST_ROW);
	lcd_puts((uint8_t *)" The Beat Down! ");

	lcd_putcmd(LCD_SET_CURSOR | SECOND_ROW);
	lcd_puts((uint8_t *)"Press to start! ");
}

void mainMenu(void)
{
	displayMainMenu();

	uint8_t lastDifficulty = 255;

	while (1)
	{
		// =========================================
		// Read potentiometer continuously
		// =========================================

		uint16_t adc = ADC_read(POT_CHANNEL);

		uint8_t difficulty;

		if (adc < 342)
		{
			difficulty = 0; // EASY
		}
		else if (adc < 683)
		{
			difficulty = 1; // MEDIUM
		}
		else
		{
			difficulty = 2; // HARD
		}

		// Only transmit when changed
		if (difficulty != lastDifficulty)
		{
			lastDifficulty = difficulty;

			switch (difficulty)
			{
				case 0:
				USART_TransmitStr("DIFFICULTY:EASY");
				break;

				case 1:
				USART_TransmitStr("DIFFICULTY:MEDIUM");
				break;

				case 2:
				USART_TransmitStr("DIFFICULTY:HARD");
				break;
			}
		}

		// =========================================
		// Non-blocking serial input
		// =========================================

		if (UCSR0A & (1 << RXC0))
		{
			char choice = UDR0;

			switch (choice)
			{
				case '1':
				startGame();
				return;

				case '2':
				game_over = 0;
				high_score_achieved = 0;
				displayHighScores();
				waitForEnter();
				displayMainMenu();
				break;

				case '3':
				recordBeats();
				return;
				
				case '4':
				sensorDebug();
				displayMainMenu();
				return;

				case '5':
				displayExitMessage();
				return;
			}
		}

		_delay_ms(50);
	}
}

void startGame (void)
{
	if (beat_count == 0)
	{
		USART_TransmitStr_P(PSTR("No beat map loaded."));
		mainMenu();
		return;
	}

	combo      = 0;
	multiplier = 1;
	game_over  = 0;
	stage      = RUNNING;

	lcd_putcmd(LCD_CLEAR);
	lcd_putcmd(LCD_SET_CURSOR | FIRST_ROW);
	lcd_puts((uint8_t *)"    Ready...    ");
	_delay_ms(1000);

	lcd_putcmd(LCD_SET_CURSOR | FIRST_ROW);
	lcd_puts((uint8_t *)"  3... 2... 1.. ");
	_delay_ms(1000);

	lcd_putcmd(LCD_CLEAR);
	lcd_putcmd(LCD_SET_CURSOR | FIRST_ROW);
	lcd_puts((uint8_t *)"  GO! GO! GO!   ");

	// Clear screen buffer
	for (uint8_t r = 0; r < 8; r++)
	{
		for (uint8_t c = 0; c < 3; c++)
		{
			game_screen[r][c] = 0;
		}
	}

	USART_TransmitStr_P(PSTR("START"));

	cli();
	ms_counter = 0;
	sei();

	uint16_t beat_index  = 0;
	uint16_t spawn_index = 0;

	uint32_t last_tick = 0;

	// MASTER ABSOLUTE TIMELINES
	uint32_t hit_timeline   = 0;
	uint32_t spawn_timeline = 0;

	//while (beat_index < beat_count && game_over == 0)
	while ((beat_index < beat_count || stage == WAITING) && !game_over)
	{
		checkLight();

		if (stage == PAUSED)
		{
			continue;
		}
		
		checkTilt();

		uint32_t current_ms;

		cli();
		current_ms = ms_counter;
		sei();
		
		//==================================================
		// INPUT HANDLING
		//==================================================
		if (left_button_pressed || middle_button_pressed || right_button_pressed)
		{
			uint8_t pressed_lane = 0xFF;

			// CONSUME the button immediately
			if (left_button_pressed)
			{
				pressed_lane = 0;
				left_button_pressed = 0;
			}
			else if (middle_button_pressed)
			{
				pressed_lane = 1;
				middle_button_pressed = 0;
			}
			else if (right_button_pressed)
			{
				pressed_lane = 2;
				right_button_pressed = 0;
			}

			//==================================================
			// VALID HIT WINDOW
			//==================================================
			if (stage == WAITING)
			{
				if (pressed_lane == current_lane)
				{
					measuring();
				}
				else
				{
					// WRONG LANE HIT
					combo           = 0;
					multiplier      = 1;
					power_up_ready  = 0;
					power_up_active = 0;

					char score_buf[16];
					char combo_buf[8];
					char mult_buf[8];

					ltoa(current_score, score_buf, 10);
					itoa(combo, combo_buf, 10);
					itoa(multiplier, mult_buf, 10);

					USART_TransmitNoAdd("HUD:");
					USART_TransmitNoAdd(score_buf);
					USART_Transmit(',');
					USART_TransmitNoAdd(combo_buf);
					USART_Transmit(',');
					USART_TransmitNoAdd(mult_buf);
					USART_Transmit('\n');

					RED_ON;
					BLUE_OFF;

					if (current_lane < 3)
					{
						game_screen[7][current_lane] = 0;
					}

					TCCR1B &= ~((1<<CS12) | (1<<CS10));
					TCNT1 = 0;
					stage = RUNNING;

					//USART_TransmitStr_P(PSTR("STATUS:WRONG LANE"));
				}
			}
			
			/*
			//==================================================
			// EARLY / SPAM INPUT
			//==================================================
			else
			{
				combo = 0;
				multiplier = 1;
				power_up_ready = 0;

				char score_buf[16];
				char combo_buf[8];
				char mult_buf[8];

				ltoa(current_score, score_buf, 10);
				itoa(combo, combo_buf, 10);
				itoa(multiplier, mult_buf, 10);

				USART_TransmitNoAdd("HUD:");
				USART_TransmitNoAdd(score_buf);
				USART_Transmit(',');
				USART_TransmitNoAdd(combo_buf);
				USART_Transmit(',');
				USART_TransmitNoAdd(mult_buf);
				USART_Transmit('\n');

				RED_ON;
				BLUE_OFF;

				USART_TransmitStr_P(PSTR("STATUS:EARLY/SPAM"));
			}
			*/
		}

		//==================================================
		// SCROLL DISPLAY
		//==================================================
		if ((current_ms - last_tick) >= TICK_INTERVAL)
		{
			last_tick = current_ms;

			// shift downward
			for (int8_t r = 7; r > 0; r--)
			{
				game_screen[r][0] = game_screen[r-1][0];
				game_screen[r][1] = game_screen[r-1][1];
				game_screen[r][2] = game_screen[r-1][2];
			}

			// clear top row
			game_screen[0][0] = 0;
			game_screen[0][1] = 0;
			game_screen[0][2] = 0;

			// spawn notes
			while (spawn_index < beat_count)
			{
				uint16_t packed =
				beat_map[spawn_index].delta_and_lane;

				uint32_t delta =
				(packed & 0x3FFF);

				uint8_t lane =
				(uint8_t)((packed >> 14) & 0x03);

				uint32_t note_time =
				spawn_timeline + delta;

				// spawn 800ms early
				if ((current_ms + SCROLL_TIME) >= note_time)
				{
					if (lane < 3)
					{
						game_screen[0][lane] = 1;
					}

					spawn_timeline = note_time;
					spawn_index++;
				}
				else
				{
					break;
				}
			}

			displayGameScreen(game_screen);
		}

		//==================================================
		// HIT WINDOW TRIGGER
		//==================================================
		if (beat_index < beat_count)
		{
			uint16_t packed = beat_map[beat_index].delta_and_lane;
			uint32_t delta = (packed & 0x3FFF);
			uint8_t lane = (uint8_t)((packed >> 14) & 0x03);
			uint32_t target_time = hit_timeline + delta;

			// Calculate your dynamic window based on the potentiometer
			//uint16_t window = ADC_read(POT_CHANNEL) * 40 / 1023 + 50;
			
			uint16_t window = getTimingWindow();

			// Open the hit window early!
			if (stage != WAITING && (current_ms + window) >= target_time)
			{
				current_lane = lane;
				stage = WAITING;
				
				// Set the reference point to the ACTUAL target, not current_ms
				beat_start_ms = target_time;

				BLUE_ON;
				itoa(lane, game_conversion_buf, 10);
				//USART_TransmitLine("STATUS:BEAT", ':', game_conversion_buf);

				TCNT1 = 0;
				TCCR1B |= (1<<CS12) | (1<<CS10);
				hit_timeline = target_time;
				beat_index++;
			}
		}
		
		if (stage == WAITING)
		{
			uint16_t ticks = TCNT1;

			// Convert Timer1 ticks to elapsed milliseconds since the window opened
			uint32_t elapsed_ms = ((uint32_t)ticks * 64UL) / 16000UL;

			uint16_t window = ADC_read(POT_CHANNEL) * 40 / 1023 + 50;

			// The window spans from (-window) to (+window), so the total duration is window * 2
			if (elapsed_ms > (uint32_t)(window * 2))
			{
				// MISS
				TCCR1B &= ~((1<<CS12) | (1<<CS10)); // Stop Timer1
				TCNT1 = 0;

				combo           = 0;
				multiplier      = 1;
				power_up_ready  = 0;
				power_up_active = 0;
				
				char score_buf[16];
				char combo_buf[8];
				char mult_buf[8];

				ltoa(current_score, score_buf, 10);
				itoa(combo, combo_buf, 10);
				itoa(multiplier, mult_buf, 10);

				USART_TransmitNoAdd("HUD:");
				USART_TransmitNoAdd(score_buf);
				USART_Transmit(',');
				USART_TransmitNoAdd(combo_buf);
				USART_Transmit(',');
				USART_TransmitNoAdd(mult_buf);
				USART_Transmit('\n');

				RED_ON;
				BLUE_OFF;

				stage = RUNNING;

				//USART_TransmitStr_P(PSTR("STATUS:MISS"));
			}
		}
	}

	game_over = 1;
	
	gameOver();
}

void measuring(void)
{
	uint32_t current_ms;

	cli();
	current_ms = ms_counter;
	sei();
	
	// Calculate how far off the button press was from the absolute target beat point
	int32_t offset = (int32_t)current_ms - (int32_t)beat_start_ms;

	if (offset < 0)
	{
		offset = -offset;
	}

	response_time = (uint32_t)offset;

	// Safely scale the lookahead window to a max of 90ms (Option A)
	// This prevents the hit window from opening while the note is visually in Row 6
	//uint16_t window = ADC_read(POT_CHANNEL) * 40 / 1023 + 50;  //50ms - 90ms window
	
	uint16_t window = getTimingWindow();

	if (response_time <= window) // Valid hit within the target row window!
	{
		// Calculate a clean accuracy percentage (higher percentage = closer to 0ms offset)
		uint8_t accuracy = 100 - (uint8_t)(response_time * 100 / window);
		current_score += (accuracy * multiplier);     // Increment score

		combo++;
		
		if (combo >= 30)
		{
			multiplier = 4;
			
			if (combo >= 30 && power_up_active)
			{
				multiplier = 8;
			}
		}
		else if (combo >= 20)
		{
			multiplier = 3;
			
			if (combo >= 20 && power_up_active)
			{
				multiplier = 6;
			}
		}
		else if (combo >= 10)
		{
			multiplier = 2;
			
			if (combo >= 10 && power_up_active)
			{
				multiplier = 4;
			}
		}
		else
		{
			multiplier = 1;
			
			if (power_up_active)
			{
				multiplier = 2;
			}
			
		}
		
		//==================================================
		// SEND HUD UPDATE TO PYTHON
		//==================================================

		char score_buf[16];
		char combo_buf[8];
		char mult_buf[8];

		ltoa(current_score, score_buf, 10);
		itoa(combo, combo_buf, 10);
		itoa(multiplier, mult_buf, 10);

		USART_TransmitNoAdd("HUD:");
		USART_TransmitNoAdd(score_buf);
		USART_Transmit(',');
		USART_TransmitNoAdd(combo_buf);
		USART_Transmit(',');
		USART_TransmitNoAdd(mult_buf);
		USART_Transmit('\n');
		
		// Provide immediate sensory feedback
		BLUE_OFF;
		RED_OFF;
		//_//delay_ms(5); // Fast flash feedback
		BLUE_ON;

		if (combo >= COMBO_THRESHOLD)
		{
			power_up_ready = 1;
			lcd_putcmd(LCD_SET_CURSOR | SECOND_ROW);
			lcd_puts((uint8_t *)"POWER-UP READY!");
		}
		
		// CLEAR the note from the target line Row 7 so it disappears immediately on a successful tap!
		if (current_lane < 3)
		{
			game_screen[7][current_lane] = 0;
		}
	}
	else // Tapped, but somehow fell outside the timing bounds
	{
		combo           = 0;
		multiplier      = 1;
		power_up_ready  = 0;
		power_up_active = 0;
		
		char score_buf[16];
		char combo_buf[8];
		char mult_buf[8];

		ltoa(current_score, score_buf, 10);
		itoa(combo, combo_buf, 10);
		itoa(multiplier, mult_buf, 10);

		USART_TransmitNoAdd("HUD:");
		USART_TransmitNoAdd(score_buf);
		USART_Transmit(',');
		USART_TransmitNoAdd(combo_buf);
		USART_Transmit(',');
		USART_TransmitNoAdd(mult_buf);
		USART_Transmit('\n');
		
		RED_ON;
		BLUE_OFF;
		//USART_TransmitStr_P(PSTR("STATUS:TIMING MISS"));
	}

	// Turn off Timer1 tracking and restore status back to active polling
	TCCR1B &= ~((1<<CS12) | (1<<CS10));
	TCNT1 = 0;
	stage = RUNNING;
}

void gameOver (void)
{
	RED_OFF;
	BLUE_OFF;

	// GUARD: If current_score is 0, they didn't just play a game (e.g. they are just navigating menus)
	if (current_score == 0)
	{
		high_score_achieved = 0;
		displayHighScores();
		mainMenu();
		return;
	}

	high_score_achieved = 0;

	for (uint8_t i = 0; i < 9; i++)
	{
		if (current_score > top_scores[i].score)
		{
			high_score_achieved = 1;
			break;
		}
	}

	displayGameOver(current_score, high_score_achieved);
	
	cli();
	waitForEnter();
	sei();

	if (high_score_achieved)
	{
		char initials[4];
		uint8_t idx = 0;

		USART_TransmitStr_P(PSTR("Enter initials: "));

		while (idx < 3)
		{
			char c = USART_Receive();

			if (c == '\r' || c == '\n')
			{
				continue;
			}

			initials[idx++] = c;
		}

		initials[3] = '\0';
		
		while (UCSR0A & (1 << RXC0))
		{
			volatile char throwaway = UDR0;
		}

		// Insert score into leaderboard
		for (int8_t i = 8; i >= 0; i--)
		{
			if (i == 0 || current_score <= top_scores[i - 1].score)
			{
				top_scores[i].score = current_score;

				top_scores[i].initials[0] = initials[0];
				top_scores[i].initials[1] = initials[1];
				top_scores[i].initials[2] = initials[2];
				top_scores[i].initials[3] = '\0';

				break;
			}

			top_scores[i] = top_scores[i - 1];
		}
		
		saveHighScores();
	}

	// Reset current_score to 0 so this block cannot accidentally re-trigger
	current_score = 0;
	high_score_achieved = 0;

	displayHighScores();
	
	cli();
	waitForEnter();
	sei();
	
	mainMenu();
}

void transmitBeatMap (void)
{
	USART_TransmitStr_P(PSTR("BEATMAP_START"));

	uint32_t current_absolute_ms = 0;
	char lane_buf[32];
	for (uint16_t i = 0; i < beat_count; i++)
	{
		uint16_t packed_data = beat_map[i].delta_and_lane;
		
		// Extract delta from lower 14 bits and lane from upper 2 bits
		uint32_t delta = packed_data & 0x3FFF;
		uint8_t lane   = (uint8_t)((packed_data >> 14) & 0x03);
		
		// Accumulate deltas to send raw absolute timestamps back to Python's file recorder
		current_absolute_ms += delta;
		
		ltoa((long)current_absolute_ms, game_conversion_buf, 10);
		itoa(lane, lane_buf, 10);
		USART_TransmitLine(game_conversion_buf, ',', lane_buf);
	}

	USART_TransmitStr_P(PSTR("BEATMAP_END"));
}

void recordBeats (void)
{
	beat_count = 0;
	uint32_t last_beat_ms = 0;

	USART_TransmitStr_P(PSTR("START"));

	lcd_putcmd(LCD_SET_CURSOR | FIRST_ROW);
	lcd_puts((uint8_t *)" Recording...   ");
	lcd_putcmd(LCD_SET_CURSOR | SECOND_ROW);
	lcd_puts((uint8_t *)"L=lft M=mid R=rt");

	USART_TransmitStr_P(PSTR("Recording! Tap along to the song."));
	USART_TransmitStr_P(PSTR("Song ends automatically when MAX_BEATS reached."));

	// Clear clock latency cleanly right before intercepting input loops
	cli();
	ms_counter = 0;
	sei();

	while (beat_count < MAX_BEATS)
	{
		if (UCSR0A & (1 << RXC0))
		{
			char incoming = UDR0;
			if (incoming == 's' || incoming == 'S')
			{
				USART_TransmitStr_P(PSTR("Song ended early by host."));
				break;
			}
		}
		
		uint8_t pressed = 0;
		uint8_t target_lane = 0;

		if (left_button_pressed)
		{
			left_button_pressed = 0;
			target_lane = 0;
			pressed = 1;
			USART_TransmitStr_P(PSTR("L"));
		}
		else if (middle_button_pressed)
		{
			middle_button_pressed = 0;
			target_lane = 1;
			pressed = 1;
			USART_TransmitStr_P(PSTR("M"));
		}
		else if (right_button_pressed)
		{
			right_button_pressed = 0;
			target_lane = 2;
			pressed = 1;
			USART_TransmitStr_P(PSTR("R"));
		}

		if (pressed)
		{
			uint32_t current_time = ms_counter;
			uint32_t delta = current_time - last_beat_ms;
			
			// Clip delta to max capacity of 14 bits (16383ms) to preserve register safety
			if (delta > 0x3FFF) delta = 0x3FFF;

			// Pack delta into lower 14 bits, lane into upper 2 bits (bits 14-15)
			uint16_t packed_data = (delta & 0x3FFF) | ((uint16_t)(target_lane & 0x03) << 14);
			
			beat_map[beat_count].delta_and_lane = packed_data;
			beat_count++;
			
			last_beat_ms = current_time;
			BLUE_ON;
		}
	}

	transmitBeatMap();

	USART_TransmitStr_P(PSTR("Recording complete!"));
	lcd_putcmd(LCD_SET_CURSOR | FIRST_ROW);
	lcd_puts((uint8_t *)"Recording done! ");
}

void receiveBeatMap (void)
{
	char    line[32];
	uint8_t idx = 0;
	
	beat_count = 0;
	uint32_t last_timestamp = 0; // Tracks preceding absolute time to compute relative delta
	
	USART_TransmitStr_P(PSTR("READY_FOR_BEATMAP"));

	while (1)
	{
		char c = USART_Receive();

		if (c == '\r') continue;
		
		if (c == '\n')
		{
			line[idx] = '\0';

			// Safe intercept: Read keyword before clearing index variable
			if (line[0] == 'L' && line[1] == 'O' && line[2] == 'A' && line[3] == 'D')
			{
				break;
			}

			idx = 0;

			if (beat_count >= MAX_BEATS) continue;

			uint32_t timestamp = 0;
			uint8_t  lane      = 0;
			uint8_t  i         = 0;

			while (line[i] != ',' && line[i] != '\0')
			{
				timestamp = timestamp * 10 + (line[i] - '0');
				i++;
			}
			
			if (line[i] == ',') i++;
			lane = line[i] - '0';

			// Safe fallback validation for standard 0-2 indexing
			if (lane > 2)
			{
				if (lane >= 1 && lane <= 3) lane--;
				else continue;
			}

			// Calculate the relative delta expected by your working startGame loop
			uint32_t delta = timestamp - last_timestamp;
			if (delta > 0x3FFF) delta = 0x3FFF;

			// Pack into the 16-bit field matching delta_and_lane structural shifts
			uint16_t packed_data = (delta & 0x3FFF) | ((uint16_t)(lane & 0x03) << 14);

			beat_map[beat_count].delta_and_lane = packed_data;
			beat_count++;

			last_timestamp = timestamp; // Step timeline index forward
		}
		else
		{
			if (idx < 31) line[idx++] = c;
		}
	}

	// Flush residual trailing bytes from UART hardware pipeline
	volatile char throwaway;
	while (UCSR0A & (1 << RXC0))
	{
		throwaway = UDR0;
	}

	USART_TransmitStr_P(PSTR("Beat map loaded!"));
	itoa(beat_count, game_conversion_buf, 10);
	USART_TransmitStr(game_conversion_buf);
	USART_TransmitStr_P(PSTR(" beats ready."));
}

void saveHighScores (void)
{
	eeprom_write_byte((uint8_t *)EEPROM_MAGIC_ADDR, EEPROM_MAGIC);
	eeprom_write_block(
	(const void *)top_scores,
	(void *)EEPROM_SCORES_ADDR,
	sizeof(top_scores)
	);
}

void loadHighScores (void)
{
	uint8_t magic = eeprom_read_byte((uint8_t *)EEPROM_MAGIC_ADDR);

	if (magic != EEPROM_MAGIC)             // first boot, no data yet
	{
		for (uint8_t i = 0; i < 9; i++)
		{
			top_scores[i].score = 0;
			top_scores[i].initials[0] = '-';
			top_scores[i].initials[1] = '-';
			top_scores[i].initials[2] = '-';
			top_scores[i].initials[3] = '\0';
		}
		return;
	}

	eeprom_read_block(
	(void *)top_scores,
	(const void *)EEPROM_SCORES_ADDR,
	sizeof(top_scores)
	);
}

void waitForEnter(void)
{
	//USART_TransmitStr_P(PSTR("PRESS ENTER"));

	// Flush old buffered bytes first
	while (UCSR0A & (1 << RXC0))
	{
		volatile char throwaway = UDR0;
	}

	// Wait for fresh keypress
	USART_Receive();

	// Flush trailing garbage
	while (UCSR0A & (1 << RXC0))
	{
		volatile char throwaway = UDR0;
	}
}

void sensorDebug(void)
{
	USART_TransmitStr_P(PSTR("SENSOR_DEBUG_START"));
	
	while (UCSR0A & (1 << RXC0))
	{
		volatile char throwaway = UDR0;
	}

	while (1)
	{
		// Exit on key press
		if (UCSR0A & (1 << RXC0))
		{
			volatile char throwaway = UDR0;

			USART_TransmitStr_P(PSTR("SENSOR_DEBUG_END"));
			break;
		}

		int16_t x = getTiltX();

		uint16_t pot   = ADC_read(POT_CHANNEL);
		uint16_t light = ADC_read(PHOTO_CHANNEL);

		char xbuf[16];
		char pbuf[16];
		char lbuf[16];

		itoa(x, xbuf, 10);
		itoa(pot, pbuf, 10);
		itoa(light, lbuf, 10);

		USART_TransmitNoAdd("SENSOR:");

		USART_TransmitNoAdd(xbuf);
		USART_Transmit(',');

		USART_TransmitNoAdd(pbuf);
		USART_Transmit(',');

		USART_TransmitNoAdd(lbuf);

		USART_Transmit('\n');

		_delay_ms(100);
	}
}