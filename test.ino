#include <AD9833.h>   


#define FNC_PIN 4     //ad9833

#define ST_IDLE 0    // state where we wait for command to scan ()
#define ST_PULSEON 1    // state where the PULSE will be switched on
#define ST_WAITON 2   // state where we wait till the PULSE has been on for a given duration
#define ST_PULSEOFF 3   // state where the PULSE will be switched off
#define ST_COUNT1 4 // state where we are waiting for hardware triggered ISR
#define ST_COUNT2 5 // state where we are resetting array index
#define ST_REPORT 6  // state where we are reading results and setting leds

// remember the current state; initialise with ST_IDLE
static byte currentState = ST_IDLE;

volatile int signals[6] = {0};  //declares an array with 6 positions and sets them all to zero
volatile byte signalindex = 0;  //index to the array
volatile int lastsignals[6]; // 
unsigned long int timer = micros(); /// possible??


AD9833 gen(FNC_PIN);      

void setup() {

    gen.Begin(); 
    gen.ApplySignal(SINE_WAVE,REG0,10000);
    gen.EnableOutput(false);  

    Serial.begin(115200);
    pinMode(2, INPUT);

    attachInterrupt (digitalPinToInterrupt (2), comparatortrigger1, RISING);
    attachInterrupt (digitalPinToInterrupt (2), comparatortrigger2, FALLING);
}

void loop()
{

  switch (currentState)
  {
    case ST_IDLE:
      // wait for command to scan
      // ??? flash some leds?
      Serial.println("Scanning");
      Serial.println(lastsignals[1]);
      Serial.println(lastsignals[2]);
      Serial.println(lastsignals[3]);
      Serial.println(lastsignals[4]);
      Serial.println(lastsignals[5]);
      Serial.println(lastsignals[6]);
            if (wait(1000000) == true)
      {
        // if lapsed, go to next step (state)
        currentState = ST_PULSEON;
      }
      
      break;  

    case ST_PULSEON:
      // switch the PULSE on
      gen.EnableOutput(true); 
      signals[signalindex] = timer; 
      signalindex++;  
      // go to the next step (state)
      currentState = ST_WAITON;
      break;

    case ST_WAITON:
      // check if on duration has lapsed
      if (wait(20000) == true)
      {
        // if lapsed, go to next step (state)
        currentState = ST_PULSEOFF;
      }
      break;

    case ST_PULSEOFF:
      // switch the PULSE off
      gen.EnableOutput(false); 
      signals[signalindex] = timer; 
      signalindex++; 
      currentState = ST_COUNT1;
      break;

    case ST_COUNT1:
      // fill remaining array values

      if (signalindex == 5)
      {
          lastsignals[6] = signals[6]; //flag values - copy to global
          currentState = ST_COUNT2;
      }
      break;

    case ST_COUNT2:
      // reset array index count
      for (signalindex = 0; signalindex < 5; signalindex++)
      {
          signals[signalindex] = 0;
          
      }
      currentState = ST_REPORT;
      break;

    case ST_REPORT:
      
      currentState = ST_IDLE;
      break;



      
  }
}
void comparatortrigger1 ()     
    { 
    signals[signalindex] = timer;  /// "micros() works initially but will start behaving erratically after 1-2 ms.?????
    signalindex++;  //increment the array index to the next position
    }   // end of ISR

void comparatortrigger2 ()     
    { 
    signals[signalindex] = timer; 
    signalindex++;  //increment the array index to the next position
    }  
/*
  check if a duration has lapsed
  In:
    duration (from timer)
  Returns:
    false if duration has not lapsed, else true
*/
bool wait(unsigned long duration)
{
  static unsigned long startTime;
  static bool isStarted = false;

  // if wait period not started yet
  if (isStarted == false)
  {
    // set the start time of the wait period
    startTime = micros();
    // indicate that it's started
    isStarted = true;
    // indicate to caller that wait period is in progress
    return false;
  }

  // check if wait period has lapsed
  if (micros() - startTime >= duration)
  {
    // lapsed, indicate no longer started so next time we call the function it will initialise the start time again
    isStarted = false;
    // indicate to caller that wait period has lapsed
    return true;
  }

  // indicate to caller that wait period is in progress
  return false;
}
