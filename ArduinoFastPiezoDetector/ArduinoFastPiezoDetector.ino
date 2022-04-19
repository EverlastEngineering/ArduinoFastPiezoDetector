/*
 * ADC work
 */
 
// #define SERIAL_PLOT_MODE
#define MIN_THRESHOLD 20 //discard readings below this adc value
#define MIN_NOTE_THRESHOLD 30 //minimum time allowed between notes on the same channel

#define ANALOGPINS 6
byte adcPin[ANALOGPINS] = {0,1,2,3,4,5};  // This relies on A0 being 0, etc, which on the UNO is true. Mapped manually for clarity.

#define CHANNELS 4
volatile int maxResults[CHANNELS];

#define DISCARD 1
byte discardCounter = 0;

#ifdef SERIAL_PLOT_MODE
volatile int countedSamples = 0;
volatile int missedSamples = 0;
#endif

byte currentPin = 0;
int adcValue[CHANNELS];

volatile boolean inLoop;

/*
 * MIDI setup
 */

 //MIDI note defines for each trigger
#define SNARE_NOTE 70
#define LTOM_NOTE 71
#define RTOM_NOTE 72
#define LCYM_NOTE 73
#define RCYM_NOTE 74
#define KICK_NOTE 75

byte noteMap[4] = {LCYM_NOTE,SNARE_NOTE,LTOM_NOTE,RCYM_NOTE}; 

//MIDI defines
#define NOTE_ON_CMD 0x90
#define NOTE_OFF_CMD 0x80
#define MAX_MIDI_VELOCITY 127

int timeSinceLastNote[CHANNELS];
int lastNoteVelocity[CHANNELS];

//MIDI baud rate
//#define SERIAL_RATE 31250
#define SERIAL_RATE 38400

void setup ()
  {
  Serial.begin (SERIAL_RATE);
  Serial.println ();
  
  /* all the hard work for setting the registers and finding the fastest way to access the adc data was done by this fellow:
   *  http://yaab-arduino.blogspot.com/2015/02/fast-sampling-from-analog-input.html
   */

  ADCSRA = 0;             // clear ADCSRA register
  ADCSRB = 0;             // clear ADCSRB register
  ADMUX |= (adcPin[0] & 0x07);    // set A0 analog input pin
  ADMUX |= (1 << REFS0);  // set reference voltage
//  ADMUX |= (1 << ADLAR);  // left align ADC value to 8 bits from ADCH register

  // sampling rate is [ADC clock] / [prescaler] / [conversion clock cycles]
  // for Arduino Uno ADC clock is 16 MHz and a conversion takes 13 clock cycles
  ADCSRA |= (1 << ADPS2) | (1 << ADPS0);    // 32 prescaler for 38.5 KHz
//  ADCSRA |= (1 << ADPS2);                     // 16 prescaler for 76.9 KHz
//  ADCSRA |= (1 << ADPS1) | (1 << ADPS0);    // 8 prescaler for 153.8 KHz

  ADCSRA |= (1 << ADATE); // enable auto trigger
  ADCSRA |= (1 << ADIE);  // enable interrupts when measurement complete
  ADCSRA |= (1 << ADEN);  // enable ADC
  ADCSRA |= (1 << ADSC);  // start ADC measurements

  // end code from http://yaab-arduino.blogspot.com/2015/02/fast-sampling-from-analog-input.html

  for (int i=0;i<CHANNELS;i++) {
    maxResults[i]=0;
  }

  for (int i=0;i<ANALOGPINS;i++) {
     pinMode(adcPin[i], INPUT);
  }
} 

