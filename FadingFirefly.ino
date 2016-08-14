
#define USE_12BIT_PWM 1
#if USE_12BIT_PWM
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
#define PWM_MAX 4095

#endif

///Analog pins for various boards
#if defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_PROTRINKET3) || defined(ARDUINO_AVR_PROTRINKET5)
//analog out pins for Uno, Trinket pro
const int AnalogPinCount = 6;
int analogOutPins[] = {
  3,
  5,
  6,
  9,
  10,
  11,
};

#elif defined(ARDUINO_AVR_FEATHER32U4)
//analog out pins for Feather 32U4
const int AnalogPinCount = 7;
int analogOutPins[] = {
  5,
  6,
  9,
  10,
  11,
  12,
  13,
};

#elif defined(ARDUINO_SAMD_FEATHER_M0)
//TODO: the analog pins on the Feather M0 aren't worked as documented
//analog out pins for Feather M0
const int AnalogPinCount = 4;
int analogOutPins[] = {
//  0,
//  1, digital blink ^
  5,
  6,
//  9,
//  10,
  11,
//  12,
//  13,
//  A0,
  A1,
//  A2, nothing ^
//  A3, digital blink v
//  A4,
//  A5,
//  20,
//  21,
//  22,
//  23,
//  24,
};

#elif defined(ARDUINO_AVR_TRINKET3) || defined(ARDUINO_AVR_TRINKET5)
//analog out pins for Trinket
const int AnalogPinCount = 3;
int analogOutPins[] = {
  0,
  1,
  4,
};
#endif

extern const uint8_t PROGMEM pwm_gamma[];

#define ANALOG_MAX 255
#define MSEC_PER_SEC 1000
#define RANDOM_DELAY_TIME random(2 * MSEC_PER_SEC, 4 * MSEC_PER_SEC)

struct Keyframe {
  int time; //time offset between this keyframe and the previous one, in milliseconds
  int value; //value of the keyframe
};
typedef struct Keyframe Keyframe;

Keyframe keyframe(float timeInSec, int value) {
  Keyframe k;
  k.time = timeInSec * MSEC_PER_SEC;
  k.value = value;
  return k;
}

struct Timeline {
  int duration;
  int numFrames;
  Keyframe *frames;  
};
typedef struct Timeline Timeline;

namespace Pattern {
  
  const float RAMP_UP_TIME = 0.3; //seconds to go from off to on
  const float RAMP_DOWN_TIME = 0.5; // seconds to go from on to off
  const float ON_TIME = 0.5; //time to remain on
  const float OFF_TIME = 0.3; //time to remain off

  void timelineSetDuration(Timeline *timeline) {
    int duration = 0;
    for (int i = 0; i < timeline->numFrames; i++) {
      duration += timeline->frames[i].time;
    }
    timeline->duration = duration;
  }

//  Timeline oneFlash(float rampUpDuration, float onDuration, float rampDownDuration, float delayDuration) {
//    Keyframe frames[] = {
//      keyframe(0, 0),
//      keyframe(rampUpDuration, ANALOG_MAX), //ramp up
//      keyframe(onDuration, ANALOG_MAX), //on
//      keyframe(rampDownDuration, 0), //ramp down
//      keyframe(delayDuration, 0), //delay for a few seconds
//    };
//    Timeline t = { 0, 5, frames };
//    timelineSetDuration(&t);
//    return t;
//  }

  Timeline *doubleFlash() {
    static Keyframe frames[] = {
      keyframe(0.0, 0),
      keyframe(RAMP_UP_TIME, ANALOG_MAX),
      keyframe(ON_TIME, ANALOG_MAX),
      keyframe(RAMP_DOWN_TIME, 0),
      keyframe(OFF_TIME, 0), //pause for 0.3 sec
      keyframe(RAMP_UP_TIME, ANALOG_MAX),
      keyframe(ON_TIME, ANALOG_MAX),
      keyframe(RAMP_DOWN_TIME, 0),
    };
    
    static Timeline t = { 0, 8, frames };
    timelineSetDuration(&t);
    return &t;
  }

  Timeline *singleFlash() {
    static Keyframe frames[] = {
      keyframe(0, 0),
      keyframe(RAMP_UP_TIME, ANALOG_MAX), //ramp up
      keyframe(RAMP_DOWN_TIME, 0), //ramp down
    };
    static Timeline t = { 0, 3, frames };
    timelineSetDuration(&t);
    return &t;
  }

#if USE_12BIT_PWM
  Timeline *singleFlash12Bit() {
    static Keyframe frames[] = {
      keyframe(0, 0),
      keyframe(RAMP_UP_TIME, PWM_MAX), //ramp up
      keyframe(RAMP_DOWN_TIME, 0), //ramp down
    };
    static Timeline t = { 0, 3, frames };
    timelineSetDuration(&t);
    return &t;
  }
#endif

  Timeline *pyralisMale() {
    static Keyframe frames[] = {
      keyframe(0.0, 0),
      keyframe(RAMP_UP_TIME, ANALOG_MAX),
      keyframe(0.25, ANALOG_MAX),
      keyframe(RAMP_DOWN_TIME, 0),
      keyframe(1.0, 0),
    };
    
    static Timeline t = { 0, 5, frames };
    timelineSetDuration(&t);
    return &t;
  }

}

