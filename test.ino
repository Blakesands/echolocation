#include <AD9833.h>   


#define FNC_PIN 4     //ad9833

#define ST_IDLE 0    // state where we print last signal values to serial 
#define ST_PULSEON 1    // state where the PULSE will be switched on
#define ST_WAITON 2   // state where we wait till the PULSE has been on for a given duration
#define ST_PULSEOFF 3   // state where the PULSE will be switched off
#define ST_COUNT1 4 // state where we are waiting for hardware triggered ISR
#define ST_COUNT2 5 // state where we are resetting array index
#define ST_REPORT 6  // state where we are reading results 
#define ST_ACT 7  // state where we are  setting leds
#define ST_CALL 8  // state where we are waiting for command to scan (frequency, length of signal, Range time out, signal time out, comparator threshold .02v)// not written yet

// remember the current state; initialise with ST_IDLE
static byte currentState = ST_IDLE;

volatile long int signals[5] = {0};  //declares an array with 4 positions and sets them all to zero // 4 values is signal and 1 echo for testing principle
volatile int signalindex = 0;  //index to the array
volatile long int lastsignals[4]; // 
// volatile unsigned long int micros() = micros(); /// possible?? needs long??? need to deal with overflow??
bool havevalues = false;
//bool clickcall = false;

AD9833 gen(FNC_PIN);      // Function Generator

void setup() {

  gen.Begin(); 
  gen.ApplySignal(SINE_WAVE,REG0,10000); // Set pulse

  Serial.begin(115200); // debugging

  pinMode(2, INPUT); // interrupt pin
  pinMode(3, INPUT); // interrupt pin
}

void loop()
{

  switch (currentState)
  {
    case ST_IDLE:
    // ??? flash some leds?
    if (havevalues = true) 
    {
      Serial.println("Last Scan"); // debugging
      Serial.println(lastsignals[0]);
      Serial.println(lastsignals[1]);
      Serial.println(lastsignals[2]);
      Serial.println(lastsignals[3]);
      //        Serial.println(lastsignals[4]);
      //        Serial.println(lastsignals[5]);
    }

    currentState = ST_CALL;  // next state
    break;

    case ST_CALL:
    attachInterrupt (digitalPinToInterrupt (3), clickcallisr, RISING); // button to start scan moves to next state in isr
    break;

    case ST_PULSEON: // switch the PULSE on
    gen.EnableOutput(true); 
    signals[signalindex] = micros(); 
    signalindex++;
    currentState = ST_WAITON;  // next state
    break;

    case ST_WAITON:
    // check if on pulse duration has lapsed
    if (wait(20000) == true) // Pulse duration in micros
    {
      // if lapsed, go to next step (state)
      currentState = ST_PULSEOFF;  // next state
    }
    break;

    case ST_PULSEOFF:
    // switch the PULSE off
    gen.EnableOutput(false); 
    signals[signalindex] = micros(); // mark end of signal pulse
    signalindex++; 
    currentState = ST_COUNT1; // next state
    break;

    case ST_COUNT1:
    // fill remaining array values
    attachInterrupt (digitalPinToInterrupt (2), comparatortrigger1, RISING); // increments signal index

    while (signalindex < 3) {
      // do nothing
    }
    attachInterrupt (digitalPinToInterrupt (2), comparatortrigger2, FALLING);

    while (signalindex < 4) {
      // do nothing
    }

    if (signalindex == 4) {
        detachInterrupt (digitalPinToInterrupt (2));
        havevalues = true;
        currentState = ST_COUNT2;  // next state 
    }
    break;

    case ST_COUNT2:
    signalindex = 0; // reset array index count
    currentState = ST_REPORT;
    break;

    case ST_REPORT:
    for( int i = 0 ; i < 4 ; ++i ) {
      lastsignals[ i ] = signals[ i ]; //flag values - copy to global
    }
    currentState = ST_IDLE;
    break;

  }// end of state machine switcher
} // End of loop


void comparatortrigger1 () { 
  signals[signalindex] = micros();  /// "micros() works initially but will start behaving erratically after 1-2 ms".?????
  signalindex++;  //increment the array index to the next position
}   // end of ISR

void comparatortrigger2 () { 
  signals[signalindex] = micros();
  signalindex++;  //increment the array index to the next position 
}  // end of ISR

void clickcallisr () {
  //    clickcall = true;
  detachInterrupt (digitalPinToInterrupt (3));
  currentState = ST_PULSEON;  // next state    
}

bool wait(unsigned long duration) {
  static unsigned long startTime;
  static bool isStarted = false;
  if (isStarted == false) {
    startTime = micros();
    isStarted = true;
    return false;
  }
  if (micros() - startTime >= duration) {
    isStarted = false;
    return true;
  }
  return false; 
}   
