const byte adcPin0 = 0;  // A0
const byte adcPin1 = 1;  // A1
const byte adcPin2 = 2;  // A2
const byte adcPin3 = 3;  // A3

const int MAX_RESULTS = 128;

volatile int counter;

volatile int results [MAX_RESULTS];
volatile int results1 [MAX_RESULTS];
volatile int results2 [MAX_RESULTS];
volatile int results3 [MAX_RESULTS];

volatile int resultNumber;
volatile int resultNumber1;
volatile int resultNumber2;
volatile int resultNumber3;

volatile int currentPin = 0;

volatile boolean inLoop;
// ADC complete ISR
ISR (ADC_vect)
  {
    if (resultNumber >= MAX_RESULTS) {
      Serial.println("BUFFER OVERRUN");
      return;
    }
//    ADCSRA = 0;  // turn off ADC
//  else
    if (inLoop)
      return;
    switch(currentPin) {
    case 0:
      results [resultNumber++] = ADC;
      ADMUX = bit (REFS0) | (adcPin1 & 7);
      currentPin = 1;
      break;
    case 1:
    counter++;
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
    
  
  }  // end of ADC_vect
  
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
  ADMUX = bit (REFS0) | (adcPin0 & 7);
  ADCSRB = bit (ADTS0) | bit (ADTS2);  // Timer/Counter1 Compare Match B
  ADCSRA |= bit (ADATE);   // turn on automatic triggering

  // wait for buffer to fill
//  while (resultNumber < MAX_RESULTS)
//    { }
//    
//  for (int i = 0; i < MAX_RESULTS; i++)
//    Serial.println (results [i]);
  
}  // end of setup

void loop () {
  go();
}
const int displayPin = 1;

int adcValue [4];
void go () {
  inLoop = true;
  int max = 0;
//  Serial.println (results[0]);
  for (int i = 0; i < MAX_RESULTS; i++) {
    if (results[i] > max)
      max = results[i];
    results[i] = 0;
  }
  adcValue[0] = max;
  max = 0;
//  Serial.println (results[0]);
  for (int i = 0; i < MAX_RESULTS; i++) {
    if (results1[i] > max)
      max = results1[i];
    results1[i] = 0;
  }
  adcValue[1] = max;
  max = 0;
//  Serial.println (results[0]);
  for (int i = 0; i < MAX_RESULTS; i++) {
    if (results2[i] > max)
      max = results2[i];
    results2[i] = 0;
  }
  adcValue[2] = max;
  max = 0;
//  Serial.println (results[0]);
  for (int i = 0; i < MAX_RESULTS; i++) {
    if (results3[i] > max)
      max = results3[i];
    results3[i] = 0;
  }
  adcValue[3] = max;

//  Serial.println(resultNumber);
//  Serial.println(counter);
  Serial.println (adcValue[displayPin]);
  
  resultNumber = resultNumber1 = resultNumber2 = resultNumber3 = 0;

  
  inLoop = false;
  
/* This delay is very specific to the frequency of the pieze sensor being monitored.
// Frequencies below this minimum are not guarenteed to register.
// A lower minimum increases latency.
*/
#define MINIMUM_FREQUENCY 190

  delay(1000/MINIMUM_FREQUENCY);

  counter = 0;
  
 
}
