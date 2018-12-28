#include <Arduino.h>
#include "InputDebounce.h"
#include <NeoPixelBus.h>

#define LED                     13
#define MOTOR_IN_1              9
#define MOTOR_IN_2              5
#define MOTOR_EN_A              2
#define MOTOR_EN_B              A4
#define PIN_CURRENT             A5
#define SWITCH_FRONT            6
#define SWITCH_BACK             7
#define TICK_DELAY_MS           30
#define BUTTON_DEBOUNCE_DELAY   10
#define SWITCH_TIMEOUT          4500

#define DUTY_MIN                0
#define DUTY_MAX                100

#define DEBUG


enum state {
    IDLE,
    BOOT_FIND_SWITCHES,
    RAMP_UP,
    MOVE,
    RAMP_DOWN,
    TIMEOUT
} cur_state = IDLE;

enum direction {
    FORWARD,
    REVERSE,
    BRAKE
} cur_direction = BRAKE;

direction next_direction = FORWARD;
byte cur_duty = 0;
static InputDebounce input_front;
static InputDebounce input_back;
static long tick_millis;
static long switch_millis;
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(50, 8);
RgbColor red(255, 0, 0);
RgbColor green(0, 255, 0);
RgbColor blue(0, 0, 255);
RgbColor white(10);


void setup() {

    Serial.begin(115200);
    Serial.println("\n\nhello\n\n");

    pinMode(LED, OUTPUT);

    pinMode(MOTOR_IN_1, OUTPUT);
    pinMode(MOTOR_IN_2, OUTPUT);
    pinMode(MOTOR_EN_A, OUTPUT);
    pinMode(MOTOR_EN_B, OUTPUT);

    input_front.setup(SWITCH_FRONT, BUTTON_DEBOUNCE_DELAY,
            InputDebounce::PIM_INT_PULL_UP_RES, 0, InputDebounce::ST_NORMALLY_CLOSED);
    input_back.setup(SWITCH_BACK, BUTTON_DEBOUNCE_DELAY,
            InputDebounce::PIM_INT_PULL_UP_RES, 0, InputDebounce::ST_NORMALLY_CLOSED);

    digitalWrite(MOTOR_IN_1, LOW);
    digitalWrite(MOTOR_IN_2, LOW);
    digitalWrite(MOTOR_EN_A, HIGH);
    digitalWrite(MOTOR_EN_B, HIGH);


    tick_millis = millis();
    switch_millis = millis();

    // start slowly moving
    cur_duty = 10;
    cur_direction = FORWARD;
    cur_state = RAMP_UP;


    strip.SetPixelColor(21, white);
    strip.SetPixelColor(25, white);
    strip.SetPixelColor(29, white);
    strip.SetPixelColor(33, white);
    strip.Show();


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
            (cur_state == TIMEOUT ? "TIMEOUT" : "UNKNOWN")))));
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
            strip.SetPixelColor(1, RgbColor(0,0,0));
            strip.Show();
        } else if(cur_state == RAMP_UP) {
            if (cur_duty < DUTY_MAX) {
                cur_duty += 5;
            } else {
                cur_state = MOVE;
            }
            strip.SetPixelColor(1, RgbColor(0, 0, 10));
            strip.Show();
        } else if(cur_state == MOVE) {
            strip.SetPixelColor(1, RgbColor(0, 10, 0));
            strip.Show();
        } else if(cur_state == RAMP_DOWN) {
            if(cur_duty > DUTY_MIN) {
                cur_duty-=10;
            } else {
                cur_state = IDLE;
                next_direction = cur_direction == FORWARD ? REVERSE : FORWARD;
                cur_direction = BRAKE;
            }
            strip.SetPixelColor(1, RgbColor(10,0,0));
            strip.Show();
        } else if(cur_state == TIMEOUT) {
            strip.SetPixelColor(1, RgbColor(10,0,0));
            strip.SetPixelColor(21, red);
            strip.SetPixelColor(25, red);
            strip.SetPixelColor(29, red);
            strip.SetPixelColor(33, red);
            strip.Show();
        }


        if(cur_direction == BRAKE) {
            analogWrite(MOTOR_IN_1, DUTY_MIN);
            analogWrite(MOTOR_IN_2, DUTY_MIN);
            strip.SetPixelColor(2, RgbColor(10,0,0));
            strip.Show();
        } else if(cur_direction == FORWARD) {
            analogWrite(MOTOR_IN_1, cur_duty);
            analogWrite(MOTOR_IN_2, DUTY_MIN);
            strip.SetPixelColor(2, RgbColor(0,10,0));
            strip.Show();
        } else if(cur_direction == REVERSE) {
            analogWrite(MOTOR_IN_1, DUTY_MIN);
            analogWrite(MOTOR_IN_2, cur_duty);
            strip.SetPixelColor(2, RgbColor(0,0,10));
            strip.Show();
        }




        // check if switch has timedout
        if(now_millis - switch_millis > SWITCH_TIMEOUT) {
            cur_state = TIMEOUT;
            cur_duty = DUTY_MIN;
            cur_direction = BRAKE;
            next_direction = BRAKE;
        }




        tick_millis = now_millis;
    }

}
