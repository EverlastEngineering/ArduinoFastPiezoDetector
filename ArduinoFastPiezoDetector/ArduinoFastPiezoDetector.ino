#include <AltSoftSerial.h>
AltSoftSerial midiSerial; // 2 is RX, 3 is TX

/*  
 * ADC work
 */
 
// #define SERIAL_PLOT_MODE
// #define DEBUG

#define MIN_THRESHOLD 30 //discard readings below this adc value
#define MIN_NOTE_THRESHOLD 60 //minimum time allowed between notes on the same channel

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

#define DIGITAL_INPUTS 2
#define KICK_INPUT_PIN 5
#define HIT_HAT_INPUT_PIN 6

bool buttonPressed[] = {true, true};
byte digitalPins[] = {KICK_INPUT_PIN, HIT_HAT_INPUT_PIN};

/*
 * MIDI setup
 */

// #define SEND_NOTE_OFF_VELOCITY
bool hiHatPedalMakesOpeningSounds = false;

#define SNARE_NOTE 38
#define LTOM_NOTE 41
#define RTOM_NOTE 43
#define HI_HAT_CLOSED_NOTE 42
#define HI_HAT_OPEN_NOTE 46
#define PEDAL_HI_HAT_NOTE 44
#define CRASH 49
#define KICK_NOTE 36

byte noteMap[4] = {HI_HAT_OPEN_NOTE,SNARE_NOTE,LTOM_NOTE,CRASH}; 

//MIDI defines
#define NOTE_ON_CMD 0x99
#define NOTE_OFF_CMD 0x89
#define MAX_MIDI_VELOCITY 127
#define NOMINAL_MIDI_VELOCITY 64

unsigned long timeOfLastNote[CHANNELS];
bool channelArmed[CHANNELS];
int lastNoteVelocity[CHANNELS];

#define SEND_BYTES(x) for (byte l : x) { midiSerial.write(l);}

// MIDI baud rate
#define MIDI_SERIAL_RATE 31250
#define SERIAL_RATE 38400

void setup ()
  {
  Serial.println("Startup");
  
  Serial.begin (SERIAL_RATE);
  midiSerial.begin(MIDI_SERIAL_RATE);
  
  for (int i=0;i<CHANNELS;i++) {
    maxResults[i]=0;
    timeOfLastNote[i]=0;
  }

  for (int i=0;i<DIGITAL_INPUTS;i++) {
    pinMode(digitalPins[i], INPUT_PULLUP);
  }

  pinMode(LED_BUILTIN, OUTPUT);

  // FYI the Roland Mt-80s Device ID is 16 - 0x10
  // GS Mode Reset - Seems to be unnecessary so removed for now
  // byte sysexReset[] =  { 0xF0,0x41,0x16,0x42,0x12,0x40,0x00,0x7F,0x00,0x41,0xF7 };
  // SEND_BYTES(sysexReset)

  // Use SysEx to Assign Mode to "2" on Channel 10, which allows notes to overlap each other
  // Otherwise, a new note with a lower velocity will chop the existing note's sustain to the new volume
  byte assignMode[] =  { 0xF0,0x41,0x10,0x42,0x12,0x40,0x10,0x14,0x02,0x1a,0xF7 };
  SEND_BYTES(assignMode)
  delay(500);

  // This sets the drum kit.
  // byte setKit[] = {0xC9,0x10};
  // SEND_BYTES(setKit)

  /* working kits
  0x10 = power kit aka Phil Collins
  0x18 = electronic 
  0x19 = moar electronic 
  0x28 = brush
  /*


  /* ADC Setup
  * all the hard work for setting the registers and finding the fastest way to access the adc data was done by this fellow:
  * http://yaab-arduino.blogspot.com/2015/02/fast-sampling-from-analog-input.html
  */

  ADCSRA = 0;             // clear ADCSRA register
  ADCSRB = 0;             // clear ADCSRB register
  ADMUX |= (adcPin[0] & 0x07);    // set A0 analog input pin
  ADMUX |= (1 << REFS0);  // set reference voltage
  // ADMUX |= (1 << ADLAR);  // left align ADC value to 8 bits from ADCH register

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
    
    discardCounter = 0;
    #ifdef SERIAL_PLOT_MODE
      countedSamples++; 
    #endif

    if (ADC > maxResults[currentPin])
      maxResults[currentPin] = ADC;
    
    currentPin++;
    currentPin = currentPin%CHANNELS;
  
    ADMUX = bit (REFS0) | (adcPin[currentPin] & 7);
  }
  else {
    discardCounter++;
    return;
  }
}
unsigned long now = 0;

