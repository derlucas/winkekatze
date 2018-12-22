#include <Arduino.h>
#include "InputDebounce.h"

#define LED                     13
#define MOTOR_IN_1              6
#define MOTOR_IN_2              5
#define MOTOR_EN_A              3
#define MOTOR_EN_B              A5
#define PIN_CURRENT             A1
#define SWITCH_FRONT            7
#define SWITCH_BACK             8
#define TICK_DELAY_MS           200
#define BUTTON_DEBOUNCE_DELAY   50
#define SWITCH_TIMEOUT          10000

#define DUTY_MIN                0
#define DUTY_MAX                12      //TODO: figure out maximum sensible speed
#define CURRENT_MAX             5000    // in mA

#define DEBUG


enum state {
    IDLE,
    RAMP_UP,
    MOVE,
    RAMP_DOWN,
    TIMEOUT,
    OVERCURRENT,
} cur_state = IDLE;

enum direction {
    FORWARD,
    REVERSE,
    BRAKE
} cur_direction = BRAKE;

direction next_direction = FORWARD;
byte cur_duty = 0;
int16_t cur_current = 0; // in mA  10A = 10000
static InputDebounce input_front;
static InputDebounce input_back;
static long tick_millis;
static long switch_millis;

int16_t sample_current_sensor() {
    int16_t val;
    long temp = 0;

    for (int i = 0; i < 10; ++i) {
        temp += analogRead(PIN_CURRENT);
        delay(1);
    }

    temp /= 10;

    // Allegro ACS712 ELC 30A = 66mV/A or 0,066mV per mA
    //val = temp * (5.0 / 1023.0);

    return val;
}

void setup() {

    Serial.begin(115200);
    Serial.println("\n\nhello\n\n");

    pinMode(LED, OUTPUT);

    pinMode(MOTOR_IN_1, OUTPUT);
    pinMode(MOTOR_IN_2, OUTPUT);
    pinMode(MOTOR_EN_A, OUTPUT);
    pinMode(MOTOR_EN_B, OUTPUT);

    input_front.setup(SWITCH_FRONT, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES);
    input_back.setup(SWITCH_BACK, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES);

    digitalWrite(MOTOR_IN_1, LOW);
    digitalWrite(MOTOR_IN_2, LOW);
    digitalWrite(MOTOR_EN_A, HIGH);
    digitalWrite(MOTOR_EN_B, HIGH);


    tick_millis = millis();
    switch_millis = millis();

    // start slowly moving
    cur_duty = 5;
    cur_direction = FORWARD;
    cur_state = MOVE;

}


void loop() {


    long now_millis = millis();

    unsigned int front_on_time = input_front.process(now_millis);
    unsigned int back_on_time = input_back.process(now_millis);


    if(now_millis - tick_millis > TICK_DELAY_MS) {

        if(front_on_time) {
#ifdef DEBUG
            Serial.println("front switch");
#endif

            if((cur_state == MOVE || cur_state == RAMP_UP) && cur_direction == FORWARD) {
                cur_state = RAMP_DOWN;
            }
            switch_millis = now_millis;
        }
        if(back_on_time) {
#ifdef DEBUG
            Serial.println("back switch");
#endif

            if((cur_state == MOVE || cur_state == RAMP_UP) && cur_direction == REVERSE) {
                cur_state = RAMP_DOWN;
            }
            switch_millis = now_millis;
        }

#ifdef DEBUG
        Serial.print("state: ");
        Serial.print(cur_state == IDLE ? "IDLE" : (cur_state == RAMP_UP ? "RAMP_UP" :
            (cur_state == RAMP_DOWN ? "RAMP_DOWN": (cur_state == MOVE ? "MOVE" :
            (cur_state == TIMEOUT ? "TIMEOUT" : (cur_state == OVERCURRENT ? "OVERCURRENT" : "UNKNOWN"))))));
        Serial.print(" duty: ");
        Serial.print(cur_duty);
        Serial.print(" direction: ");
        Serial.print(cur_direction == FORWARD ? "FORWARD" : (cur_direction == REVERSE ? "REVERSE" : "BRAKE"));
        Serial.println("");
#endif

        if(cur_state == IDLE) {
            if(next_direction != cur_direction) {
                cur_direction = next_direction;
                cur_state = RAMP_UP;
            }
        } else if(cur_state == RAMP_UP) {
            if(cur_duty < DUTY_MAX) {
                cur_duty++;
            } else {
                cur_state = MOVE;
            }
        } else if(cur_state == RAMP_DOWN) {
            if(cur_duty > DUTY_MIN) {
                cur_duty--;
            } else {
                cur_state = IDLE;
                next_direction = cur_direction == FORWARD ? REVERSE : FORWARD;
                cur_direction == BRAKE;
            }
        }


        if(cur_direction == BRAKE) {
            analogWrite(MOTOR_IN_1, DUTY_MIN);
            analogWrite(MOTOR_IN_2, DUTY_MIN);
        } else if(cur_direction == FORWARD) {
            analogWrite(MOTOR_IN_1, cur_duty);
            analogWrite(MOTOR_IN_2, DUTY_MIN);
        } else if(cur_direction == REVERSE) {
            analogWrite(MOTOR_IN_1, DUTY_MIN);
            analogWrite(MOTOR_IN_2, cur_duty);
        }


        // check if switch has timedout
        if(now_millis - switch_millis > SWITCH_TIMEOUT) {
            Serial.println("Error, Switch timeout");
            cur_state = IDLE;
            cur_duty = DUTY_MIN;
            cur_direction = BRAKE;
            next_direction = BRAKE;
        }


        tick_millis = now_millis;
    }

}
