
/* Matt McGuire
   Compiles for Arduino pro / pro mini
   ATMEGA 328 (5v 16MHz)

   Inital build 4/12/2017 for  79 Suzuki GS550 project
   6/1/2016 increased button timeout on bounce 
*/
 
#include<avr/wdt.h>

//inputs will all be held high, if shunted to ground
//will be the signal to begin the process.
#define switchB A0  //Break input, solid input, mom push button
#define switchL A1  //left turn signal, mom push button, activate on 'up'
#define switchR A2  //Right turn signal, mom push button, activate on 'up'
#define switchH A3  //Headlight beam select, mom push button, activate on 'up'

#define RLBRK 2     //rear left break light (inner ring)
#define RRBRK 3     //rear right break light (inner ring)
#define RLTS 4      //rear left turn signal (outer ring)
#define RRTS 5      //rear right turn signal (outer ring)
#define FLTS 6      //front left turn signal
#define FRTS 7      //front right turn signal
#define HILOW 8     //high / low selector for headlight beam




#define LED 13      //led indicator for showing current state

#define _on LOW             //low state is on in this case, because using PNP transistors to do the heavy lifting
#define _off HIGH           //high state is a floating voltage, so it's off
#define _blinktime 500      //how quickly to blink for the turn signals (1/2 second)
#define _timeout 180000     //how long to blink for turn signals (ms ==1000ms*60sec) *3min max
#define _flshCntrMax 10     //maximum ammount of flashes the break light will do before being solid
#define _flshSleapTime 100  //how long to wait in milliseconds between flashes on the break light

enum {
  none,
  left,
  right
} currTS; //what the current turn signal is doing

unsigned long Time; //what the current time is
bool isontime; //if blinker should be on or off
int flshcntr; //how many times the break light flashed
int flserstate; //the flasher state
unsigned long futureTurnSignalCancelTime;

void setup() {
  Serial.begin(9600);
//  for (int i = 0; i < 4; i++) { //Inputs switchs held high
//    pinMode(i, INPUT);
//    digitalWrite(i, _off);
//  }

  pinMode(A0,INPUT);
  pinMode(A1,INPUT);
  pinMode(A2,INPUT);
  pinMode(A3,INPUT);
  digitalWrite(A0,HIGH);
  digitalWrite(A1,HIGH);
  digitalWrite(A2,HIGH);
  digitalWrite(A3,HIGH);
  
  for (int i = 2; i < 9; i++) { //outputs starting off low, keeps the lights off
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
  }
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  pinMode(HILOW, OUTPUT);
  digitalWrite(HILOW, LOW);

  watchdogsetup();
  Serial.println("Hello");
}


void watchdogsetup(){
  cli();
  wdt_reset();

  /*
  WDTSCR configureation
  7 WDIF -interrupt flag (auto flagged)
  6 WDIE -Interrupt enable
  5 WDP3 
  4 WDCE -safty enable config mode
  3 WDE -reset enable
  2 WDP2
  1 WDP1
  0 WDP0
  */
  //enter watchdog config mode
  WDTCSR |=B00011000;
  //set watchdog to 1 second
  WDTCSR = B01001110;

  sei();
}


