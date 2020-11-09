#include <AD9833.h>   
#include <SPI.h>

// Function Generator
#define FNC_PIN 4     
AD9833 gen(FNC_PIN); 

bool self_test () // checks the mics can hear the speaker, then raises the logic flip threshold until the comparators go off
{

    digitalPotWrite(speaker_test_threshold); // 0-128
    gen.Reset(); 
    gen.ApplySignal(SINE_WAVE,REG0,10000);
    gen.EnableOutput(true); 
    delay(200);
    if (comparator_test == true) 
    {
        gen.EnableOutput(false);
        speaker_test = true;
    }
    else 
    {
        gen.EnableOutput(false);
        speaker_test = false;
    }
    digitalPotWrite(mic_test_threshold); /// starts off at 0V so all comparators should be high due to environmental noise     
    delay(200);
    for (mic_test_threshold = 0, mic_test_threshold < 128, mic_test_threshold++)
    {
        if (comparator_test == false) 
        {   
            scan_threshold = mic_test_threshold + sensitivity;
            mic_test = true; // all comparators should be high or mics arent working
            Serial.println("Self Test: Done "); // debugging
            Serial.print("Speaker Test: "); // debugging
            Serial.print(speaker_test); // debugging
            Serial.println("Mic Test: "); // debugging
            Serial.print(mic_test); // debugging
            return true;
        }
       digitalPotWrite(mic_test_threshold); 
      
    }
    mic_test = false; 
    Serial.println("Self Test: Done "); // debugging
    Serial.print("Speaker Test: "); // debugging
    Serial.print(speaker_test); // debugging
    Serial.println("Mic Test: "); // debugging
    Serial.print(mic_test); // debugging
    return true;
}  


void scan ()
{
    wave_index = 0; // 0 sine wave 1 triangle wave
    frequency = 10000;
    scan_length = 30;
    digitalPotWrite(scan_threshold); // set comparator threshold
    gen.ApplySignal(wave_type[wave_index],REG0,frequency); // set ad9833 frequency and wave type
    gen.EnableOutput(true); 
    if (wait(scan_length) == true) // Pulse duration in micros init to 30
        {
            gen.EnableOutput(false); 
        }
    while (bin_index < 1500) // fill range bin array
      { 
        if(digitalRead(7) == HIGH) 
        { 
            range_bin[bin_index] = true; 
        }
        else
        {
            range_bin[bin_index] = false;
        }
        bin_index++;  
      }
    bin_index = 0; // reset array index count
    have_values = true; //flag values
    for( int i = 0 ; i < 1500 ; ++i ) 
        {
        range_bin[ i ] = last_range_bin[ i ]; 
        }
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

/// Potentiometer variables
uint8_t address = 0x00;
uint8_t CS= 10;
uint8_t speaker_test_threshold = 75; //  0 - 128
uint8_t mic_test_threshold = 0;
uint8_t scan_threshold = 0; 
uint8_t sensitivity = 0; // steps of around 30mV

// scan variables
uint8_t scan_length = 30; // us
uint16_t frequency = 10000;
uint8_t scan_range = 40; // reflection timeout in ms
uint8_t scan_errorcheck = 3; // timeout for receiving signal pulse in ms
static uint8_t wave_type[2] = {SINE_WAVE, TRIANGLE_WAVE};
uint8_t wave_index = 0;

// Arrays and logging
volatile bool range_bin[1499];
volatile bool last_range_bin[1499];
volatile uint16_t bin_index = 0;

bool have_values = false;
bool click_called = false;
bool self_test = false;
bool speaker_test = false;
bool mic_test = false;


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


}