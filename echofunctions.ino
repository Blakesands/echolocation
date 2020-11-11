/* 

Version 0.1 to test distance measurement accuracy - next step is to add more mics and perform triangulation
Controls an AD9833 waveform generator to emit short pulses (Bill William's Library)
Uses a MCP4132 digital pot to set a reference voltage to a LM211 comparator peak detection circuit (over SPi)
Comparator Circuit flips logic state when the threshold voltage set is exceeded by the input voltage from the microphone.
Reads a digital pin really fast to time a reflected sound wave and calculate range, uses range value to light leds

*/

#include <AD9833.h>  

#include <SPI.h>

#include <Adafruit_NeoPixel.h>
#define PIN            6
#define NUMPIXELS      15

Adafruit_NeoPixel strip = Adafruit_NeoPixel(15, PIN, NEO_GRB + NEO_KHZ800);

// Function Generator
#define FNC_PIN 4     
AD9833 gen(FNC_PIN); 

// testing variables grouped in one place
uint8_t speaker_test_threshold = 75; //  0 - 128
uint8_t sensitivity = 0; // steps of around 30mV
uint8_t scan_length = 30; // us
uint16_t frequency = 10000;
uint8_t scan_range = 40; // reflection timeout in ms
uint8_t wave_index = 0; // 0 = sine wave 1 = triangle
uint16_t distance_factor = 1; // depends on how fast it can fill range bin array **** TO BE FOUND

/// Potentiometer variables
static uint8_t address = 0x00;
static uint8_t CS= 10;
volatile uint8_t mic_test_threshold = 0;
uint8_t scan_threshold = 0; 

/// Potentiometer SPi comms
int digitalPotWrite(uint8_t value)
    {
        digitalWrite(CS, LOW);
        SPI.transfer(address);
        SPI.transfer(value);
        digitalWrite(CS, HIGH);
    }

// scan variables
static uint8_t wave_type[2] = {SINE_WAVE, TRIANGLE_WAVE};
volatile uint16_t range = 0; /// output of scan
volatile uint16_t echo_start = 0;
volatile uint16_t echo_end = 0;
volatile uint16_t echo_length = 0;
// Arrays and logging

volatile bool range_bin[1499];
volatile uint16_t bin_index = 0;

uint16_t check_values = 0; // validation counter
bool have_values = false;
bool click_called = false; /// pin3 isr flags click_called and detatches interrupt

// Self test
bool speaker_test = false;
bool mic_test = false;

//// timing function
bool wait(unsigned long duration)
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


void setup() 
{
   
    gen.Begin();  // Set AD9833
    gen.ApplySignal(SINE_WAVE,REG0,10000);

    strip.begin(); 
    strip.setBrightness(2);
    strip.show(); // Initialize all pixels to 'off'

    Serial.begin(115200); // debugging
    SPI.begin(); // potentiometer

    pinMode (CS, OUTPUT); // potentiometer
    pinMode(7, INPUT); // comparator pin
    pinMode(3, INPUT); // interrupt pin // triggers scan for testing
    pinMode(6, OUTPUT); // neopixels
    
}

void loop() /***********************************************************************************************************/
{
    
    self_test;

    Serial.println("Self Test: Done "); // debugging
    Serial.print("Speaker Test: "); // debugging
    Serial.println(speaker_test); // debugging
    Serial.print("Mic Test: "); // debugging
    Serial.println(mic_test); // debugging
    Serial.print("Scan Threshold: "); // debugging
    Serial.println(scan_threshold); // debugging
    Serial.print("inc Sensitivity: "); // debugging
    Serial.println(sensitivity); // debugging

    if (mic_test == false || speaker_test == false)
    {
        syserror; // a nasty wibble noise
        Serial.println("self test failed"); // debugging
        for( ; ; ) {} // infinite loop
    }

    click_ready; // plays a nice wibble noise and flashes leds
            
    attachInterrupt (digitalPinToInterrupt (3), clickcallisr, RISING); // interrupt for testing // isr detaches interrupt and flags click_called

    do while (click_called == false) // loop til reset
    {
        if (click_called == true)
        {
            scan;
            validate_readings;
            if (validate_readings == true)
            {
                validate_readings = false;
                report;
                act;
                click_ready;
            }

            attachInterrupt (digitalPinToInterrupt (3), clickcallisr, RISING);
            click_called = false; 
        }
    }
} /// END OF LOOP /********************************************************************************************************************************/

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
    digitalPotWrite(mic_test_threshold); /// Speaker is OFF. Threshold starts off at 23mV, all comparators high    
    delay(200);
    for (mic_test_threshold = 0; mic_test_threshold < 128; mic_test_threshold++)
    {
        if (comparator_test == false)
        {   
            scan_threshold = mic_test_threshold + sensitivity;
            mic_test = true; // all comparators should be high or mics arent working
            return (true);
        }
       digitalPotWrite(mic_test_threshold); 
      
    }
    mic_test = false; 
    return (true);
}