class Flasher {
  protected:
  int pin; //pin number of the flasher
  int cycleTime; //time within the cycle, in milliseconds
  int delayTime; //time to delay between cycles, in milliseconds
  Timeline *timeline;

  int delayForCycle() {
    return this->delayTime;
  }

  protected:
  Flasher(int pin, Timeline *timeline, int delayTime) {
    this->pin = pin;
    this->cycleTime = 0;
    this->delayTime = delayTime;

    this->timeline = timeline;
  }

  public:
  //Single flash with a random delay 
  static Flasher *SingleFlasher(int pin) {
    return new Flasher(pin, Pattern::singleFlash(), RANDOM_DELAY_TIME);
  }

  //Double flash with a random delay 
  static Flasher *DoubleFlasher(int pin) {
    return new Flasher(pin, Pattern::doubleFlash(), RANDOM_DELAY_TIME);
  }

  //Flash in a pattern like the common eastern firefly, Photinus pyralis
  static Flasher *PyralisMale(int pin) {
    Flasher *f = new Flasher(pin, Pattern::pyralisMale(), 0);
    //delay starting the pattern up to 5 seconds so not all instances flash at once
    f->cycleTime = -random(0, 5000);
    return f;
  }

  virtual void setValue(int value) {
    //gamma correct the value
    int newValue = pgm_read_byte(&pwm_gamma[value]);
//    int newValue = (int)(pow((float)value / (float)ANALOG_MAX, 2.8) * ANALOG_MAX + 0.5);
    analogWrite(this->pin, newValue);
  }
  
  virtual void update(int deltaTime) {
    this->cycleTime = (this->cycleTime + deltaTime);
    
    if (this->cycleTime > this->timeline->duration) {
      this->cycleTime = -this->delayForCycle();
    }
    if (this->cycleTime < 0) {
      //we delay a cycle by setting cycle time to be negative
      return;
    }
    int newValue = 0;
  
    Keyframe prevFrame = this->timeline->frames[0];
    long prevFrameTime = prevFrame.time;
    for (int i = 1; i < this->timeline->numFrames; i++) {
      Keyframe frame = this->timeline->frames[i];

      long timeAfterPrevFrame = this->cycleTime - prevFrameTime;
      if (timeAfterPrevFrame <= frame.time) {
        float alpha = timeAfterPrevFrame / (float)frame.time;
        int delta = frame.value - prevFrame.value;
        //linear tweening
        newValue = prevFrame.value + (alpha * delta);
        break;
      } else {
        prevFrameTime += frame.time;
        prevFrame = frame;
      }
    }

    this->setValue(newValue);
  }

};

#if USE_12BIT_PWM

class PWMFlasher : public Flasher {
  protected:
  PWMFlasher(int pin, Timeline *timeline, int delayTime) : Flasher(pin, timeline, delayTime) { 
    //nothing to override
  }

  public:
  //Single flash with a random delay 
  static PWMFlasher *SingleFlasher(int pin) {
    return new PWMFlasher(pin, Pattern::singleFlash12Bit(), RANDOM_DELAY_TIME);
  }

  virtual void setValue(int value) {
    //gamma correct the value
    int newValue = (int)(pow((float)value / (float)PWM_MAX, 2.8) * PWM_MAX + 0.5);
    //set the PWM value
    pwm.setPin(this->pin, newValue);
  }
};

#endif


#if USE_12BIT_PWM
const int PWMPinCount = 16;
const int FlasherCount = AnalogPinCount + PWMPinCount;
#else
const int FlasherCount = AnalogPinCount;
#endif

Flasher *flashers[FlasherCount];

//const int FlasherCount = 6;
//Flasher *flashers[FlasherCount] = {
//  Flasher::SingleFlasher(3),
//  Flasher::DoubleFlasher(5),
//  Flasher::PyralisMale(6),
//  Flasher::DoubleFlasher(9),
//  Flasher::SingleFlasher(10),
//  Flasher::PyralisMale(11),
//};

unsigned long lastTime = 0;

void setup() {
//  Serial.begin(9600);
  for (int i = 0; i < AnalogPinCount; i++) {
    int pin = analogOutPins[i];
    pinMode(pin, OUTPUT);
    flashers[i] = Flasher::SingleFlasher(pin);
  }

#if USE_12BIT_PWM
  pwm.begin();
  pwm.setPWMFreq(1600);  // This is the maximum PWM frequency

#if !defined(ARDUINO_AVR_TRINKET3) && !defined(ARDUINO_AVR_TRINKET5)
  // save I2C bitrate
  uint8_t twbrbackup = TWBR;
  // must be changed after calling Wire.begin() (inside pwm.begin())
  TWBR = 12; // upgrade to 400KHz!
#endif

  for (int i = 0; i < PWMPinCount; i++) {
    flashers[i + AnalogPinCount] = PWMFlasher::SingleFlasher(i);
  }
#endif

}

void loop() {
  unsigned long currentTime = millis();
  int deltaTime = currentTime - lastTime;
  lastTime = currentTime;

  for (int i = 0; i < FlasherCount; i++) {
    flashers[i]->update(deltaTime);
  }
}

//LUT for gamma brightness correction
const uint8_t PROGMEM pwm_gamma[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 
  };
