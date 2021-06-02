/*  2018-04-04
 *   By Pedro
 */

#include "lin_slave.h" 
//#include <Bounce2.h>
#include <SoftwareSerial.h>    

// standard Arduino settings
#define TX_PIN 1        
#define RX_PIN 0        
#define led1Pin 8
#define CS 4        // HIGH enables MCP2003 chip. Allows LIN to Ready mode? (per LIN protocol, Ready mode Vbb>5.5V, rx on , tx off => Ready mode)

Lin lin(Serial, TX_PIN);   

volatile const int Rx = 2;           // Rx looped from Rx pin. RxD pin follows the state of the LIN. interruptPin!!!
volatile boolean clearToSend = true; // clearToSend is used to flag that we shouldn't try to send anything as the LIN bus is busy
unsigned long int timeout = 90*14;
char message[3]; 
uint8_t bytesRcvd = 0;
SoftwareSerial debugSerial(18, 19); // RX, TX

void setup()                     
{ 
  lin.begin(19200);
  debugSerial.begin(19200);
  pinMode (CS, OUTPUT);       // initialize pin.
  pinMode(Rx, INPUT_PULLUP);  // pin looped from Rx pin 0
  digitalWrite (CS, HIGH);    // write pin high.
  
  pinMode(led1Pin, OUTPUT);
  digitalWrite(led1Pin, LOW); 
  
  busClear();
} 

unsigned long t = 0;
void delay_until(unsigned long ms) 
{
  unsigned long end = t + (1000 * ms);
  unsigned long d = end - micros();

  if ( d > 1000000 ) 
  {
    t = micros();
    return;
  }
  
  if (d > 15000) 
  {
    unsigned long d2 = (d-15000)/1000;
    delay(d2);
    d = end - micros();
  }
  delayMicroseconds(d);
  t = end;
}

void loop()
{
  unsigned int timeoutCount=0;
   if(Serial.available()==3)
   {
     for (uint8_t i=0; i<3; i++)
     {
      message[i] = Serial.read(); bytesRcvd++;
     }
     
     debugSerial.print(bytesRcvd); bytesRcvd=0;
     debugSerial.println(" messages received");
     for(int i=0; i<20; i++)
     {
      debugSerial.print(message[i],HEX);
      debugSerial.print(","); 
     }debugSerial.println();
     Serial.end();
     Serial.begin(19200);
   }
}

void busClear() 
{
#define START_TIMER1 TCCR1B |= (1 << CS10)|(1 << CS12) // Set CS10 and CS12 bits for 1024 prescaler:
#define STOP_TIMER1  TCCR1B &= 0B11111000              // CS12=CS11=CS10=0 --> no clock source (Timer/Counter) stopped

  // initialize Timer1
  TCCR1A = 0;     // set entire TCCR1A register to 0
  TCCR1B = 0;     // same for TCCR1B

  // set compare match register to desired timer count:
  //OCR1A = 157; // Set timer to fire CTC interrupt after approx 10ms
  OCR1A = 15624; // This is the value that seems to be correct

  // turn on CTC mode:
  TCCR1B |= (1 << WGM12);

  // enable timer compare interrupt:
  TIMSK1 |= (1 << OCIE1A);

  attachInterrupt(0, rxChange, CHANGE); // run rxChange every time serial Rx changes state
  //attachInterrupt(digitalPinToInterrupt(Rx), rxChange, CHANGE);
}

 

 /*!**********************************************************************************************
  * Here the guys are PE0=Tx and PE1=Rx (check the Arduino pinout diagram)
  ************************************************************************************************/
void rxChange() 
{
  if (PINE & (1<<PINE1) && (PINE & (1<<PINE0))) // check Tx high and Rx high
  { 
    START_TIMER1;
  }
  else if (PINE & (1<<PINE1) &&  (!(PINE & (1<<PINE0)))) // check Tx high and Rx low
  { 
    clearToSend = false;
    digitalWrite(led1Pin, HIGH);
    STOP_TIMER1;
  }
}


ISR(TIMER1_COMPA_vect) // runs if timer runs for 10ms
{
  clearToSend = true;
  digitalWrite(led1Pin, LOW); // era LOW

  STOP_TIMER1;
}




