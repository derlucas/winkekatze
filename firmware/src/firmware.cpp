#include <Arduino.h>
#include "InputDebounce.h"
#include <NeoPixelBus.h>

#define LED                     13
#define MOTOR_IN_1              9
#define MOTOR_IN_2              5
#define MOTOR_EN_A              2
#define MOTOR_EN_B              A4
#define PIN_CURRENT             A5
#define SWITCH_FRONT            7
#define SWITCH_BACK             6
#define TICK_DELAY_MS           30
#define BUTTON_DEBOUNCE_DELAY   10
#define SWITCH_TIMEOUT          4500 //enter error mode if no end switch pressed after this time
#define CENTERMOVE_TIME         1000 //time from last switch to reacht center position

#define DUTY_MIN                0
#define DUTY_MAX                100

#define PIN_BUTTON				10

//#define DEBUG


enum state {
    IDLE,
    BOOT_FIND_SWITCHES,
    RAMP_UP,
    MOVE,
    RAMP_DOWN,
	TIMEOUT,
	STOP
} cur_state = IDLE;

enum direction {
    FORWARD,
    REVERSE,
    BRAKE
} cur_direction = BRAKE;

direction next_direction = FORWARD;
int cur_duty = 0;
static InputDebounce input_front;
static InputDebounce input_back;
static InputDebounce input_button;
bool button_down=false;
static long tick_millis;
static long switch_millis;
#define STRIPPIN 8
#define NUM_LEDS 59
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(NUM_LEDS, STRIPPIN);
RgbColor red(255, 0, 0);
RgbColor green(0, 255, 0);
RgbColor blue(0, 0, 255);
RgbColor white(10);
RgbColor black(0);
unsigned long strip_tick_millis;
#define STRIP_TICK_DELAY_MS 100 //refresh delay for led strip
#define STRIPMODE_OFF 0
#define STRIPMODE_WHITE 1
#define STRIPMODE_RED 2
uint8_t stripmode = STRIPMODE_RED; //mode for led strip


int wavesleft; //to soft disable waving
bool wavecenterflag=false;

void setPwmFrequency(int pin, int divisor) {
  byte mode;
  if(pin == 5 || pin == 6 || pin == 9 || pin == 10) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 64: mode = 0x03; break;
      case 256: mode = 0x04; break;
      case 1024: mode = 0x05; break;
      default: return;
    }
    if(pin == 5 || pin == 6) {
      TCCR0B = TCCR0B & 0b11111000 | mode;
    } else {
      TCCR1B = TCCR1B & 0b11111000 | mode;
    }
  } else if(pin == 3 || pin == 11) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 32: mode = 0x03; break;
      case 64: mode = 0x04; break;
      case 128: mode = 0x05; break;
      case 256: mode = 0x06; break;
      case 1024: mode = 0x07; break;
      default: return;
    }
    TCCR2B = TCCR2B & 0b11111000 | mode;
  }
}

void setup() {

    Serial.begin(115200);
    Serial.println("\n\nhello\n\n");

    pinMode(LED, OUTPUT);

    pinMode(MOTOR_IN_1, OUTPUT);
    pinMode(MOTOR_IN_2, OUTPUT);
    // Set pin 9's PWM frequency to 3906 Hz (31250/8 = 3906)
    // Note that the base frequency for pins 3, 9, 10, and 11 is 31250 Hz
    //setPwmFrequency(MOTOR_IN_1, 8); //MOTOR_IN_1 is set to pin 9. default frequency=490

    // Set pin 6's PWM frequency to 62500 Hz (62500/1 = 62500)
    // Note that the base frequency for pins 5 and 6 is 62500 Hz
    //setPwmFrequency(MOTOR_IN_2, 8); //MOTOR_IN_2 is set to pin 5. default frequency=980

    //WARNING: Setting the frequency higher than default is not working (motor stops)

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

	input_button.setup(PIN_BUTTON, BUTTON_DEBOUNCE_DELAY,
		InputDebounce::PIM_INT_PULL_UP_RES, 0, InputDebounce::ST_NORMALLY_OPEN);



    tick_millis = millis();
    switch_millis = millis();

    // start slowly moving
    /*cur_duty = 10;
    cur_direction = FORWARD;
    cur_state = RAMP_UP;
    */

    // start non moving. expecting arm in center position
    cur_duty = 0;
    cur_direction = FORWARD;
    cur_state = STOP;
    wavesleft=0;
}


