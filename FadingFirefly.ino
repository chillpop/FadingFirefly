extern const uint8_t PROGMEM gamma[];

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
//      keyframe(rampUpDuration, 255), //ramp up
//      keyframe(onDuration, 255), //on
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
      keyframe(RAMP_UP_TIME, 255),
      keyframe(ON_TIME, 255),
      keyframe(RAMP_DOWN_TIME, 0),
      keyframe(OFF_TIME, 0), //pause for 0.3 sec
      keyframe(RAMP_UP_TIME, 255),
      keyframe(ON_TIME, 255),
      keyframe(RAMP_DOWN_TIME, 0),
    };
    
    static Timeline t = { 0, 8, frames };
    timelineSetDuration(&t);
    return &t;
  }

  Timeline *singleFlash() {
    static Keyframe frames[] = {
      keyframe(0, 0),
      keyframe(RAMP_UP_TIME, 255), //ramp up
      keyframe(RAMP_DOWN_TIME, 0), //ramp down
    };
    static Timeline t = { 0, 3, frames };
    timelineSetDuration(&t);
    return &t;
  }

  Timeline *pyralisMale() {
    static Keyframe frames[] = {
      keyframe(0.0, 0),
      keyframe(RAMP_UP_TIME, 255),
      keyframe(0.25, 255),
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
    pinMode(pin, OUTPUT);
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

    //gamma correct the value
    newValue = pgm_read_byte(&gamma[newValue]);
    analogWrite(this->pin, newValue);    
  }

};

/*
class DigitalFlasher : public Flasher {
  protected:
  DigitalFlasher(int pin, Timeline *timeline, int delayTime) : Flasher(pin, timeline, delayTime) {
  }

  public:
  static DigitalFlasher *singleFlasher(int pin) {
    static Keyframe frames[] = {
      keyframe(0, HIGH),
      keyframe(0.3, LOW),
    };
    static Timeline t = { 300, 2, frames };

    DigitalFlasher *f = new DigitalFlasher(pin, &t, RANDOM_DELAY_TIME);
    return f;
  }

  void update(int deltaTime) {
    this->cycleTime = (this->cycleTime + deltaTime);
    
    if (this->cycleTime > this->timeline->duration) {
      this->cycleTime = -this->delayForCycle();
    }

    Keyframe prevFrame = this->timeline->frames[0];
    int newValue = prevFrame.value;    

    long prevFrameTime = prevFrame.time;
    unsigned int runningDuration = prevFrame.time;
    for (int i = 1; i < this->timeline->numFrames; i++) {
      Keyframe frame = this->timeline->frames[i];

      runningDuration += frame.time;
      if (runningDuration > this->cycleTime) {
        break;
      } else {
        newValue = frame.value;
      }
    }

    digitalWrite(this->pin, newValue);
  }

};
*/

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
//analog out pins for Feather
const int AnalogPinCount = 8;
int analogOutPins[] = {
  3,
  5,
  6,
  9,
  10,
  11,
  12,
  13,
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

const int FlasherCount = AnalogPinCount;
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
    flashers[i] = Flasher::SingleFlasher(pin);
  }
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
const uint8_t PROGMEM gamma[] = {
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