void loop() {
  wdt_reset(); //reset the watchdog on each loop

  //---------------------------------------------------
  //see if this is an ontime or off time for the blinker
  unsigned long curr = millis();
  if (curr - Time >= _blinktime) {
    isontime = !isontime;
    Time = curr;
  }
  int blnk = isontime ? _off : _on;

  //---------------------------------------------------
  //check for the cancelation of turnsignals
  if (curr >= futureTurnSignalCancelTime) {
    currTS = none;
  }

  //---------------------------------------------------
  //Get the statuses
  bool B = isBreak();
  bool L = isLeftTurn();
  bool R = isRightTurn();
  setHiLowBeam();
  if (!B)flshcntr = 0;


  if (L || R) {
    if (currTS == none) {
      //the turn signal needs to start
      //and run for a max of 3 minutes
      futureTurnSignalCancelTime = curr + _timeout;
      currTS = L ? left : right;
     // Serial.println("in LR");
      //if (currTS==left) Serial.println("left triggered"); else Serial.println("Right triggered");
    } else {
      //the turn signal is being canceled, because of a
      //secondary push to either button
      currTS = none;
       Serial.println("cleared");
    }
  }

  //---------------------------------------------------
  //update the led outputs
  if (B && currTS == none) { //back lights should flash with break only
    digitalWrite(FLTS, LOW);
    digitalWrite(FRTS, LOW);
    if (flshcntr <= _flshCntrMax) {
      //flash between outer ring and inner ring
      if (flserstate) {
        digitalWrite(RLTS, HIGH); //outer ring on, inner off
        digitalWrite(RRTS, HIGH);
        digitalWrite(RLBRK, LOW);
        digitalWrite(RRBRK, LOW);
      } else {
        digitalWrite(RLTS, LOW); //outer ring off, inner on
        digitalWrite(RRTS, LOW);
        digitalWrite(RLBRK, HIGH);
        digitalWrite(RRBRK, HIGH);
      }
      flshcntr++;
      delay(_flshSleapTime);
      flserstate = !flserstate;
    } else {
      //all rear lights on solid
      digitalWrite(RLTS, HIGH);
      digitalWrite(RRTS, HIGH);
      digitalWrite(RLBRK, HIGH);
      digitalWrite(RRBRK, HIGH);
    }
    digitalWrite(LED, HIGH);  //show the break light is on
  } else if (B && currTS == left) { //right break on solid, left flashes
    digitalWrite(FLTS, blnk);
    digitalWrite(FRTS, LOW);
    digitalWrite(RLTS, blnk);
    digitalWrite(RRTS, HIGH);
    digitalWrite(RLBRK, LOW);
    digitalWrite(RRBRK, HIGH);
    digitalWrite(LED, isontime);
  } else if (B && currTS == right) { //left break on solid, right flashes
    digitalWrite(FLTS, LOW);
    digitalWrite(FRTS, blnk);
    digitalWrite(RLTS, HIGH);
    digitalWrite(RRTS, blnk);
    digitalWrite(RLBRK, HIGH);
    digitalWrite(RRBRK, LOW);
    digitalWrite(LED, isontime);
  } else if (!B && currTS == left) { //left flashes, no break
    digitalWrite(FLTS, blnk);
    digitalWrite(FRTS, LOW);
    digitalWrite(RLTS, blnk);
    digitalWrite(RRTS, LOW);
    digitalWrite(RLBRK, LOW);
    digitalWrite(RRBRK, HIGH);
    digitalWrite(LED, isontime);
     Serial.println("left no break");
  } else if (!B && currTS == right) { //right flashes, no break
    digitalWrite(FLTS, LOW);
    digitalWrite(FRTS, blnk);
    digitalWrite(RLTS, LOW);
    digitalWrite(RRTS, blnk);
    digitalWrite(RLBRK, HIGH);
    digitalWrite(RRBRK, LOW);
    digitalWrite(LED, isontime);
  } else { //no break or turn signals, rear outer ring stay on as marker signals
    digitalWrite(FLTS, LOW);
    digitalWrite(FRTS, LOW);
    digitalWrite(RLTS, HIGH);
    digitalWrite(RRTS, HIGH);
    digitalWrite(RLBRK, LOW);
    digitalWrite(RRBRK, LOW);
    digitalWrite(LED, LOW);
  }
}

/*------------------------------------------------------------------
  returns if the break switch(es) are currently on
  -------------------------------------------------------------------*/
bool isBreak() {
  static long lasttm;
  static int last;
  int state = digitalRead(switchB);
  bool ret = false;

  if (last != state) {
    lasttm = millis();
    last = state;
   // Serial.println("brk");
  }

  if ((millis() - lasttm) > 100) {
    ret = last == _on;
   // if (ret == true)Serial.println("brk");
  }

  return ret;
}


/*------------------------------------------------------------------
  only returns true on the release of the left turn signal button
  -------------------------------------------------------------------*/
bool isLeftTurn() {
  static long lasttm;
  static int last;
  static bool triggered;
  int state = digitalRead(switchL);
  bool ret = false;

  if (last != state) {
    lasttm = millis();
    last = state;
    triggered = true;
    //Serial.println("left");
  }

  if ((millis() - lasttm) > 100 && triggered) {
    ret = last == _on;
    triggered = false;
  }
  return ret;
}


/*------------------------------------------------------------------
  only returns true on the release of the right turn signal button
  -------------------------------------------------------------------*/
bool isRightTurn() {
  static long lasttm;
  static int last;
  static bool triggered;
  int state = digitalRead(switchR);
  bool ret = false;

  if (last != state) {
    lasttm = millis();
    last = state;
    triggered = true;
    //Serial.println("right");
  }

  if ((millis() - lasttm) > 100 && triggered) {
    ret = last == _on ? true : false;
    triggered = false;
  }
  return ret;
}

/*------------------------------------------------------------------
  will only change the hi/low beam on the release of the button
  -------------------------------------------------------------------*/
void setHiLowBeam() {
  static long lasttm;
  static int last;
  static bool triggered;
 
  int state = digitalRead(switchH);


  if (last != state ) {
    lasttm = millis();
    last = state;
    triggered = true;
  //  Serial.println("hi/lo");
  }

  if ((millis() - lasttm) > 100 && triggered && last==_on) {
      //Serial.println("alive");
    digitalWrite(HILOW, digitalRead(HILOW) == LOW ? HIGH : LOW);
    triggered = false;
  }
}

