#include <AD9833.h>   
#include <SPI.h>

#define FNC_PIN 4     //ad9833

// states for state machine switcher
#define ST_IDLE 0    // Print last signal values to serial 
#define ST_CALL 8  // Waiting for interrupt command to scan (frequency, length of signal, Range time out, signal time out, comparator threshold .02v)/
#define ST_PULSEON 1    // The PULSE will be switched on
#define ST_WAITON 2   // Wait till the PULSE has been on for a given duration
#define ST_PULSEOFF 3   // The PULSE will be switched off
#define ST_COUNT1 4 // digital read and loop til array is full
#define ST_COUNT2 5 // Resetting array index
#define ST_REPORT 6  // Reading results 
#define ST_ACT 7  // Setting leds
#define ST_CALL 25  // Waiting for command to scan (frequency, length of signal, Range time out, signal time out, comparator threshold .02v)// not written yet
#define ST_STATIC 9  // state where we are flashing leds in standby

/// Self Test Sequence on start up
#define ST_TSIG 10  // state where we reset the ad9833 and set and enable the test signal
#define ST_DPOT 11  // set the threshold voltage 0 = 23mv,  103 = 3.3v,128 = 4.6v with a 10kR resistor (voltage divider)
#define ST_WAIT1 12  // waiting for the digipot (ns) /mics ?? (us) /comparators (ns) to settle
#define ST_READ 13  // read the bool for comparator pin(s) true is on - all must be true 
#define ST_ERROR 15  // error state where we are modulating tone if all comparators do not return true (on) 
#define ST_ERROR2 22  // error state where we are modulating tone if all comparators do not return true (on) 
#define ST_STOPS 16 // state where we stop test signal
#define ST_DPOT1 17 // Set the voltage on the digipot 0 = 23mv - set to a low level ****to be decided - then to waiting state
#define ST_WAIT2 18  // state wait
#define ST_THRESH 19  // check the threshold against the max threshold number //// 0 - 128 23mV - 4.6V - go to error 2v ** test
#define ST_STORE 20  // state where we store threshold value, flag test done and return to idle

/// Potentiometer code
uint8_t address = 0x00;
uint8_t CS= 10;
uint8_t speaker_test_threshold = 75; // Threshold voltage set 0 - 128
uint8_t mic_test_threshold = 0; // Threshold voltage set 0 - 128
uint8_t current_threshold = 0; 
uint8_t sensitivity = 0; // steps of around 30mV

// remember the current/previous state; initialise with ST_IDLE
static uint8_t currentState = ST_IDLE;
static uint8_t prevState = ST_WAIT1;

// Arrays and logging
volatile bool range_bin[1499];
volatile bool last_range_bin[1499];
volatile uint16_t bin_index = 0;

bool havevalues = false;
bool clickcalled = false;

AD9833 gen(FNC_PIN);      // Function Generator

void setup() 
{
  gen.Begin();  // Set AD9833
  gen.ApplySignal(SINE_WAVE,REG0,10000);
  Serial.begin(115200); // debugging
  SPI.begin(); // potentiometer
  pinMode (CS, OUTPUT); // potentiometer
  pinMode(7, INPUT); // comparator pin
  pinMode(3, INPUT); // interrupt pin // triggers scan for testing
}

