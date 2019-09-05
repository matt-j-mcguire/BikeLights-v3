
/* Matt McGuire
   Compiles for Arduino Nano ATmega328p
   ATMEGA 328 (5v 16MHz)

   Inital build 4/12/2017 for  79 Suzuki GS550 project
   6/1/2016 increased button timeout on bounce 
   6/18/2018 rework for new hardware (v3a)
*/
#include<avr/wdt.h>


#define switchB A0  //Break input, solid input, mom push button
#define switchL A1  //left turn signal, mom push button, activate on 'up'
#define switchR A2  //Right turn signal, mom push button, activate on 'up'
#define switchH A3  //Headlight beam select, mom push button, activate on 'up'
#define RLBRK   2   //rear left break light (inner ring)
#define RRBRK   3   //rear right break light (inner ring)
#define RLTS    4   //rear left turn signal (outer ring)
#define RRTS    5   //rear right turn signal (outer ring)
#define FLTS    6   //front left turn signal
#define FRTS    7   //front right turn signal
#define HILOW   8   //high / low selector for headlight beam

#define _on LOW             //low state is on in this case, because using PNP transistors to do the heavy lifting
#define _off HIGH           //high state is a floating voltage, so it's off
#define _blinktime 500      //how quickly to blink for the turn signals (1/2 second)
#define _timeout 180000     //how long to blink for turn signals (ms ==1000ms*60sec) *3min max
#define _flshCntrMax 10     //maximum ammount of flashes the break light will do before being solid
#define _flshSleapTime 100  //how long to wait in milliseconds between flashes on the break light
#define _scrollTime 500     //how long between shifting the scrolling turnsignal

enum {
  none,
  left,
  right
} currTS; //what the current turn signal is doing

unsigned long Time;             //what the current time is
bool isontime;                  //if blinker should be on or off
int flshcntr;                   //how many times the break light flashed
int flserstate;                 //the flasher state
unsigned long futTSCancelTime;  //when to cancel the turn signal
unsigned long futureflashtime;  //when the next time the break light should flash
byte leftscroll;
byte rightscroll;
unsigned long futureScroll;


void setup() {
  //Serial.begin(9600);
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

  watchdogsetup();
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
  //reset the watchdog on each loop
  wdt_reset(); 

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
  if (curr >= futTSCancelTime) {
    currTS = none;
  }

  //---------------------------------------------------
  //Get the statuses
  bool B = isBreak();
  bool L = isLeftTurn();
  bool R = isRightTurn();
  setHiLowBeam();

  //---------------------------------------------------
  //switch state for the break flashing
  if (curr >=futureflashtime){
    flserstate = !flserstate;
    if(B && flserstate)flshcntr++;
    futureflashtime=curr+_flshSleapTime;   
  }
  if (!B)flshcntr = 0; 

  static byte scrcnt;
  if(curr>=futureScroll){
    futureScroll = curr+_scrollTime;
    scrcnt =++scrcnt % 3;
    leftscroll = (leftscroll<<1) | (!scrcnt & 1);
    rightscroll = (rightscroll >>1) | (!scrcnt<<7);
    //Serial.println(leftscroll & B1111);
  }


  if (L || R) {
    if (currTS == none) {
      //the turn signal needs to start
      //and run for a max of 3 minutes
      futTSCancelTime = curr + _timeout;
      currTS = L ? left : right;
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
      digitalWrite(RLTS, flserstate);
      digitalWrite(RRTS, flserstate);
      digitalWrite(RLBRK, !flserstate);
      digitalWrite(RRBRK, !flserstate);
    } else {
      //all rear lights on solid
      digitalWrite(RLTS, HIGH);
      digitalWrite(RRTS, HIGH);
      digitalWrite(RLBRK, HIGH);
      digitalWrite(RRBRK, HIGH);
    }
  } else if (B && currTS == left) { //right break on solid, left flashes
    digitalWrite(FLTS, blnk);
    digitalWrite(FRTS, LOW);
    digitalWrite(RLTS, blnk);
    digitalWrite(RRTS, HIGH);
    digitalWrite(RLBRK, LOW);
    digitalWrite(RRBRK, HIGH);
  } else if (B && currTS == right) { //left break on solid, right flashes
    digitalWrite(FLTS, LOW);
    digitalWrite(FRTS, blnk);
    digitalWrite(RLTS, HIGH);
    digitalWrite(RRTS, blnk);
    digitalWrite(RLBRK, HIGH);
    digitalWrite(RRBRK, LOW);
  } else if (!B && currTS == left) { //left flashes, no break
    digitalWrite(FLTS, blnk);
    digitalWrite(FRTS, LOW);
//    digitalWrite(RLTS, blnk);
//    digitalWrite(RRTS, LOW);
//    digitalWrite(RLBRK, LOW);
//    digitalWrite(RRBRK, HIGH);
    byte t = leftscroll;
    digitalWrite(RLBRK,(t & 8));
    digitalWrite(RLTS, (t & 4));
    digitalWrite(RRTS, (t & 2));
    digitalWrite(RRBRK, (t & 1));
  } else if (!B && currTS == right) { //right flashes, no break
    digitalWrite(FLTS, LOW);
    digitalWrite(FRTS, blnk);
//    digitalWrite(RLTS, LOW);
//    digitalWrite(RRTS, blnk);
//    digitalWrite(RLBRK, HIGH);
//    digitalWrite(RRBRK, LOW);
    byte t= rightscroll;
    digitalWrite(RLBRK,(t & 8));
    digitalWrite(RLTS, (t & 4));
    digitalWrite(RRTS, (t & 2));
    digitalWrite(RRBRK, (t & 1));
  } else { //no break or turn signals, rear outer ring stay on as marker signals
    digitalWrite(FLTS, LOW);
    digitalWrite(FRTS, LOW);
    digitalWrite(RLTS, HIGH);
    digitalWrite(RRTS, HIGH);
    digitalWrite(RLBRK, LOW);
    digitalWrite(RRBRK, LOW);
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
  }

  if ((millis() - lasttm) > 100) {
    ret = last == _on;
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
  }

  if ((millis() - lasttm) > 100 && triggered && last==_on) {
      //Serial.println("alive");
    digitalWrite(HILOW, digitalRead(HILOW) == LOW ? HIGH : LOW);
    triggered = false;
  }
}

