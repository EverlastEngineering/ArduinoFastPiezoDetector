#define SERIAL_PLOT_MODE
#define MIN_THRESHOLD 150
#define ANALOGPINS 6

byte adcPin[ANALOGPINS];

const byte CHANNELS = 6;
volatile int maxResults[CHANNELS];

volatile int counter = 0;
volatile int missedSamples = 0;

byte currentPin = 0;
int adcValue[CHANNELS];

volatile boolean inLoop;

EMPTY_INTERRUPT (TIMER1_COMPB_vect);
 
void setup ()
  {
  Serial.begin (115200);
  Serial.println ();
  
  // reset Timer 1
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  TCCR1B = bit (CS11) | bit (WGM12);  // CTC, prescaler of 8
  TIMSK1 = bit (OCIE1B);  // WTF?
  OCR1A = 39;    
  OCR1B = 39;   // 20 uS - sampling frequency 50 kHz

  ADCSRA =  bit (ADEN) | bit (ADIE) | bit (ADIF);   // turn ADC on, want interrupt on completion
  ADCSRA |= bit (ADPS2);  // Prescaler of 16
  ADMUX = bit (REFS0) | (adcPin[0] & 7);
  ADCSRB = bit (ADTS0) | bit (ADTS2);  // Timer/Counter1 Compare Match B
  ADCSRA |= bit (ADATE);   // turn on automatic triggering

  for (int i=0;i<CHANNELS;i++) {
    maxResults[i]=0;
  }

  // This relies on A0 being 0, etc, which on the UNO is true. Mapped manually for clarity.
  adcPin[0]=0;  
  adcPin[1]=1;
  adcPin[2]=2;
  adcPin[3]=3;
  adcPin[4]=4;
  adcPin[5]=5;
} 

const byte DISCARD = 1;
byte discardCounter = 0;

// ADC complete ISR
ISR (ADC_vect) {
    if (inLoop) {
      #ifdef SERIAL_PLOT_MODE
        missedSamples++;
      #endif
      return;
    }
    
    if (discardCounter == DISCARD) {
      currentPin++;
      discardCounter = 0;
    }
    else {
      discardCounter++;
      return;
    }

    #ifdef SERIAL_PLOT_MODE
      counter++; 
    #endif
    
    if (currentPin == CHANNELS)
      currentPin = 0;

    if (ADC > maxResults[currentPin])
      maxResults[currentPin] = ADC;
    
    byte pin = currentPin+1;
    pin = pin%CHANNELS; // <-- yeah! if the pin is higher than CHANNELS, then "pin MOD CHANNELS" loops it back to 0. 
    ADMUX = bit (REFS0) | (pin & 7);
    
    // Interestingly, if you combine the (currentPin+1)&CHANNELS on any line, there is often a failure incrementing in time. There must be a compiler optimization that fails.
}
  
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
  for (int i = 0; i<CHANNELS; i++) {
  if (adcValue[i] > MIN_THRESHOLD)
    {
      Serial.print ("Pin");
      Serial.print (i);
      Serial.print (":");
      Serial.print(adcValue[i]);
      Serial.print (",");
      Serial.println();
    }
  }

#else
// Min / Max for scaling the graph
  Serial.print ("0");
  Serial.print (",80");
//  Serial.print (",MissedSamples:");
//  Serial.print (missedSamples);
  Serial.print (",Counter:"); 
  Serial.print (counter);
  Serial.print (",Pin0:");
  Serial.print (adcValue[0]);
  Serial.print (",Pin1:");
  Serial.print (adcValue[1]);
  Serial.print (",Pin2:");
  Serial.print (adcValue[2]);
  Serial.print (",Pin3:");
  Serial.print (adcValue[3]);
  Serial.println ();
  counter = missedSamples = 0;
#endif

  
/*  This delay ensures enough time to prevent aliasing. 
 *  For example:
 *  If counter == 50000 per second, and CHANNELS == 6, then you'll get 8333 samples per second.
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
  
  delay(10);
}
