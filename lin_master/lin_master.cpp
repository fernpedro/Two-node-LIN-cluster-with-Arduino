
#include "Arduino.h"
#include "lin_master.h"

Lin::Lin(LIN_SERIAL& ser,uint8_t TxPin):serial(ser), txPin(TxPin){ }

void Lin::begin(int speed)
{
  serialSpd = speed;
  serial.begin(serialSpd);
  serialOn  = 1;

  // source: https://vector.com/portal/medien/cmc/events/Webinars/2014/Vector_Webinar_IntroductionToLIN_20140930_EN.pdf
  // vector.com, LIN seminarium, pages 21,22.
  unsigned long int Tbit = 100000/serialSpd;  // Tbit = 1/Baud rate.
                                              // Not quite in uSec, I'm saving an extra 10 to change a 1.4 (40%) to 14 below...
  unsigned long int nominalFrameTime = ((34*Tbit)+90*Tbit);  // 90 = 10*max # payload bytes + checksum (9). 
  timeout = LIN_TIMEOUT_IN_FRAMES * 14 * nominalFrameTime;  // 14 is the specced addtl 40% space above normal*10 -- the extra 10 is just pulled out of the 1000000 needed to convert to uSec (so that there are no decimal #s).
}

// Generate a BREAK signal (a low signal for longer than a byte) across the serial line
void Lin::serialBreak(void)
{
  if (serialOn) 
  {
    serial.flush();
    serial.end();
  }

/* 
 *  If you prefer to work in frequency rather than in period you can use the reciprocal relation between 
 *  frequency and period; period is equal to 1 divided by frequency. You need the period value in microseconds
 *  because there are 1 million microseconds in one second, the period is calculated as 1000000L./frequency(where
 *  the L and the end of that number tells the compiller that it should calculate using long integer math to 
 *  prevent the calculation from exceeding the range of a normal integer. Source: Arduino cookbok)
 */

  pinMode(txPin, OUTPUT);
  digitalWrite(txPin, LOW);  // Send BREAK
  //const unsigned long int brkend = (1000000UL/((unsigned long int)serialSpd)); // brkend = (serialSpd[kBit/s])^-1= 52.1 microSeconds 
  //const unsigned long int brkbegin = brkend*LIN_BREAK_DURATION;
  const unsigned long int brkend = 52; //(1000000UL/((unsigned long int)serialSpd)); // brkend = (serialSpd[kBit/s])^-1= 52.1 microSeconds 
  const unsigned long int brkbegin = 780; // brkend*LIN_BREAK_DURATION;
  //if (brkbegin > 16383) delay(brkbegin/1000);  // for delays larger than a few thousand microseconds use delay() which pauses the program in milliSeconds
  //else 
  delayMicroseconds(brkbegin); // delayMicroseconds() pauses the program. The lagest value that will produce and accurate delay is 16383
                                   
  digitalWrite(txPin, HIGH);  // BREAK delimiter
  //if (brkend > 16383) delay(brkend/1000); 
  //else 
  delayMicroseconds(brkend);

  serial.begin(serialSpd);
  serialOn = 1;
}

/* Lin defines its checksum as an inverted 8 bit sum with carry */
uint8_t Lin::dataChecksum(const uint8_t* message, char nBytes,uint16_t sum)
{
    while (nBytes-- > 0) sum += *(message++);
    // Add the carry
    while(sum>>8)  // In case adding the carry causes another carry
      sum = (sum&255)+(sum>>8); 
    return (~sum);
}

/* Create the Lin ID parity */
#define BIT(data,shift) ((addr&(1<<shift))>>shift)
uint8_t Lin::addrParity(uint8_t addr)
{
  uint8_t p0 = BIT(addr,0) ^ BIT(addr,1) ^ BIT(addr,2) ^ BIT(addr,4);
  uint8_t p1 = ~(BIT(addr,1) ^ BIT(addr,3) ^ BIT(addr,4) ^ BIT(addr,5));
  return (p0 | (p1<<1))<<6;
}

/* Send a message across the Lin bus */
void Lin::send(uint8_t addr, const uint8_t* message, uint8_t nBytes, uint8_t proto)// agregamos proto
{
  uint8_t addrbyte = (addr&0x3f) | addrParity(addr);
  uint8_t cksum = dataChecksum(message,nBytes,(proto==1) ? 0:addrbyte);
  serialBreak();       // Generate the low signal that exceeds 1 char !!!!!!!
  //serial.print(0x55,HEX); serial.print(addrbyte,HEX); serial.print(cksum);  // cksum=255, means transmission OK
  serial.write(0x55);            // Sync byte
  serial.write(addrbyte);        // ID byte
  serial.write(message,nBytes);  // data bytes
  serial.write(cksum);           // checksum
  serial.flush();
}

uint8_t Lin::recv(uint8_t addr, uint8_t* message, uint8_t nBytes, uint8_t proto)
{
  uint8_t bytesRcvd=0;
  unsigned int timeoutCount=0;
  serialBreak();       // Generate the low signal that exceeds 1 char.
  serial.flush();
  serial.write(0x55);  // Sync byte
  uint8_t idByte = (addr&0x3f) | addrParity(addr);
  serial.write(idByte);  // ID byte
  bytesRcvd = 0xfd;
  do { // I hear myself
    while(!serial.available()) { delayMicroseconds(100); timeoutCount+= 100; if (timeoutCount>=timeout) goto done; }
  } while(serial.read() != 0x55);
  bytesRcvd = 0xfe;
  do {
    while(!serial.available()) { delayMicroseconds(100); timeoutCount+= 100; if (timeoutCount>=timeout) goto done; }
  } while(serial.read() != idByte);


  bytesRcvd = 0;
  for (uint8_t i=0;i<nBytes;i++)
  {
    // This while loop strategy does not take into account the added time for the logic.  So the actual timeout will be slightly longer then written here.
    while(!serial.available()) 
    { 
      delayMicroseconds(100); 
      timeoutCount+= 100; 
      if (timeoutCount>=timeout) goto done; 
    } 
    message[i] = serial.read();
    bytesRcvd++;
  }
  while(!serial.available()) 
  { 
    delayMicroseconds(100); 
    timeoutCount+= 100; 
    if (timeoutCount>=timeout) goto done; 
  }
  if (serial.available())
  {
    uint8_t cksum = serial.read();
    bytesRcvd++;
    //if (proto==1) idByte = 0;  // Don't cksum the ID byte in LIN 1.x
    if (dataChecksum(message,nBytes,idByte) == cksum) bytesRcvd = 0xff;
  }

done:
  serial.flush();

  return bytesRcvd;
}
