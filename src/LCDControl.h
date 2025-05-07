#ifndef LDC_CONTROL_H
#define LDC_CONTROL_H

#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>

int LCDAddr;
int BLEN = 1;
int fd;

void write_word(int data) {
    int temp = data;
    if (BLEN == 1) temp |= 0x08;
    else temp &= 0xF7;
    wiringPiI2CWrite(fd, temp);
}

void send_command(int comm) {
    int buf;
    // Send bit7-4 firstly
    buf = comm & 0xF0;
    buf |= 0x04; // RS = 0, RW = 0, EN = 1
    write_word(buf);
    delay(2);
    buf &= 0xFB; // Make EN = 0
    write_word(buf);

    // Send bit3-0 secondly
    buf = (comm & 0x0F) << 4;
    buf |= 0x04; // RS = 0, RW = 0, EN = 1
    write_word(buf);
    delay(2);
    buf &= 0xFB; // Make EN = 0
    write_word(buf);
}

void send_data(int data) {
    int buf;
    // Send bit7-4 firstly
    buf = data & 0xF0;
    buf |= 0x05; // RS = 1, RW = 0, EN = 1
    write_word(buf);
    delay(2);
    buf &= 0xFB; // Make EN = 0
    write_word(buf);

    // Send bit3-0 secondly
    buf = (data & 0x0F) << 4;
    buf |= 0x05; // RS = 1, RW = 0, EN = 1
    write_word(buf);
    delay(2);
    buf &= 0xFB; // Make EN = 0
    write_word(buf);
}

int lcd_init(int address) {
    LCDAddr = address;
    fd = wiringPiI2CSetup(LCDAddr);
    if (fd == -1)
        return 0;
    
    send_command(0x33); // Must initialize to 8-line mode at first
    delay(5);
    send_command(0x32); // Then initialize to 4-line mode
    delay(5);
    send_command(0x28); // 2 Lines & 5*7 dots
    delay(5);
    send_command(0x0C); // Enable display without cursor
    delay(5);
    send_command(0x01); // Clear Screen
    wiringPiI2CWrite(fd, 0x08);
    return 1;
}

void clear() { send_command(0x01); /*clear Screen*/ }

void writeRegister(int x, int y, char data[]) {
    int addr, i;
    int tmp;
    if (x < 0) x = 0;
    if (x > 15) x = 15;
    if (y < 0) y = 0;
    if (y > 1) y = 1;

    // Move cursor
    addr = 0x80 + 0x40 * y + x;
    send_command(addr);

    tmp = strlen(data);
    for (i = 0; i < tmp; i++)
        send_data(data[i]);
}

char* getDoubleString(double input, unsigned int length) {
	if (length == 0) return NULL;
	unsigned int decimalPlace = length + 1;
	int divisor = pow(10, length);
	int outsideBounds = 0;
	
	if (input < 1) {
		decimalPlace = 1;
		input *= divisor;
	}
	else if (input > divisor * 10) {
		outsideBounds = 1;
		while (input > divisor * 10) {
			input /= 10;
		}
	}
	else while (input < divisor) {
		input *= 10; decimalPlace--;
	}
	
	char *output = (char*)malloc((length + 2) * sizeof(char));
	
	int converted = (int)input;
	unsigned int i = 0;
	while (divisor > 0) {
		if (i >= length) break;
		if (i == decimalPlace) output[i++] = '.';
		
		int digit = converted / divisor;
		output[i++] = '0' + digit;
		
		converted = converted % divisor;
		divisor /= 10;
	}
	if (outsideBounds) output[i - 1] = 'E';
	output[i] = '\0';
	
	return output;
}

void writeData(double temperature, double humidity, time_t time) {
    writeRegister(0, 0, "temp humi time");
    
    char* temp = getDoubleString(temperature, 4);
    char* hum = getDoubleString(humidity, 4);
    writeRegister(0, 1, temp);
    writeRegister(5, 1, hum);

	char timeString[6];
    strftime(timeString, sizeof(timeString), "%H:%M", localtime(&time));
    writeRegister(10, 1, timeString);
}

#endif