void loop()
{
  switch (currentState) // Beginning of State Machine switcher
  {
    case ST_IDLE:
      if (havevalues == true) 
      {
                //        Serial.println("Last Scan"); // debugging
                //        Serial.println(lastsignals[0]);
                //        Serial.println(lastsignals[1]);
                //        Serial.println(lastsignals[2]);
                //        Serial.println(lastsignals[3]);
                //        Serial.println(lastsignals[4]);
                //        Serial.println(lastsignals[5]);
      }
      currentState = ST_CALL;  // next state
      break;

    case ST_CALL:
      attachInterrupt (digitalPinToInterrupt (3), clickcallisr, RISING); 
      while (clickcalled == false) 
      {
        if (clickcalled == true)
        {
        currentState = ST_PULSEON;  // next state  
        }
      }
      break;

    case ST_PULSEON: // switch the PULSE on
      gen.EnableOutput(true); 
      range_bin[bin_index] = micros(); // sets initial signal time value on signals[0]
      bin_index++; // increments signal index  //index to the array = 1
      currentState = ST_WAITON;  // next state
      break;

    case ST_WAITON: // check if on pulse duration has lapsed
      if (wait(30) == true) // Pulse duration in micros // works out to about 1cm
      {
        currentState = ST_PULSEOFF;  // next state
      }
      break;

    case ST_PULSEOFF: // switch the PULSE off
      gen.EnableOutput(false); 
      range_bin[bin_index] = micros(); // mark end of signal pulse on signals[1]
      bin_index++;  //index to the array = 2
      currentState = ST_COUNT1; // next state
      break;

    case ST_COUNT1: /******************* RANGE BIN*/
      while (bin_index < 1500) // figure out how much i can store/process
      { 
        if(digitalRead(7) == HIGH) // read the digital pin 7
        { 
          range_bin[bin_index] = true; // store the pin bool in the array
        }
        else
        {
          range_bin[bin_index] = false;
        }
        bin_index++;  
      }
      currentState = ST_REPORT;
      break;

    case ST_DPOT1: // start mic test
      int threshold = mic_test_threshold;
      digitalPotWrite(threshold);
      currentState = ST_WAIT2;  // next state
      break;  

    case ST_REPORT:
      havevalues = true; //flag values
      bin_index = 0; // reset array index count
      for( int i = 0 ; i < 1500 ; ++i ) 
      {
        range_bin[ i ] = last_range_bin[ i ]; // copy to global
      }
      currentState = ST_IDLE; // go to set leds *** To be done
      break;

          // Self test sequence
    case ST_TSIG:
      gen.Reset(); 
      gen.ApplySignal(SINE_WAVE,REG0,10000);
      gen.EnableOutput(true); 
      currentState = ST_DPOT; 
      break;

    case ST_DPOT:
      digitalPotWrite(speaker_test_threshold);
      currentState = ST_WAIT1;  
      break;

    case ST_WAIT1:
      if (wait(500000) == true) 
      {
          currentState = ST_READ;  
      }
      break;

    case ST_READ:
      if (comparator_test == true) 
      {
        currentState = ST_STOPS;  // next state
      }
      else 
      {
        currentState = ST_ERROR;  // go to error state
      }
      break;

    case ST_ERROR:
        while (wait(10000000) == false)
        {
          gen.ApplySignal(TRIANGLE_WAVE,REG0,220);
          gen.EnableOutput(true); 
          for (int i = 215; i <= 445; i++)
            {
              gen.ApplySignal(TRIANGLE_WAVE,REG0,i);
              delay(100000/230);
            }
          delay(500); // millis
          for (int i = 445; i >= 215; i--)
            {
              gen.ApplySignal(TRIANGLE_WAVE,REG0,i);
              delay(100000/230);
            }
          if (wait(10000000) == true) // micros
            {
              gen.EnableOutput(false);// turn off the ad9833 gen
              currentState = ST_IDLE;  // back to idle state
            }
        }
      break;

    case ST_ERROR:
      gen.ApplySignal(SINE_WAVE,REG0,120);
      gen.EnableOutput(true);
      while (wait(1000000) == false)
      { 
        if (wait(1000000) == true) // micros
          {
            gen.EnableOutput(false);// turn off the ad9833 gen
            currentState = ST_IDLE;  // back to idle state
          }
      }
      break;  

    case ST_STOPS:
      gen.EnableOutput(false); 
      currentState = ST_DPOT1;  // next state
      break;

    case ST_WAIT2:
      if (wait(500000) == true) 
      {
          currentState = ST_THRESH;  // next state
      }
      break;

    case ST_THRESH:
      if (comparator_test == false) 
      {
        currentState = ST_STOPS;  // next state
      }
      else 
      {
        currentState = ST_ERROR;  // go to error state
      }
      break;

  }// end of state machine switcher
} // End of loop

void clickcallisr () // call to scan isr
{
  detachInterrupt (digitalPinToInterrupt (3));
  clickcalled = true;
}

bool wait(unsigned long duration) //// Waiting function
{
  static unsigned long startTime;
  static bool isStarted = false;
  if (isStarted == false) 
  {
    startTime = micros();
    isStarted = true;
    return false;
  }
  if (micros() - startTime >= duration) 
  {
    isStarted = false;
    return true;
  }
  return false; 
}  

/// add more comparator pins here // add other pins //&& digitalRead(3) == HIGH// 
bool comparator_test () 
{
  if (digitalRead(7) == HIGH ) 
  { 
    return true;
  }
   return false;
}

/// Potentiometer code
int digitalPotWrite(int value)
  {
    digitalWrite(CS, LOW);
    SPI.transfer(address);
    SPI.transfer(value);
    digitalWrite(CS, HIGH);
  }

/***************************************************************************/
