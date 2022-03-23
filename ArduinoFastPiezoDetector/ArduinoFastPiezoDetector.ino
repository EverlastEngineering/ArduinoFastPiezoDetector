const byte adcPin0 = 0;  // A0
const byte adcPin1 = 1;  // A1
const byte adcPin2 = 2;  // A2
const byte adcPin3 = 3;  // A3

const int MAX_RESULTS = 1;

volatile int counter = 0;
volatile int missedSamples = 0;
volatile int skippedSamples = 0;

volatile int results [MAX_RESULTS];
volatile int results1 [MAX_RESULTS];
volatile int results2 [MAX_RESULTS];
volatile int results3 [MAX_RESULTS];

volatile int resultNumber;
volatile int resultNumber1;
volatile int resultNumber2;
volatile int resultNumber3;

volatile int currentPin = 1;
const int displayPin = 1;
int adcValue [4];

volatile boolean inLoop;

 
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
  ADMUX = bit (REFS0) | (adcPin0 & 7);
  ADCSRB = bit (ADTS0) | bit (ADTS2);  // Timer/Counter1 Compare Match B
  ADCSRA |= bit (ADATE);   // turn on automatic triggering
} 

// ADC complete ISR
ISR (ADC_vect) {
    counter++; 
    if (inLoop) {
      missedSamples++;
      return;
    }

    switch(currentPin) {
      case 0:
        if (ADC > results [0])
          results [0] = ADC;
        ADMUX = bit (REFS0) | (adcPin1 & 7);
        currentPin = 1;
        break;
      case 1:     
        if (ADC > results1 [0])
          results1 [0] = ADC;
        ADMUX = bit (REFS0) | (adcPin2 & 7);
        currentPin = 2;
        break;
      case 2:
        if (ADC > results2 [0])
          results2 [0] = ADC;
        ADMUX = bit (REFS0) | (adcPin3 & 7);
        currentPin = 3;
        break;
      case 3:
        if (ADC > results3 [0])
          results3 [0] = ADC;
        ADMUX = bit (REFS0) | (adcPin0 & 7);
        currentPin = 0;
        break;
    }
}
  
EMPTY_INTERRUPT (TIMER1_COMPB_vect);


void loop () {
  
  inLoop = true;

  // tests show this loop causes ~43 missedSamples. The number of samples skipped is less than the number sampled OUTSIDE this loop, so an alternating set of buffers would
  // recover these otherwise lost samples, but we don't have enough memory to have a complete second set of buffers
  //  original looping
  // MissedSamples: 43 SkippedSamples: 0 Counter: 342 Samples: 92 Result on display pin: 361
  // 
  
  // on-the-fly-averaging
  // MissedSamples: 0  SkippedSamples: 0 Counter: 289 Samples: 92 Result on display pin: 3
  // 12500 alternating samples per channel with 4 channels



    adcValue[0] = results[0];
    adcValue[1] = results1[0];
    adcValue[2] = results2[0];
    adcValue[3] = results3[0];
    results[0] = results1[0] = results2[0] = results3[0] = 0;
  inLoop = false;

#define MIN_THRESHOLD 150

  for (int i = 0; i<2; i++) {
  if (adcValue[i] > MIN_THRESHOLD)
    {
      Serial.print ("Pin ");
      Serial.print (i);
      Serial.print (": ");
      Serial.println(adcValue[i]);
      
    }
  }

//  Serial.print (" MissedSamples: ");
//  Serial.print (missedSamples);
//  Serial.print ("Counter:");
//  Serial.print (counter);
//  Serial.print (",Pin0:");
//  Serial.print (adcValue[0]);
//  Serial.print (",Pin1:");
//  Serial.print (adcValue[1]);
//  Serial.print (",Pin2:");
//  Serial.print (adcValue[2]);
//  Serial.print (",Pin3:");
//  Serial.print (adcValue[3]);
//  Serial.println ();
  
  counter = missedSamples = 0;
  
/* This delay is very specific to the frequency of the pieze sensor being monitored.
// Frequencies below this minimum are not guarenteed to register.
// A lower minimum increases latency.
// Keep this delay low enough that the buffers will not overfill, ie, skippedSamples remains 0.
*/
#define MINIMUM_FREQUENCY 190
  delay(10);
  
}