void click_ready () // plays a tone to show system ok
{
    gen.ApplySignal(SINE_WAVE,REG0,120);
    gen.EnableOutput(true); 
    strip.setBrightness(3);
    rainbowCycle(15);
    strip.show();
    while (wait(440000) == false)
    {
        for (int i = 120; i <= 340; i + 10)
            {
            gen.ApplySignal(TRIANGLE_WAVE,REG0,i);
            delay(10);
            }
        for (int i = 340; i >= 120; i - 10)
            {
            gen.ApplySignal(TRIANGLE_WAVE,REG0,i);
            delay(10);
            }
    }
    gen.EnableOutput(false);// turn off the ad9833 gen
}

void clickcallisr () // click button on interrupt pin 3 to start a scan
{
    detachInterrupt (digitalPinToInterrupt (3));
    click_called = true;
}


void scan ()
{
    digitalPotWrite(scan_threshold); // set comparator threshold
    gen.ApplySignal(wave_type[wave_index],REG0,frequency); // set ad9833 frequency and wave type
    gen.EnableOutput(true); 
    while (wait(scan_length) == false) // Pulse duration in micros init to 30 // 
        {    

        }
    gen.EnableOutput(false);     
    while (bin_index < 1500) // fill range bin array
      { 
        if(digitalRead(7) == HIGH) 
        { 
            range_bin[bin_index] = true; 
        }
        else
        {
            range_bin[bin_index] = false;
            check_values++; // increment validation counter
        }
        bin_index++;  
      }
    bin_index = 0; // reset array index count    
    // have_values = true; //flag values
}

/// validate readings if all are false, no reflection received, if all are true, mic is overloaded // *****add more rules
bool validate_readings ()
{
    if (check_values == 1500 || check_values == 0)
        {
            check_values = 0;
            return false;
        }
    check_values = 0;
    return true;
}

/// add more comparator pins here // add other pins //&& digitalRead(3) == HIGH// 
bool comparator_test () 
{
  if (digitalRead(7) == HIGH ) 
  { 
    return (true);
  }
   return (false);
}

void report () // Go through the bin index to find the first true value and then the next false value after that
{
    for (bin_index = 0; bin_index < 1500; bin_index++)
    {
        if (range_bin[bin_index] == true)
        {
            range = bin_index;
            Serial.println("Range Detected: "); // debugging
            Serial.println(range); // debugging
            for (bin_index = range; bin_index < 1500; bin_index++)
            {
                if (range_bin[bin_index] == false)
                {
                    echo_end = bin_index;
                    
                    echo_length = (echo_end - range);

                    Serial.println("Echo Length: "); // debugging
                    Serial.println(echo_length); // debugging
                    return;
                }

            }
        }
    }

//    Serial.println("Nothing Detected!"); // debugging
    return;
}
// void report () // Go through the bin index to find the first true value and then the next false value after that
// {
//     for (bin_index = 0; bin_index < 1500; bin_index++)
//     {
//         if (range_bin[bin_index] == true)
//         {
//             range = bin_index;
//             Serial.println("Range Detected: "); // debugging
//             Serial.println(range * distance_factor); // debugging
//             for (bin_index = range; bin_index < 1500; bin_index++)
//             {
//                 if (range_bin[bin_index] == false)
//                 {echo_start = bin_index;
//                 echo_start - range = echo_length;

//                 Serial.println("Echo Length: "); // debugging
//                 Serial.println(echo_length); // debugging
//                 return;
//                 }

//             }
//         }
//     }
//     Serial.println("Nothing Detected!"); // debugging
//     return;
// }

void act ()
{
    uint8_t i = map(range, 0, 1499, 1, 15);
    strip.setPixelColor(i, strip.Color(150,20,20));
    strip.show();
}

void syserror ()
{
    while (wait(1000000) == false)
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
        if (wait(1000000) == true) // micros
        {
            gen.EnableOutput(false);// turn off the ad9833 gen
        }
    }
}

void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}