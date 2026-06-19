//======================================================================================================================
// Title:       game.c
// Author:      nathan ramos
// Created:     5/15/2026
// Description: Game logic module for THE BEAT DOWN! (TM).
//======================================================================================================================
//======================================================================================================================
//                                                 System Settings
//======================================================================================================================
#define F_CPU                  16000000UL
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
volatile GameStage stage                  = RUNNING;                         //keeps the current state of game.
volatile uint8_t   combo                  = 0;                               //tracks combo.
volatile uint8_t   multiplier             = 1;                               //tracks multiplier.
volatile uint8_t   power_up_ready         = 0;                               //tracks if power-up ready.
volatile uint8_t   power_up_active        = 0;                               //tracks if power-up active.
volatile uint8_t   game_over              = 0;                               //tracks if game is over.
volatile uint8_t   high_score_achieved    = 0;                               //tracks if high score was achieved.
volatile uint32_t  response_time          = 0;                               //tracks response time.
volatile uint32_t  high_score             = 0;                               //holds high score.
volatile uint32_t  current_score          = 0;                               //tracks current score.
volatile uint8_t   current_lane           = 0xFF;                            //tracks current lane.
volatile uint32_t  beat_start_ms          = 0;                               //holds beat time value.
char               high_score_initials[4] = "---";                           //holds user initials.
HighScore          top_scores[9];                                            //holds all high scores.
uint16_t           beat_count             = 0;                               //tracks beat count.
uint8_t            game_screen[8][3];                                        //game board - 8 rows, 3 lanes.
//======================================================================================================================
//                                                     Structs
//======================================================================================================================
Beat beat_map[MAX_BEATS];                                                    //holds the whole beat map.
//======================================================================================================================
//                                                     Buffers
//======================================================================================================================
char game_conversion_buf[32];                                                //shared buffer for itoa/ltoa conversions.
//======================================================================================================================
//                                                    Functions
//======================================================================================================================

