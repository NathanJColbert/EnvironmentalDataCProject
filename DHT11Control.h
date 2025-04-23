#ifndef DHT11_CONTROL_H
#define DHT11_CONTROL_H

#include <wiringPi.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define DHT11_MAX_TIME 85
int DHT11PIN;

bool dht11_init(int pin) {
    DHT11PIN = pin;
    return wiringPiSetup() != -1;
}

bool checksum_dht11(uint8_t shiftCount, int data[5]) {
    return (shiftCount >= 40) && (data[4] == ( (data[0] + data[1] + data[2] + data[3]) & 0xFF ));
}
bool read_dht11_dat(int data[5]) {
    uint8_t laststate = HIGH;
    uint8_t counter = 0;
    uint8_t j = 0, i;
    data[0] = data[1] = data[2] = data[3] = data[4] = 0;
    pinMode(DHT11PIN, OUTPUT);
    digitalWrite(DHT11PIN, LOW);
    delay(18);
    digitalWrite(DHT11PIN, HIGH);
    delayMicroseconds(40);
    pinMode(DHT11PIN, INPUT);
    for (i = 0; i < DHT11_MAX_TIME; i++) {
        counter = 0;
        while(digitalRead(DHT11PIN) == laststate) {
            counter++;
            delayMicroseconds(1);
            if (counter == 255) break;
        }
        laststate = digitalRead(DHT11PIN);
        if (counter == 255) break;
        if ((i >= 4) && (i % 2 == 0)) {
            data[j/8] <<= 1;
            if (counter > 25) data[j/8] |= 1;
            j++;
        }
    }
    if (!checksum_dht11(j, data)) return false;
    return true;
}

int digits(int value) {
    int i = 0;
    if (value == 0) return 1;
    while (value > 0) {
        value /= 10;
        i++;
    }
    return i;
}
void convertData(int data[5], double* value1, double* value2) {
    *value1 = data[0];
    *value1 += data[1] / pow(10, digits(data[1]));
    *value2 = data[2];
    *value2 += data[3] / pow(10, digits(data[3]));
}

#endif
