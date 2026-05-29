//======================================================================================================================
// Title:       USART.c
// Author:      nathan ramos
// Created:     5/15/2026
// Description: USART module for serial communication.
//======================================================================================================================
//======================================================================================================================
//                                                 System Settings
//======================================================================================================================
#define F_CPU                  16000000UL
//======================================================================================================================
//                                                     Imports
//======================================================================================================================
#include "USART.h"
//======================================================================================================================
//                                                    Functions
//======================================================================================================================
void USART_Init (unsigned int ubrr)
{
    UBRR0H = (unsigned char)(ubrr>>8);        //stores the MSB here.
    UBRR0L = (unsigned char)ubrr;             //stores the LSB here.
	UCSR0A |= (1<<U2X0);                      //double speed mode. needed for the higher baud rate.
    UCSR0B |= (1<<RXEN0)|(1<<TXEN0);          //enables RX and TX.
    UCSR0C |= (0<<USBS0)|(3<<UCSZ00);         //8 bit data - 1 stop bit.
}

void USART_Transmit (unsigned char data)
{
    while (!(UCSR0A & (1<<UDRE0)));           //waits for buffer to empty.
    UDR0 = data;                              //stores the data.
}

void USART_TransmitStr (const char *str)
{
    while (*str)                              //while string still there:
    {
        USART_Transmit(*str);                 //transmits one char at a time.
        str++;                                //increments string index.
    }
    USART_Transmit('\r');                     //carriage return for readability.
    USART_Transmit('\n');                     //newline for readability.
}

void USART_TransmitStr_P (const char *str)
{
    char c;
    while ((c = pgm_read_byte(str++)))        //reads from flash one byte at a time.
    {
        USART_Transmit(c);                    //transmits the byte.
    }
    USART_Transmit('\r');                     //carriage return for readability.
    USART_Transmit('\n');                     //newline for readability.
}

void USART_TransmitNoAdd(const char *s)       //for transmitting a string without \r\n
{
	while (*s)
	{
		USART_Transmit(*s++);
	}
}

unsigned char USART_Receive (void)
{
    while (!(UCSR0A & (1<<RXC0)));            //waits for data to arrive.
    return UDR0;                              //returns data from register.
}

void USART_TransmitLine (const char *str1, char separator, const char *str2)
{
	while (*str1) USART_Transmit(*str1++);
	USART_Transmit(separator);
	while (*str2) USART_Transmit(*str2++);
	USART_Transmit('\r');
	USART_Transmit('\n');
}
//======================================================================================================================
//                                                    End of File
//======================================================================================================================