/* ------------------------------------------------------------------------------------------------------------------ */
/* ---------------------------------------------------- Setup ------------------------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */
void setup (void)                                                    //handles all the setup.
{
	USART_Init(MYUBRR);                                              //initializes USART.
	lcd_init();                                                      //initializes the LCD.
	button_init();                                                   //initializes the buttons.
	timer_init();                                                    //initializes the timers.
	ADC_init();                                                      //initializes ADC (potentiometer and photoresistor)
	tilt_init();                                                     //initializes the accelerometer.
	
	for (uint8_t i = 0; i < 9; i++)                                  //initializes the high score array in SRAM
	{
		top_scores[i].score = 0;

		top_scores[i].initials[0] = '-';
		top_scores[i].initials[1] = '-';
		top_scores[i].initials[2] = '-';
		top_scores[i].initials[3] = '\0';
	}

	srand(TCNT1);                                                    //for randomness.

	sei();                                                           //enables global interrupts.

	LED_DDR;                                                         //DDR for blue LED.
	LED_DDR_RED;                                                     //DDR for red LED.
	
	loadHighScores();                                                //loads high scores from EEPROM to SRAM.
	receiveBeatMap();                                                //receives beat map from song.py.
	
	waitForEnter();                                                  //press enter to continue function.

	lcd_putcmd(LCD_SET_CURSOR | FIRST_ROW);
	lcd_puts((uint8_t *)" The Beat Down! ");

	lcd_putcmd(LCD_SET_CURSOR | SECOND_ROW);
	lcd_puts((uint8_t *)"Press to start! ");
}
/* ------------------------------------------------------------------------------------------------------------------ */
/* -------------------------------------------------- Main Menu ----------------------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */
void mainMenu(void)
{
	displayMainMenu();

	uint8_t lastDifficulty = 255;                                    //default value for difficulty.

	while (1)
	{
		//==========================================================
		// Reads potentiometer continuously
		//==========================================================

		uint16_t adc = ADC_read(POT_CHANNEL);                        //reads potentiometer value.

		uint8_t difficulty;

		if (adc < 342)                                               //determines difficulty level.
		{
			difficulty = 0;
		}
		else if (adc < 683)
		{
			difficulty = 1;
		}
		else
		{
			difficulty = 2;
		}

		if (difficulty != lastDifficulty)                            //sends difficulty value to song.py
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

		//==========================================================
		// Main Menu Choice Logic
		//==========================================================

		if (UCSR0A & (1 << RXC0))                                    //if something received from terminal:
		{
			char choice = UDR0;                                      //gets input.

			switch (choice)                                          //chooses an option based on input:
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

/* ------------------------------------------------------------------------------------------------------------------ */
/* ------------------------------------------------- Actual Game ---------------------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */
void startGame (void)
{
	//==============================================================
	// Setup
	//==============================================================
	if (beat_count == 0)                                             //if no beat map is found:
	{
		USART_TransmitStr_P(PSTR("No beat map loaded."));
		mainMenu();
		return;
	}

	combo      = 0;                                                  //variable resets.
	multiplier = 1;
	game_over  = 0;
	stage      = RUNNING;                                            //game state changed.

	lcd_putcmd(LCD_CLEAR);
	lcd_putcmd(LCD_SET_CURSOR | FIRST_ROW);
	lcd_puts((uint8_t *)"    Ready...    ");
	_delay_ms(1000);

	lcd_putcmd(LCD_SET_CURSOR | FIRST_ROW);
	lcd_puts((uint8_t *)" 3... 2... 1..  ");
	_delay_ms(1000);

	lcd_putcmd(LCD_CLEAR);
	lcd_putcmd(LCD_SET_CURSOR | FIRST_ROW);
	lcd_puts((uint8_t *)"  GO! GO! GO!   ");

	for (uint8_t r = 0; r < 8; r++)                                  //game screen reset.
	{
		for (uint8_t c = 0; c < 3; c++)
		{
			game_screen[r][c] = 0;
		}
	}

	USART_TransmitStr_P(PSTR("START"));                              //tells song.py to start the game.

	cli();                                                           //disables global interrupts (clears)
	ms_counter = 0;                                                  //clears timing counter.
	sei();                                                           //re-enables global interrupts.

	uint16_t beat_index     = 0;                                     //initializes tracker variables.
	uint16_t spawn_index    = 0;
	uint32_t last_tick      = 0;
	uint32_t hit_timeline   = 0;
	uint32_t spawn_timeline = 0;

	//==============================================================
	// Game Running Logic
	//==============================================================
	while ((beat_index < beat_count || stage == WAITING) && !game_over)
	{	
		checkTilt();                                                 //checks for tilting.

		uint32_t current_ms;                                         //initializes variable for capturing timing.

		cli();                                                       
		current_ms = ms_counter;                                     //captures current time.
		sei();
		
		//==========================================================
		// Input Handling
		//==========================================================
		if (left_button_pressed || middle_button_pressed || right_button_pressed)
		{
			uint8_t pressed_lane = 0xFF;                             //initializes pressed_lane

			if (left_button_pressed)                                 //gives value for which lane was pressed.
			{
				pressed_lane        = 0;
				left_button_pressed = 0;
			}
			else if (middle_button_pressed)
			{
				pressed_lane          = 1;
				middle_button_pressed = 0;
			}
			else if (right_button_pressed)
			{
				pressed_lane         = 2;
				right_button_pressed = 0;
			}

			//======================================================
			// Hit Window
			//======================================================
			if (stage == WAITING)
			{
				if (pressed_lane == current_lane)                    //if right lane pressed:
				{
					measuring();                                     //measures the timing.
				}
				else                                                 //wrong lane hit.
				{
					combo           = 0;                             //resets values.
					multiplier      = 1;
					power_up_ready  = 0;
					power_up_active = 0;

					char score_buf[16];   
					char combo_buf[8];
					char mult_buf[8];

					ltoa(current_score, score_buf, 10);
					itoa(combo, combo_buf, 10);
					itoa(multiplier, mult_buf, 10);

					USART_TransmitNoAdd("HUD:");                      //sends data to game HUD
					USART_TransmitNoAdd(score_buf);
					USART_Transmit(',');
					USART_TransmitNoAdd(combo_buf);
					USART_Transmit(',');
					USART_TransmitNoAdd(mult_buf);
					USART_Transmit('\n');

					RED_ON;                                           //red LED to signal error
					BLUE_OFF;

					if (current_lane < 3)
					{
						game_screen[7][current_lane] = 0;             //clears beat
					}

					TCCR1B &= ~((1<<CS12) | (1<<CS10));               //turns off timer1
					TCNT1 = 0;                                        //resets timer1 value
					stage = RUNNING;
				}
			}
			
			/*
			//======================================================= //could not get it working successfully :(
			// Early / Spam Detection
			//======================================================= //but maybe one day??
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

		//==========================================================
		// Scroll Display
		//==========================================================
		if ((current_ms - last_tick) >= TICK_INTERVAL)               //TICK_INTERVAL is the refresh rate, essentially.
		{
			last_tick = current_ms;                                  //takes a snapshot of the current time.

			for (int8_t r = 7; r > 0; r--)                           //shifts the beats down a row.
			{
				game_screen[r][0] = game_screen[r-1][0];
				game_screen[r][1] = game_screen[r-1][1];
				game_screen[r][2] = game_screen[r-1][2];
			}

			game_screen[0][0] = 0;                                   //clears the top row.
			game_screen[0][1] = 0;
			game_screen[0][2] = 0;

			while (spawn_index < beat_count)                         //spawns the new notes:
			{
				uint16_t packed =
				beat_map[spawn_index].delta_and_lane;                //takes packed beat data.

				uint32_t delta =
				(packed & 0x3FFF);                                   //separates delta (time variance) value.

				uint8_t lane =
				(uint8_t)((packed >> 14) & 0x03);                    //separates lane value.

				uint32_t note_time =
				spawn_timeline + delta;                              //gets true time value.

				if ((current_ms + SCROLL_TIME) >= note_time)         //generates note, but 800ms before it actually arrives.
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

			displayGameScreen(game_screen);                          //refreshes game screen to display new values.
		}

		//==========================================================
		// Hit Window Trigger
		//==========================================================
		if (beat_index < beat_count)
		{
			uint16_t packed = beat_map[beat_index].delta_and_lane;
			uint32_t delta = (packed & 0x3FFF);
			uint8_t lane = (uint8_t)((packed >> 14) & 0x03);
			uint32_t target_time = hit_timeline + delta;             //exact value to be hit.
			
			uint16_t window = getTimingWindow();                     //gets timing window - based on difficulty.

			if (stage != WAITING && (current_ms + window) >= target_time)
			{
				current_lane = lane;                                 //allows for early hit detection.
				stage = WAITING;                                     //ready for user attempt
				
				beat_start_ms = target_time;                         //sets reference point.

				BLUE_ON;                                             //turns on blue LED.
				itoa(lane, game_conversion_buf, 10);                 //gets lane information to buffer.

				TCNT1 = 0;                                           //resets timer1
				TCCR1B |= (1<<CS12) | (1<<CS10);                     //activates timer1
				hit_timeline = target_time;                          //used for calculating next note.
				beat_index++;
			}
		}
		
		//==========================================================
		// Timing and Missed Beats
		//==========================================================
		if (stage == WAITING)
		{
			uint16_t ticks = TCNT1;                                  //tracks timer ticks.

			uint32_t elapsed_ms = ((uint32_t)ticks * 64UL) / 16000UL;//calculates time with ticks.

			uint16_t window = getTimingWindow();

			if (elapsed_ms > (uint32_t)(window * 2))                 //if entire window is missed:
			{
				TCCR1B &= ~((1<<CS12) | (1<<CS10));                  //stops timer1
				TCNT1 = 0;                                           //resets timer1

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
			}
		}
	}

	game_over = 1;
	
	gameOver();
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* ------------------------------------------------- Hit Scoring ---------------------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */
void measuring(void)
{
	uint32_t current_ms;

	cli();
	current_ms = ms_counter;
	sei();
	
	int32_t offset = (int32_t)current_ms - (int32_t)beat_start_ms;   //calculates time since last beat.

	if (offset < 0)                                                  //gets absolute value.
	{
		offset = -offset;
	}

	response_time = (uint32_t)offset;                                //saves user response time.
	
	uint16_t window = getTimingWindow();                             //checks against window.

	if (response_time <= window)                                     //if hit was in time:
	{
		uint8_t accuracy = 100 - (uint8_t)(response_time * 100 / window);
		current_score += (accuracy * multiplier);                    //scores based on accuracy.
		
		//==========================================================
		// Combos
		//==========================================================
		combo++;                                                     //increases the combo.
		
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
		
		//==========================================================
		// Send HUD Update to Python
		//==========================================================

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
		
		BLUE_OFF;
		RED_OFF;
		BLUE_ON;

		if (combo >= COMBO_THRESHOLD)                                //if high enough combo for power-up:
		{
			power_up_ready = 1;
			lcd_putcmd(LCD_SET_CURSOR | SECOND_ROW);
			lcd_puts((uint8_t *)"POWER-UP READY!");
		}
		
		if (current_lane < 3)
		{
			game_screen[7][current_lane] = 0;                        //clears hit beat.
		}
	}
	else                                                             //tapped, but missed:
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
	}

	TCCR1B &= ~((1<<CS12) | (1<<CS10));
	TCNT1 = 0;
	stage = RUNNING;
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* -------------------------------------------------- Game Over ----------------------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */
void gameOver (void)
{
	RED_OFF;
	BLUE_OFF;
	
	//==========================================================
	// High Score Logic
	//==========================================================
	if (current_score == 0)                                      //ensures no funny business with high score system.
	{
		high_score_achieved = 0;
		displayHighScores();
		mainMenu();
		return;
	}

	high_score_achieved = 0;

	for (uint8_t i = 0; i < 9; i++)                              //checks to see if high score achieved.
	{
		if (current_score > top_scores[i].score)
		{
			high_score_achieved = 1;
			break;
		}
	}

	displayGameOver(current_score, high_score_achieved);         //displays game over stuff.
	
	cli();
	waitForEnter();
	sei();

	if (high_score_achieved)                                     //if high score was achieved:
	{
		char initials[4];
		uint8_t idx = 0;

		USART_TransmitStr_P(PSTR("Enter initials: "));           //initiates request for initials.

		while (idx < 3)
		{
			char c = USART_Receive();                            //gets initials.

			if (c == '\r' || c == '\n')
			{
				continue;
			}

			initials[idx++] = c;
		}

		initials[3] = '\0';
		
		while (UCSR0A & (1 << RXC0))                             //clears buffer.
		{
			volatile char throwaway = UDR0;
		}

		for (int8_t i = 8; i >= 0; i--)                          //adds high score to leaderboard.
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
		
		saveHighScores();                                        //saves high scores to EEPROM.
	}

	current_score = 0;                                           //resets current score to 0 for safety.
	high_score_achieved = 0;

	displayHighScores();                                         //displays high scores.
	
	cli();
	waitForEnter();
	sei();
	
	mainMenu();
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* ----------------------------------------------- Sending Beats ---------------------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */
void transmitBeatMap (void)
{
	USART_TransmitStr_P(PSTR("BEATMAP_START"));                  //tells song.py the beat map is starting.

	uint32_t current_absolute_ms = 0;                            //
	char lane_buf[32];
	for (uint16_t i = 0; i < beat_count; i++)
	{
		uint16_t packed_data = beat_map[i].delta_and_lane;
		
		uint32_t delta = packed_data & 0x3FFF;
		uint8_t lane   = (uint8_t)((packed_data >> 14) & 0x03);
		
		current_absolute_ms += delta;
		
		ltoa((long)current_absolute_ms, game_conversion_buf, 10);
		itoa(lane, lane_buf, 10);
		USART_TransmitLine(game_conversion_buf, ',', lane_buf);
	}

	USART_TransmitStr_P(PSTR("BEATMAP_END"));
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* ----------------------------------------------- Recording Beats -------------------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */
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
			
			if (delta > 0x3FFF) delta = 0x3FFF;                    //ensures delta never goes too high for safety.

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

/* ------------------------------------------------------------------------------------------------------------------ */
/* --------------------------------------------- Receiving Beat Map ------------------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */
void receiveBeatMap (void)
{
	char    line[32];
	uint8_t idx = 0;
	
	beat_count = 0;
	uint32_t last_timestamp = 0;
	
	USART_TransmitStr_P(PSTR("READY_FOR_BEATMAP"));

	while (1)
	{
		char c = USART_Receive();

		if (c == '\r') continue;
		
		if (c == '\n')
		{
			line[idx] = '\0';

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

			if (lane > 2)
			{
				if (lane >= 1 && lane <= 3) lane--;
				else continue;
			}

			uint32_t delta = timestamp - last_timestamp;
			if (delta > 0x3FFF) delta = 0x3FFF;

			uint16_t packed_data = (delta & 0x3FFF) | ((uint16_t)(lane & 0x03) << 14);

			beat_map[beat_count].delta_and_lane = packed_data;
			beat_count++;

			last_timestamp = timestamp;
		}
		else
		{
			if (idx < 31) line[idx++] = c;
		}
	}

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

/* ------------------------------------------------------------------------------------------------------------------ */
/* --------------------------------------------- High Score Saving -------------------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */
void saveHighScores (void)
{
	eeprom_write_byte((uint8_t *)EEPROM_MAGIC_ADDR, EEPROM_MAGIC);
	eeprom_write_block(
	(const void *)top_scores,
	(void *)EEPROM_SCORES_ADDR,
	sizeof(top_scores)
	);
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* --------------------------------------------- High Score Loading ------------------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */
void loadHighScores (void)
{
	uint8_t magic = eeprom_read_byte((uint8_t *)EEPROM_MAGIC_ADDR);     //checks the data of the address of where the data should be stored.

	if (magic != EEPROM_MAGIC)                                          //if data isn't there or isn't valid:
	{
		for (uint8_t i = 0; i < 9; i++)                                 //initializes clean data.
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
	);                                                                  //loads high scores from EEPROM.
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* ----------------------------------------------- Enter to Continue ------------------------------------------------ */
/* ------------------------------------------------------------------------------------------------------------------ */
void waitForEnter(void)
{
	while (UCSR0A & (1 << RXC0))
	{
		volatile char throwaway = UDR0;
	}

	USART_Receive();

	while (UCSR0A & (1 << RXC0))
	{
		volatile char throwaway = UDR0;
	}
}
/* ------------------------------------------------------------------------------------------------------------------ */
/* ------------------------------------------------- Sensor Debugging ----------------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------ */
void sensorDebug(void)
{
	USART_TransmitStr_P(PSTR("SENSOR_DEBUG_START"));
	
	while (UCSR0A & (1 << RXC0))
	{
		volatile char throwaway = UDR0;
	}

	while (1)
	{
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
//======================================================================================================================
//                                                 End of File
//======================================================================================================================