void loop() {
    long now_millis = millis();

    unsigned int front_on_time = input_front.process(now_millis);
	unsigned int back_on_time = input_back.process(now_millis);
	unsigned int button_on_time = input_button.process(now_millis);


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

		if (button_on_time) { //user button pressed

			
            if (!button_down) { //pressed
                button_down=true;
                wavesleft+=2; //start waving (forth and back)
#ifdef DEBUG
                Serial.println("user button");
#endif
            }
		}else{
            button_down=false;
        }

        if (button_on_time > 1000) { //button held down
#ifdef DEBUG
                Serial.println("user button hold. stop waving");
#endif
            wavesleft = 0;
        }

#ifdef DEBUG
        Serial.print("state: ");
        Serial.print(cur_state == IDLE ? "IDLE" : (cur_state == RAMP_UP ? "RAMP_UP" :
            (cur_state == RAMP_DOWN ? "RAMP_DOWN": (cur_state == MOVE ? "MOVE" :
            (cur_state == TIMEOUT ? "TIMEOUT" : (cur_state == STOP ? "STOP" : "UNKNOWN"))))));
        Serial.print(" duty: ");
        Serial.print(cur_duty);
        Serial.print(" direction: ");
        Serial.print(cur_direction == FORWARD ? "FORWARD" : (cur_direction == REVERSE ? "REVERSE" : "BRAKE"));
        Serial.print(" next_direction: ");
        Serial.print(next_direction == FORWARD ? "FORWARD" : (next_direction == REVERSE ? "REVERSE" : "BRAKE"));
        Serial.print(" wavesleft: ");
        Serial.print(wavesleft);
        Serial.println("");
#endif

        if(cur_state == IDLE) {
            if(next_direction != cur_direction) {
                cur_direction = next_direction;
                cur_state = RAMP_UP;
            }
        } else if(cur_state == RAMP_UP) {
            if (cur_duty < DUTY_MAX) {
                cur_duty += 5;
            } else {
                cur_duty = DUTY_MAX;
                cur_state = MOVE;
            }
        } else if(cur_state == MOVE) {
            if(now_millis - switch_millis > CENTERMOVE_TIME) { //time from last switch to reach center position
                if (!wavecenterflag) {
                    wavecenterflag=true; //only do this if once every wave
                    wavesleft -=1;
    #ifdef DEBUG
                    Serial.println("Wave Centerpos");
    #endif
                    
                    if (wavesleft<=0) { //needz stop wavin'
                        cur_state = STOP;
    #ifdef DEBUG
                    Serial.println("No waves left. Stopping.");
    #endif
                    }
                }
            }else{
                wavecenterflag=false; //reset flag
            }
            
        } else if(cur_state == RAMP_DOWN) {
            if(cur_duty > DUTY_MIN) {
                cur_duty-=10;
            } else {
                cur_duty=DUTY_MIN;
                cur_state = IDLE;
                next_direction = cur_direction == FORWARD ? REVERSE : FORWARD;
                cur_direction = BRAKE;
            }
        } else if(cur_state == STOP) {
            if(cur_duty > DUTY_MIN) {
                cur_duty-=10; //stop slowly
            } else if (cur_direction!=BRAKE){
                cur_duty=0;
                next_direction = cur_direction; //save last direction before braking
                cur_direction = BRAKE;
            }
            if (wavesleft>0) { //can start waving again
#ifdef DEBUG
                Serial.println("Start waving.");
#endif
                if (next_direction == BRAKE) { //in case it hasnt moved before
                    next_direction = FORWARD;
                }
                cur_direction = next_direction; //start in the same direction
                
                cur_state = RAMP_UP;
                switch_millis = now_millis - CENTERMOVE_TIME; //for timeout check. subtract time to move to center
                wavecenterflag=true; //do not stop at center position for this cycle
            }
		}


        if(cur_direction == BRAKE) {
            analogWrite(MOTOR_IN_1, DUTY_MIN);
            analogWrite(MOTOR_IN_2, DUTY_MIN);
        } else if(cur_direction == FORWARD) {
            analogWrite(MOTOR_IN_1, (byte)cur_duty);
            analogWrite(MOTOR_IN_2, DUTY_MIN);
        } else if(cur_direction == REVERSE) {
            analogWrite(MOTOR_IN_1, DUTY_MIN);
            analogWrite(MOTOR_IN_2, (byte)cur_duty);
        }




        // check if switch has timedout
        if (cur_state != STOP) 
        {
            if(now_millis - switch_millis > SWITCH_TIMEOUT) {
                cur_state = TIMEOUT;
                cur_duty = DUTY_MIN;
                cur_direction = BRAKE;
                next_direction = BRAKE;
            }
        }




        tick_millis = now_millis;
    }

    if(now_millis - strip_tick_millis > STRIP_TICK_DELAY_MS) { //led strip update
        
        if (cur_state != TIMEOUT) { //no error
            switch (stripmode) {
                case STRIPMODE_OFF:
                for (uint16_t i=3;i<NUM_LEDS;i++) {
                    strip.SetPixelColor(i, black);
                }
                case STRIPMODE_WHITE:
                strip.SetPixelColor(21, white);
                strip.SetPixelColor(25, white);
                strip.SetPixelColor(29, white);
                strip.SetPixelColor(33, white);
                break;
                case STRIPMODE_RED:
                #define REDMINCOLOR 10
                #define REDMAXCOLOR 200
                for (uint16_t i=3;i<NUM_LEDS;i++) {
                    uint8_t _red=REDMINCOLOR+(  (sin(now_millis/1000.0+i/10.0)+1)/2.0*(REDMAXCOLOR-REDMINCOLOR)   );
                    strip.SetPixelColor(i, RgbColor(_red,0,0));
                }
                break;
            }
        }


        if(cur_state == TIMEOUT) {
            strip.SetPixelColor(1, RgbColor(10,0,0));
            strip.SetPixelColor(21, red);
            strip.SetPixelColor(25, red);
            strip.SetPixelColor(29, red);
            strip.SetPixelColor(33, red);
        }else if(cur_state == RAMP_DOWN) {
            strip.SetPixelColor(1, RgbColor(10,0,0));
        }else if(cur_state == MOVE) {
            strip.SetPixelColor(1, RgbColor(0, 10, 0));
        }else if(cur_state == RAMP_UP) {
            strip.SetPixelColor(1, RgbColor(0, 0, 10));
        }if(cur_state == IDLE) {
            strip.SetPixelColor(1, RgbColor(0,0,0));
        }

        if(cur_direction == BRAKE) {
            strip.SetPixelColor(2, RgbColor(10,0,0));
        } else if(cur_direction == FORWARD) {
            strip.SetPixelColor(2, RgbColor(0,10,0));
        } else if(cur_direction == REVERSE) {
            strip.SetPixelColor(2, RgbColor(0,0,10));
        }

        

        strip.Show();

        strip_tick_millis=now_millis;
    }

}