void loop () {
  // this pulls the LED down and off, its turned on during a midiWrite event at least 2ms ago
  digitalWrite(LED_BUILTIN, LOW);

  digitalPinScan();

  // the inLoop boolean prevents the ADC interrupt code from colliding with this loop while the work is done below. keep anything processor intensive OUT of this section of the loop, like Serial output or heavy math.
  // on an UNO, this loop work occasionally causes ONE sample to be discarded, which is only 1/50,000 of a seconds' worth of data. That's great.
  inLoop = true;

  for (int i=0;i<CHANNELS;i++) {
    adcValue[i] = maxResults[i];
    maxResults[i] = 0;
  }

  inLoop = false;

  // This is where the real code needs to go to DO something with this.
  now = millis();
  for (int i = 0; i<CHANNELS; i++) {
    if (adcValue[i] > MIN_THRESHOLD) {
      long timeSinceLastNote = now - timeOfLastNote[i];
      byte velocity = adcValue[i] / 8;
      if (velocity > lastNoteVelocity[i]) {
        channelArmed[i] = true;
      }
      else if (channelArmed[i]) {
        if (timeSinceLastNote > MIN_NOTE_THRESHOLD) {
          handleAnalogEvent(noteMap[i],lastNoteVelocity[i]);
          
          timeOfLastNote[i] = now;
        }
        channelArmed[i] = false;
      }
      lastNoteVelocity[i] = velocity;
      
      
      #ifdef DEBUG
      Serial.print("Channel: ");
      Serial.print(i);
      Serial.print(" Velocity: ");
      Serial.print(velocity);
      Serial.print(" Now: ");
      Serial.print(now);
      Serial.print(" timeOfLastNote: ");
      Serial.print(timeOfLastNote[i]);
      Serial.print(" timeSinceLastNote: ");
      Serial.println(timeSinceLastNote);
      #endif

    }
  }

  #ifdef SERIAL_PLOT_MODE
  bool constantlyDraw = true;
  for (int i=0;i<CHANNELS;i++) {
    if (adcValue[i] > MIN_THRESHOLD) {
      constantlyDraw = true;
    }
  }
  if (constantlyDraw) {
    Serial.print ("s:");
    Serial.print ("1023");
    Serial.print (",m:");
    Serial.print (missedSamples);
    Serial.print (",c:"); 
    Serial.print (countedSamples);
    for (int i=0;i<CHANNELS;i++) {
      Serial.print (",P");
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

void digitalPinScan() {
  for (int i=0;i<DIGITAL_INPUTS;i++) { // because i hate when other people write this kind of code, i'll put comments for future me
    if (digitalRead(digitalPins[i]) == !buttonPressed[i]) { // if the pin state doesn't match the stored state
      handleDigitalInput(digitalPins[i], buttonPressed[i]); // then send the opposite (ie the same as pin state without reading it again)
      buttonPressed[i] = !buttonPressed[i]; // and flip it
    }
  }
}

void handleDigitalInput(byte pin, bool pressed) {
  if (pin == KICK_INPUT_PIN && pressed) {
    noteFireLinearVelocity(KICK_NOTE,80);
  }
  else if (pin == HIT_HAT_INPUT_PIN && pressed) {
    noteFireLinearVelocity(HI_HAT_CLOSED_NOTE,80);
  }
  else if (hiHatPedalMakesOpeningSounds && pin == HIT_HAT_INPUT_PIN && !pressed) {
    noteFireLinearVelocity(HI_HAT_OPEN_NOTE,80);
  }
}

#ifdef DEBUG
void noteDebug(byte note, byte velocity)
{
  Serial.print("Note: ");
  Serial.print(note);
  Serial.print(" Velocity: ");
  Serial.print(velocity);
  Serial.println();
}
#endif

#define VELOCITY_FACTOR 1.5
// a VELOCITY_FACTOR greater than one removes dynamic range and sets a higher low limit
// less than one burns your house down and increases dynamic range. you don't want this.
// 1 makes the range limits 0-127
// 2 makes the range limits 63-127
// 3 makes the range limits 85-127

byte correctVelocityCurve(byte velocity) {
  return (velocity / VELOCITY_FACTOR) + (MAX_MIDI_VELOCITY - (MAX_MIDI_VELOCITY / VELOCITY_FACTOR));
}

void handleAnalogEvent(byte note, byte velocity) {
  if (note == HI_HAT_OPEN_NOTE && buttonPressed[1] == false) {
      note = HI_HAT_CLOSED_NOTE;
  }
  noteFire(note, velocity);
}

void noteFire(byte note, byte velocity)
{
  noteFireLinearVelocity(note, correctVelocityCurve(velocity));
}

void noteFireLinearVelocity(byte note, byte velocity)
{
  midiNoteOn(note, velocity);
  midiNoteOff(note, velocity);
  #ifdef DEBUG
  noteDebug(note, velocity);
  #endif
}

void midiNoteOn(byte note, byte midiVelocity)
{
  digitalWrite(LED_BUILTIN, HIGH);
  midiSerial.write(NOTE_ON_CMD);
  midiSerial.write(note);
  midiSerial.write(midiVelocity);
  
}

void midiNoteOff(byte note, byte midiVelocity)
{
  midiSerial.write(NOTE_OFF_CMD);
  midiSerial.write(note);
  #ifdef SEND_NOTE_OFF_VELOCITY 
  midiSerial.write(midiVelocity); // unread on the MT-80s
  #endif
}
