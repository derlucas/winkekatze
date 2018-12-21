#include <Arduino.h>
#include "InputDebounce.h"

#define LED     13
#define MOTOR_IN_1    6
#define MOTOR_IN_2    5
#define MOTOR_EN_A    3
#define MOTOR_EN_B    A5
#define SWITCH_FRONT    7
#define SWITCH_BACK     8
#define TICK_DELAY_MS   20
#define BUTTON_DEBOUNCE_DELAY 5 // [ms]

#define DUTY_MIN    0
#define DUTY_MAX    50      //TODO: figure out maximum sensible speed


enum state {
    IDLE,
    RAMP_UP,
    MOVE,
    RAMP_DOWN,
} cur_state = IDLE;

enum direction {
    FORWARD,
    REVERSE,
    BRAKE
} cur_direction = BRAKE;
byte cur_duty = 0;
static InputDebounce input_front;
static InputDebounce input_back;


void setup() {

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

    // start slowly moving

}


void loop() {

    static long tick_millis = millis();
    long now_millis = millis();

    unsigned int front_on_time = input_front.process(now);
    unsigned int back_on_time = input_back.process(now);


    if(front_on_time) {
        if(cur_state == MOVE && cur_direction == FORWARD) {
            cur_state = RAMP_DOWN;
        }
    }
    if(back_on_time) {
        if(cur_state == MOVE && cur_direction == REVERSE) {
            cur_state == RAMP_DOWN;
        }
    }


    if(now_millis - tick_millis > TICK_DELAY_MS) {



        if(cur_state == RAMP_UP) {
            if(cur_duty < DUTY_MAX) {
                cur_duty++;
            } else {
                cur_state = MOVE;
            }
        } else if(cur_state == RAMP_DOWN) {
            if(cur_duty > DUTY_MIN) {
                cur_duty--;
            } else {
                cur_state = MOVE;
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

        tick_millis = now_millis;
    }

}
