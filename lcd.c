/***************************************************************************
 * LCD Source File
 *
 * Created: 2022-04-16
 * Updated: 2022-04-16
 * Author: GBO
 * updated by: nlantau
 **************************************************************************/

/***************************************************************************
 *
 **************************************************************************/

/***** Include section ****************************************************/
#include "lcd.h"

/***** Functions **********************************************************/
void lcd_init_pinout(void) {
    /* 4 data lines */
    LCD_D7_DDR |= (1 << LCD_D7_BIT);
    LCD_D6_DDR |= (1 << LCD_D6_BIT);
    LCD_D5_DDR |= (1 << LCD_D5_BIT);
    LCD_D4_DDR |= (1 << LCD_D4_BIT);

    /* E line */
    LCD_E_DDR |= (1 << LCD_E_BIT);

    /* RS line */
    LCD_RS_DDR |= (1 << LCD_RS_BIT);

} /* End init_lcd_pinout() */

void lcd_init(void) {
    lcd_init_pinout();

    /* Initial 40 ms delay */
    _delay_ms(100);

    /* Select the Instruction Register (RS low) */
    LCD_RS_PORT &= ~(1 << LCD_RS_BIT);

    /* Make sure E is initially low */
    LCD_E_PORT &= ~(1 << LCD_E_BIT);

    /* First part of reset sequence */
    lcd_write(LCD_FUNCTION_RESET);

    /* 4.1 ms delay (min) */
    _delay_ms(10);

    /* Second part of reset sequence */
    lcd_write(LCD_FUNCTION_RESET);

    /* 100 us delay (min) */
    _delay_us(200);

    /* Thirs part of reset sequence */
    lcd_write(LCD_FUNCTION_RESET);

    /* This delay is omitted in the data sheet */
    _delay_us(200);

    /* Set 4-bit mode */
    lcd_write(LCD_FUNCTION_SET_4BIT);

    /* 40 us delay (min) */
    _delay_us(80);

    /* Set mode, lines, and font */
    lcd_putcmd(LCD_FUNCTION_SET_4BIT);

    /* 40 us delay (min) */
    _delay_us(80);

    /* Turn display OFF */
    lcd_putcmd(LCD_DISPLAY_OFF);

    /* 40 us delay (min) */
    _delay_us(80);

    /* Clear display RAM */
    lcd_putcmd(LCD_CLEAR);

    /* 1.64 ms delay (min) */
    _delay_ms(4);

    /* Set desired shift characteristics */
    lcd_putcmd(LCD_ENTRY_MODE);

    /* 40 us delay (min) */
    _delay_us(80);

    /* Turn the display ON */
    lcd_putcmd(LCD_DISPLAY_ON);

    /* 40 us delay (min) */
    _delay_us(80);
}

void lcd_puts(uint8_t *s) {
    /* Character counter */
    // volatile uint8_t i = 0;
    while (*s) {
        // lcd_putc(s[i++]);
        lcd_putc(*s++);
        _delay_us(80);  // 40 uS delay (min)
    }

} /* End lcd_puts()*/

void lcd_putc(uint8_t data) {
    /* Select the Data Register (RS high) */
    LCD_RS_PORT |= (1 << LCD_RS_BIT);

    /* Make sure E is initially low */
    LCD_E_PORT &= ~(1 << LCD_E_BIT);

    /* Write the upper 4-bits of the data */
    lcd_write(data);

    /* Write the lower 4-bits of the data */
    lcd_write(data << 4);

} /* End lcd_putc() */

void lcd_putcmd(uint8_t instr) {
    /* Select the Instruction Register (RS low) */
    LCD_RS_PORT &= ~(1 << LCD_RS_BIT);

    /* Make sure E is initially low */
    LCD_E_PORT &= ~(1 << LCD_E_BIT);

    /* Write the upper 4-bits of the data */
    lcd_write(instr);

    /* Write the lower 4-bits of the data */
    lcd_write(instr << 4);

    _delay_us(80);

} /* End lcd_putcmd() */

void lcd_write(uint8_t b) {
    /* Assume that data is '0' */
    LCD_D7_PORT &= ~(1 << LCD_D7_BIT);

    /* Make data '1' if necessary */
    if (b & 1 << 7) LCD_D7_PORT |= (1 << LCD_D7_BIT);

    /* Repeat for each data bit */
    LCD_D6_PORT &= ~(1 << LCD_D6_BIT);

    if (b & 1 << 6) LCD_D6_PORT |= (1 << LCD_D6_BIT);

    LCD_D5_PORT &= ~(1 << LCD_D5_BIT);

    if (b & 1 << 5) LCD_D5_PORT |= (1 << LCD_D5_BIT);

    LCD_D4_PORT &= ~(1 << LCD_D4_BIT);

    if (b & 1 << 4) LCD_D4_PORT |= (1 << LCD_D4_BIT);

    /* Enable pin high */
    LCD_E_PORT |= (1 << LCD_E_BIT);

    /* Implement 'Data set-up time' (80 ns)
     * and 'Enable pulse width' (230 ns)
     */
    _delay_us(1);
    /* Enable pin low */
    LCD_E_PORT &= ~(1 << LCD_E_BIT);

    /* Implement 'Data hold time' (10 ns) and
     * 'Enable cycle time' (500 ns)
     */
    _delay_us(1);

} /* End lcd_write() */

void lcd_clear_row(uint8_t row) {
    uint8_t clear_row[] = "                ";
    lcd_putcmd(LCD_SET_CURSOR | row);
    lcd_puts(clear_row);

} /* lcd_clear_row() */

/* End lcd.c */
