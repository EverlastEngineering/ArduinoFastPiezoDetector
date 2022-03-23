const byte adcPin0 = 0;  // A0
const byte adcPin1 = 1;  // A1
const byte adcPin2 = 2;  // A2
const byte adcPin3 = 3;  // A3

const int MAX_RESULTS = 128;

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
    if (resultNumber1 >= MAX_RESULTS) {
      skippedSamples++;
      return;
    }

    switch(currentPin) {
      case 0:
        results [resultNumber++] = ADC;
        ADMUX = bit (REFS0) | (adcPin1 & 7);
        currentPin = 1;
        break;
      case 1:     
          results1 [resultNumber1++] = ADC;
        ADMUX = bit (REFS0) | (adcPin2 & 7);
        currentPin = 2;
        break;
      case 2:
        results2 [resultNumber2++] = ADC;
        ADMUX = bit (REFS0) | (adcPin3 & 7);
        currentPin = 3;
        break;
      case 3:
        results3 [resultNumber3++] = ADC;
        ADMUX = bit (REFS0) | (adcPin0 & 7);
        currentPin = 0;
        break;
    }
}
  
EMPTY_INTERRUPT (TIMER1_COMPB_vect);


void loop () {
  int result;
  int samples;
  
  inLoop = true;

  // tests show this loop causes ~43 missedSamples. The number of samples skipped is less than the number sampled OUTSIDE this loop, so an alternating set of buffers would
  // recover these otherwise lost samples, but we don't have enough memory to have a complete second set of buffers

  for (int i = 0; i < resultNumber1; i++) {
    if (results[i] > adcValue[0])
      adcValue[0] = results[i];
    results[i] = 0;

    if (results1[i] > adcValue[1])
      adcValue[1] = results1[i];
    results1[i] = 0;

    if (results2[i] > adcValue[2])
        adcValue[2] = results2[i];
    results2[i] = 0;
    
    if (results3[i] > adcValue[2])
        adcValue[3] = results3[i];
    results3[i] = 0;
  }

  samples = resultNumber1;
  result = adcValue[1];
  resultNumber = resultNumber1 = resultNumber2 = resultNumber3 = 0;
  inLoop = false;

  Serial.print (" MissedSamples: ");
  Serial.print (missedSamples);
  Serial.print (" SkippedSamples: ");
  Serial.print (skippedSamples);
  Serial.print (" Counter: ");
  Serial.print (counter);
  Serial.print (" Samples: ");
  Serial.print (samples);
  Serial.print (" Result on display pin: ");
  Serial.print (result);
  Serial.println ();
  
  counter = missedSamples = skippedSamples = 0;
  
/* This delay is very specific to the frequency of the pieze sensor being monitored.
// Frequencies below this minimum are not guarenteed to register.
// A lower minimum increases latency.
// Keep this delay low enough that the buffers will not overfill, ie, skippedSamples remains 0.
*/
#define MINIMUM_FREQUENCY 190
  delay(1000/MINIMUM_FREQUENCY);
  
}