// ADC complete ISR
ISR (ADC_vect) {
    if (discardCounter == DISCARD) {
      if (inLoop) {
        #ifdef SERIAL_PLOT_MODE
          missedSamples++;
        #endif
        return;
      }
      currentPin++;
      discardCounter = 0;
    }
    else {
      discardCounter++;
      return;
    }

    #ifdef SERIAL_PLOT_MODE
      countedSamples++; 
    #endif
    
    if (currentPin == CHANNELS)
      currentPin = 0;

    if (ADC > maxResults[currentPin])
      maxResults[currentPin] = ADC;
    
    byte pin = currentPin+1;
    pin = pin%CHANNELS; // <-- yeah! if the pin is higher than CHANNELS, then "pin MOD CHANNELS" loops it back to 0. 
    ADMUX = bit (REFS0) | (adcPin[pin] & 7);
    
    // Interestingly, if you combine the (currentPin+1)&CHANNELS on any line, there is often a failure incrementing the selected pin; its too slow. There must be a compiler optimization that fails.
}
unsigned long now = 0;
void loop () {

  // the inLoop boolean prevents the ADC interrupt code from colliding with this loop while the work is done below. keep anything processor intensive OUT of this section of the loop, like Serial output or heavy math.
  // on an UNO, this loop work occasionally causes ONE sample to be discarded, which is only 1/50,000 of a seconds' worth of data. That's great.
  inLoop = true;

  for (int i=0;i<CHANNELS;i++) {
    adcValue[i] = maxResults[i];
    maxResults[i] = 0;
  }

  inLoop = false;

#ifndef SERIAL_PLOT_MODE
  // This is where the real code needs to go to DO something with this.
  now = millis();
  for (int i = 0; i<CHANNELS; i++) {
    if (adcValue[i] > MIN_THRESHOLD) {
      byte velocity = adcValue[i] / 8;
      if (velocity < lastNoteVelocity[i]) {
        noteDebug(noteMap[i],velocity);
        timeSinceLastNote[i] = now;
      }
      // if ((now-timeSinceLastNote[i]) > MIN_NOTE_THRESHOLD) 
      {
      Serial.print("Now: ");
      Serial.print(now);
      Serial.print(" TimeSinceLastNote: ");
      Serial.println(timeSinceLastNote[i]);
      lastNoteVelocity[i] = velocity;
      timeSinceLastNote[i] = now;
      }
    }
  }
    

#else
// Min / Max for scaling the graph
bool draw = false;
for (int i=0;i<CHANNELS;i++) {
  if (adcValue[i] > MIN_THRESHOLD) {
    draw = true;
  }
}
if (draw) {
  Serial.print ("Scale:");
  Serial.print ("1023");
  Serial.print (",MissedSamples:");
  Serial.print (missedSamples);
  Serial.print (",Counted_Samples:"); 
  Serial.print (countedSamples);
  for (int i=0;i<CHANNELS;i++) {
    Serial.print (",Pin_");
    Serial.print (i);
    Serial.print (":");
    Serial.print (adcValue[i]);
  }
  Serial.println ();
}
  countedSamples = missedSamples = 0;
#endif

  
/*  This delay ensures enough time to prevent aliasing. 
 *  For example:
 *  If countedSamples == 50000 per second, and CHANNELS == 6, then you'll get 8333 samples per second.
 *  Half are discarded by the DISCARD system to allow the analog pin to settle.
 *  Half again can be considered discarded by being a negative voltage (unless you either full bridge recitfy (BOOOM) or bias the ac signal, which you should.
 *  In my test, my UNO with 6 channels, i could sample down to 80Hz without missing the peak with a delay of 10, thus getting a stable max value on a sine wave with an acceptable latency.
 *  Less channels and higher minimum freq means you can lower this delay.
 *  
 *  Note that at HIGHER piezo freqencies, the sampling algo will break down if they match the sample rate or are multiple of it, due to the sample peak being 'skipped over'.
 *  This can be addressed by sampling one channel at a much higher freq for CHANNEL # of cycles ones, but that can cause slightly less accuracy to get missed at lower freqs occasionally too.
 *  It's more logic in the interrupt though, which is bad.
 *  
 *  The short of it is: tune the system to your expected piezo frequency. This will NOT do higher AND lower freqs perfectly without compromise.
 */

  // on my UNO, 3ms can reliably detect the max value of single peak 100hz sine wave.
  delay(2);
}

void noteDebug(byte note, byte velocity)
{
  Serial.print("Note: ");
  Serial.print(note);
  Serial.print(" Velocity: ");
  Serial.print(velocity);
  Serial.println();
}

void noteFire(byte note, byte velocity)
{
  midiNoteOn(note, velocity);
  midiNoteOff(note, velocity);
}

void midiNoteOn(byte note, byte midiVelocity)
{
  Serial.write(NOTE_ON_CMD);
  Serial.write(note);
  Serial.write(midiVelocity);
}

void midiNoteOff(byte note, byte midiVelocity)
{
  Serial.write(NOTE_OFF_CMD);
  Serial.write(note);
  Serial.write(midiVelocity);
}
