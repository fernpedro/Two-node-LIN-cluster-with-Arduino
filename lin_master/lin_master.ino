/*  2018-04-04
  By Pedro
 */

#include "lin_master.h" 
#include <Bounce2.h>    

// standard Arduino settings
#define TX_PIN 1        
#define RX_PIN 0        
#define ledPin 13
#define up_button 7
#define down_button 8
#define CS 4        // HIGH enables MCP2003 chip. Allows LIN to Ready mode? (per LIN protocol, Ready mode Vbb>5.5V, rx on , tx off => Ready mode)

Lin lin(Serial, TX_PIN);   

// instantiate Bounce object
Bounce debouncer_down_button = Bounce(); 
Bounce debouncer_up_button = Bounce();

volatile const int Rx = 2;           // / Rx(Arduino interrupt pin 2)looped from RxD pin transceiver whichs follows the state of the LIN.
volatile boolean clearToSend = true; // clearToSenad is used to flag that we shouldn't try to send anything as the LIN bus is busy

void setup()                     
{ 
  lin.begin(19200);
  //Serial.begin(9600);
  pinMode (CS, OUTPUT);       // initialize pin.
  pinMode(Rx, INPUT_PULLUP);  // pin looped from Rx pin 0
  digitalWrite (CS, HIGH);    // write pin high.
  pinMode(down_button, INPUT_PULLUP);
  pinMode(up_button, INPUT_PULLUP);

  // After setting up the button, setup the Bounce instance :
  debouncer_down_button.attach(down_button);
  debouncer_down_button.interval(5); // interval in ms

  debouncer_up_button.attach(up_button);
  debouncer_up_button.interval(5); // interval in ms
  
  pinMode(ledPin, OUTPUT);
  //digitalWrite(ledPin, LOW);  
  
  busClear();
} 

void loop()
{
   uint8_t message1[] = {1, 2, 3}; 
   uint8_t message2[] = {1, 1, 1};
  // update the bounce instanceS
   debouncer_down_button.update();
   debouncer_up_button.update();
   // Get the updated value :
  int value1 = debouncer_down_button.read();
  int value2 = debouncer_up_button.read();  
  
  //Serial.println(value);
   
   // https://github.com/thomasfredericks/Bounce2/blob/master/examples/bounce2buttons/bounce2buttons.ino
   if(value1== LOW && clearToSend == true && debouncer_down_button.fell())
   {
     lin.send(0x11, message1, 3, 2);

   }
   else if(value2== LOW && clearToSend == true && debouncer_up_button.fell())
   {
    lin.send(0x09,message2,3);  //********************* falta un parameter
   }
}

/*!***********************************************************************************************
 * 
 * Here we need to monitor the RxD line and only send if it has been HIGH(idle) for more than 10ms
 * We do it by looping the RxD pin into pin2 and use an interrupt to start a timer. If the RxD line goes 
 * LOW before the timer has finish counting it resets until the RxD line goes HIGH again. Only when the
 * RxD line goes HIGH and the timer is allowed to finish, we set a flag that says it's clear to send
 **************************************************************************************************/


void busClear() 
{
#define START_TIMER1 TCCR1B |= (1 << CS10)|(1 << CS12) // Set CS10 and CS12 bits for 1024 prescaler:
#define STOP_TIMER1  TCCR1B &= 0B11111000              // CS12=CS11=CS10=0 --> no clock source (Timer/Counter) stopped

  // initialize Timer1
  TCCR1A = 0;     // set entire TCCR1A register to 0
  TCCR1B = 0;     // same for TCCR1B

  // set compare match register to desired timer count:
  OCR1A = 157; // Set timer to fire CTC interrupt after approx 10ms

  // turn on CTC mode:
  TCCR1B |= (1 << WGM12);

  // enable timer compare interrupt:
  TIMSK1 |= (1 << OCIE1A);

  attachInterrupt(0, rxChange, CHANGE); // run rxChange every time serial Rx changes state
}                                       // The Arduino board external interrupt 0 (Arduino digital pin 2) is hooked up to Rxd on the transceiver


 /*!**********************************************************************************************
  * Here the guys are PE0=Tx and PE1=Rx (check the Arduino pinout diagram)
  ************************************************************************************************/
void rxChange() 
{
  if (PINE & (1<<PINE1) && (PINE & (1<<PINE0))) // check Tx high and Rx high
  { 
    START_TIMER1;
  }
  else if (PINE & (1<<PINE1) && (!(PINE & (1<<PINE0)))) // check Tx high and Rx low
  { 
    clearToSend = false;
    digitalWrite(ledPin, HIGH);
    STOP_TIMER1;
  }
}


ISR(TIMER1_COMPA_vect) // runs if timer runs for 10ms
{
  clearToSend = true;
  digitalWrite(ledPin, LOW); 
  STOP_TIMER1;
}


