#include <Arduino.h>

#define IN_1A   5
#define IN_2A   4
#define EN_A    2
#define EN_B    A4

void setup() {

    pinMode(13, OUTPUT);

    pinMode(IN_1A, OUTPUT);
    pinMode(IN_2A, OUTPUT);
    pinMode(EN_A, OUTPUT);
    pinMode(EN_B, OUTPUT);

    digitalWrite(IN_1A, LOW);
    digitalWrite(IN_2A, LOW);
    digitalWrite(EN_A, HIGH);
    digitalWrite(EN_B, HIGH);


}


void loop() {


    digitalWrite(IN_1A, HIGH);
    digitalWrite(IN_2A, LOW);

    digitalWrite(13, HIGH);

    delay(200);

    digitalWrite(IN_1A, LOW);
    digitalWrite(IN_2A, HIGH);

    digitalWrite(13, LOW);
    delay(200);


}
