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
volatile uint8_t   game_over      = 0;
volatile uint8_t   high_score_achieved = 0;
volatile uint32_t  response_time  = 0;
volatile uint32_t  high_score     = 0;
volatile uint32_t  current_score  = 0;
volatile uint8_t   current_lane   = 0xFF;
volatile uint32_t  beat_start_ms  = 0;
char               high_score_initials[4] = "---";
HighScore          top_scores[10];
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
	
	for (uint8_t i = 0; i < 10; i++)
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
	
	receiveBeatMap();

	displayStartMessage();

	lcd_putcmd(LCD_SET_CURSOR | FIRST_ROW);
	lcd_puts((uint8_t *)" The Beat Down! ");

	lcd_putcmd(LCD_SET_CURSOR | SECOND_ROW);
	lcd_puts((uint8_t *)"Press to start! ");
}

void mainMenu (void)
{
	displayMainMenu();

	while (1)
	{
		char choice = USART_Receive();

		switch (choice)
		{
			case '1': startGame();                    return;
			case '2': displayHighScores();            return;
			case '3': displayDifficulty(ADC_read(0)); return;
			case '4': displayExitMessage();           return;
			case '5': recordBeats();                  return;
			default:
			USART_TransmitStr_P(PSTR("Invalid option. Try again."));
			break;
		}
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

	while (beat_index < beat_count && game_over == 0)
	{
		if (stage == PAUSED)
		{
			continue;
		}

		uint32_t current_ms;

		cli();
		current_ms = ms_counter;
		sei();
		
		if (stage == WAITING)
		{
			if ((left_button_pressed   && current_lane == 0) ||
			(middle_button_pressed && current_lane == 1) ||
			(right_button_pressed  && current_lane == 2))
			{
				measuring();
			}
			else if (left_button_pressed ||
			middle_button_pressed ||
			right_button_pressed)
			{
				// WRONG BUTTON

				combo = 0;
				multiplier = 1;
				power_up_ready = 0;

				RED_ON;
				BLUE_OFF;

				TCCR1B = 0;
				TCNT1 = 0;

				stage = RUNNING;

				USART_TransmitStr_P(PSTR("WRONG"));
			}

			left_button_pressed   = 0;
			middle_button_pressed = 0;
			right_button_pressed  = 0;
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
		// HIT WINDOW
		//==================================================
		if (beat_index < beat_count)
		{
			uint16_t packed =
			beat_map[beat_index].delta_and_lane;

			uint32_t delta =
			(packed & 0x3FFF);

			uint8_t lane =
			(uint8_t)((packed >> 14) & 0x03);

			uint32_t target_time =
			hit_timeline + delta;

			if (stage != WAITING && current_ms >= target_time)
			{
				current_lane = lane;
				
				stage = WAITING;
				
				beat_start_ms = current_ms;

				BLUE_ON;

				itoa(lane, game_conversion_buf, 10);
				USART_TransmitLine("BEAT", ':', game_conversion_buf);

				TCNT1 = 0;

				TCCR1B |= (1<<CS12) | (1<<CS10);

				hit_timeline = target_time;

				beat_index++;
			}
		}
		
		if (stage == WAITING)
		{
			uint16_t ticks = TCNT1;

			uint32_t elapsed_ms =
			((uint32_t)ticks * 64UL) / 16000UL;

			uint16_t window =
			ADC_read(POT_CHANNEL) * 200 / 1023 + 50;

			if (elapsed_ms > window)
			{
				// MISS

				TCCR1B &= ~((1<<CS12) | (1<<CS10));
				TCNT1 = 0;

				combo = 0;
				multiplier = 1;
				power_up_ready = 0;

				RED_ON;
				BLUE_OFF;

				stage = RUNNING;

				USART_TransmitStr_P(PSTR("MISS"));
			}
		}
	}

	game_over = 1;
	
	gameOver();
}

/*
void startGame (void)
{
	if (beat_count == 0)
	{
		USART_TransmitStr_P(PSTR("No beat map loaded. Record one first (option 5)."));
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

	// Clear the global game screen buffer
	for (uint8_t r = 0; r < 8; r++)
	{
		game_screen[r][0] = 0;
		game_screen[r][1] = 0;
		game_screen[r][2] = 0;
	}

	USART_TransmitStr_P(PSTR("START"));
	
	ms_counter             = 0;
	uint16_t beat_index    = 0;          // tracks which beat to trigger at bottom
	uint16_t spawn_index   = 0;          // tracks which beat to spawn at top
	uint32_t last_tick     = 0;          // tracks last scroll tick
	uint32_t target_ms     = 0;          // absolute ms of next beat to trigger at bottom
	uint32_t abs_target_ms = 0;
	
	// Dynamic tracking variables to replace the broken 2.8KB array
	uint32_t running_spawn_total = 0;
	//uint32_t next_spawn_ms = 0;

	// Initialize targets using the first beat
	if (beat_count > 0)
	{
		abs_target_ms       = beat_map[0].delta_and_lane & 0x3FFF;
		target_ms           = abs_target_ms;
		running_spawn_total = 0;
		
		// Spawning happens SCROLL_TIME (800ms) before hitting the bottom
		//next_spawn_ms = (running_spawn_total > SCROLL_TIME) ? (running_spawn_total - SCROLL_TIME) : 0;
	}

	while (beat_index < beat_count && game_over == 0)
	{
		//checkLight();
		//checkTilt();

		if (stage == PAUSED) continue;

		// Scroll screen every TICK_INTERVAL ms (100ms)
		if (ms_counter - last_tick >= TICK_INTERVAL)
		{
			last_tick = ms_counter; //last_tick += TICK_INTERVAL;

			// Shift rows down from row 7 down to row 1
			for (int8_t r = 7; r > 0; r--)
			{
				game_screen[r][0] = game_screen[r-1][0];
				game_screen[r][1] = game_screen[r-1][1];
				game_screen[r][2] = game_screen[r-1][2];
			}

			// Clear the top row so it can receive new spawns
			game_screen[0][0] = 0;
			game_screen[0][1] = 0;
			game_screen[0][2] = 0;

			while (spawn_index < beat_count)
			{
				// Calculate what the absolute timestamp of this note actually is
				uint32_t next_note_absolute_ms = running_spawn_total + (beat_map[spawn_index].delta_and_lane & 0x3FFF);

				// Is this note scheduled to hit the bottom within our 800ms lookahead window?
				if ((ms_counter + SCROLL_TIME) >= next_note_absolute_ms)
				{
					uint8_t lane = (beat_map[spawn_index].delta_and_lane >> 14) & 0x03;
					game_screen[0][lane] = 1; // Drop it on the top row!
					
					// Lock in this delta to our absolute timeline progress
					running_spawn_total = next_note_absolute_ms;
					spawn_index++;
				}
				else
				{
					// The next note is too far in the future! Stop checking for this frame.
					break;
				}
			}
			// Redraw the screen over USART to your Python console terminal
			displayGameScreen(game_screen);
		}

		// Trigger beat hit window when beat reaches bottom
		if (ms_counter >= target_ms)
		{
			BLUE_ON;

			uint8_t lane = (beat_map[beat_index].delta_and_lane >> 14) & 0x03;
			itoa(lane, game_conversion_buf, 10);
			USART_TransmitLine("BEAT", ':', game_conversion_buf);

			TCNT1 = 0;
			TCCR1B |= (1<<CS12)|(1<<CS10);     // start hit response window timer

			beat_index++;

			// Track absolute target time for the next beat hit window
			if (beat_index < beat_count)
			{
				abs_target_ms += (beat_map[beat_index].delta_and_lane & 0x3FFF);
				target_ms = abs_target_ms;
			}
		}
	}

	game_over = 1;
}
*/

void measuring (void)
{
	uint32_t current_ms;

	cli();
	current_ms = ms_counter;
	sei();
	
	int32_t offset =
	(int32_t)current_ms - (int32_t)beat_start_ms;

	if (offset < 0)
	{
		offset = -offset;
	}

	response_time = (uint32_t)offset;

	//response_time = current_ms - beat_start_ms;
	
	//uint16_t ticks = TCNT1;

	//TCCR1B &= ~((1<<CS12)|(1<<CS10));             //stop Timer1.
	//TCNT1 = 0;                                    //reset timer.

	//response_time = ((uint32_t)ticks * 1000UL) / 15625UL;  //convert ticks to ms.

	uint16_t window = ADC_read(POT_CHANNEL) * 200 / 1023 + 50;  //50-250ms window.

	if (response_time <= window)                   //hit within window:
	{
		uint8_t accuracy = 100 - (uint8_t)(response_time * 100 / window);
		current_score += (accuracy * multiplier);     //add to score.

		combo++;
		BLUE_ON;
		RED_OFF;

		if (combo >= COMBO_THRESHOLD)
		{
			power_up_ready = 1;
			lcd_putcmd(LCD_SET_CURSOR | SECOND_ROW);
			lcd_puts((uint8_t *)"POWER-UP READY!");
		}
	}
	else                                           //miss:
	{
		combo          = 0;
		multiplier     = 1;
		power_up_ready = 0;
		RED_ON;
		BLUE_OFF;
	}

	stage = RUNNING;
}

void gameOver (void)
{
	RED_OFF;
	BLUE_OFF;

	high_score_achieved = 0;

	for (uint8_t i = 0; i < 10; i++)
	{
		if (current_score > top_scores[i].score)
		{
			high_score_achieved = 1;
			break;
		}
	}

	displayGameOver(current_score, high_score_achieved);

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

		// Insert score into leaderboard
		for (int8_t i = 9; i >= 0; i--)
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
	}

	displayHighScores();

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