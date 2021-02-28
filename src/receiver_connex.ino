// scanner_connex

#ifdef RECEIVER

#include "Arduino.h"
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "attack.h"

#define CE CE_GPIO
#define CSN CSN_GPIO
#define PKT_SIZE 37
#define PAY_SIZE 32
#define MICROSOFT 1
#define LOGITECH 2
#define LED 2

RF24 radio(CE, CSN);

unsigned long startTime;
int ledpin = LED;

/**
 * Connex message structure
 * pipe                 - pipe
 * zonea                - 2 bytes Appears to change for a each zone
 * sequence             - message sequence number
 * unknown1             - unknown byte field, typically 1
 * zone                 - zone for the message
 * targetTemperature_C  - temperature setting in celcius, / 10 to obtain
 * length               - Guessing 9 bytes
 * crc                  - Guessing 2 byte crc value
 * 
 * 9A BE B1 46 1 3 0 0 9 1 0 BB A7 D6 D5 EA BC 55 2A A8 24 9C 13 8B 67 B2 45 4E FD BF AE BD 0 0 0 0 0
 */
/*
typedef struct connex {
  uint8_t pipe;
  uint16_t zonea;
  uint8_t sequence;
  uint8_t unknown1;
  uint8_t zone;
  uint16_t targetTemperature_C;
  uint8_t length;
  uint8_t unknown2;
  uint8_t zero;
  uint16_t crc;
  uint8_t buf[PKT_SIZE];
} connex_t;
*/
typedef struct connex
{
  uint8_t buf[PKT_SIZE];
} connex_t;

/**  0xAALL;
 *  852 A0 92 70 4E 2D 9A
 *  886 A0 92 70 4E 2D 9E
 *  978 A0 92 70 4E 2D 9C
*  1007 A0 92 70 4E 2D 98
*/
// uint64_t promisc_addr = 0xAALL;
// uint64_t promisc_addr = 0x9270LL; // working
// uint64_t promisc_addr = 0x4E2DLL; // working
uint64_t promisc_addr = 0x92704E2DLL; // working
uint8_t channel = 40;
uint64_t address;
uint8_t payload[PAY_SIZE];
uint8_t payload_size;
bool payload_encrypted = false;
uint8_t payload_type = 0;
uint16_t sequence;

const uint8_t num_channels = 40;
uint8_t values[num_channels + 1];

uint8_t writeRegister(uint8_t reg, uint8_t value)
{
  uint8_t status;

  digitalWrite(CSN, LOW);
  status = SPI.transfer(W_REGISTER | (REGISTER_MASK & reg));
  SPI.transfer(value);
  digitalWrite(CSN, HIGH);
  return status;
}

uint8_t writeRegister(uint8_t reg, const uint8_t *buf, uint8_t len)
{
  uint8_t status;

  digitalWrite(CSN, LOW);
  status = SPI.transfer(W_REGISTER | (REGISTER_MASK & reg));
  while (len--)
    SPI.transfer(*buf++);
  digitalWrite(CSN, HIGH);

  return status;
}

// Update a CRC16-CCITT with 1-8 bits from a given byte
uint16_t crc_update(uint16_t crc, uint8_t byte, uint8_t bits)
{
  crc = crc ^ (byte << 8);
  while (bits--)
    if ((crc & 0x8000) == 0x8000)
      crc = (crc << 1) ^ 0x1021;
    else
      crc = crc << 1;
  crc = crc & 0xFFFF;
  return crc;
}

void reset()
{
  payload_type = 0;
  payload_encrypted = false;
  payload_size = 0;
  for (int i = 0; i < PAY_SIZE; i++)
  {
    payload[i] = 0;
  }
  radio.begin();
}

void radioReceiveSetup()
{
  Serial.println("Configuring receiver...");

  radio.begin();
  radio.setAutoAck(false);
#ifdef KBS250
  writeRegister(RF_SETUP, 0x21); // Disable PA, 250kbs rate, LNA enabled
#endif
#ifdef MBS1
  writeRegister(RF_SETUP, 0x01); // Disable PA, 1M rate, LNA enabled
#endif
#ifdef MBS2
  writeRegister(RF_SETUP, 0x09); // Disable PA, 2M rate, LNA enabled
#endif
  radio.setPayloadSize(PKT_SIZE);
  radio.setChannel(channel);
  // RF24 doesn't ever fully set this -- only certain bits of it
  writeRegister(EN_RXADDR, 0x00);
  // RF24 doesn't have a native way to change MAC...
  // 0x00 is "invalid" according to the datasheet, but Travis Goodspeed found it works :)
  writeRegister(SETUP_AW, 0x02);
  uint64_t promisc_addr0 = 0xAALL;
  uint64_t promisc_addr1 = 0x92704E2DLL; // working
  radio.openReadingPipe(0, promisc_addr0);
  radio.openReadingPipe(1, promisc_addr1);
  radio.disableCRC();
  radio.startListening();
  radio.stopListening();
  radio.printPrettyDetails();
  radio.startListening();
  Serial.println("Configuring receiver complete.");
}

void radioReceiveLoop()
{
  int x;
  //  uint8_t buf[PKT_SIZE];
  connex_t buffer;
  //  unsigned long wait = 100;
  uint8_t payload_length;
  uint16_t crc, crc_given;
  digitalWrite(ledpin, LOW);

  uint8_t pipe; // initialize pipe data
  if (radio.available(&pipe))
  {
    digitalWrite(ledpin, HIGH);
    radio.read(&buffer, sizeof(buffer));

    Serial.print(millis());
    Serial.print(" ");
    Serial.print(pipe);
    /*
    Serial.print(": pi: ");
    Serial.print(buffer.pipe, HEX);
    Serial.print(" za: ");
    Serial.print(buffer.zonea, HEX);
    Serial.print(" sq: ");
    Serial.print(buffer.sequence, HEX);
    Serial.print(" u1: ");
    Serial.print(buffer.unknown1, HEX);
    Serial.print(" z: ");
    Serial.print(buffer.zone);
    Serial.print(" tt: ");
    Serial.print(buffer.targetTemperature_C/10);
    Serial.print(" len: ");
    Serial.print(buffer.length);
    Serial.print(" crc: ");
    Serial.print(buffer.crc);
    */
    Serial.print(" buf: ");

    for (int j = 0; j < PKT_SIZE; j++)
    {
      Serial.print(buffer.buf[j], HEX);
      Serial.print(" ");
    }
    Serial.println("");

    // Read the given CRC
    crc_given = (buf[6 + payload_length] << 9) | ((buf[7 + payload_length]) << 1);
    crc_given = (crc_given << 8) | (crc_given >> 8);
    if (buf[8 + payload_length] & 0x80)
      crc_given |= 0x100;

    // Calculate the CRC
    crc = 0xFFFF;
    for (x = 0; x < 6 + payload_length; x++)
      crc = crc_update(crc, buf[x], 8);
    crc = crc_update(crc, buf[6 + payload_length] & 0x80, 1);
    crc = (crc << 8) | (crc >> 8);
  }
}

void setup()
{
  Serial.begin(921600);
  pinMode(ledpin, OUTPUT);
  digitalWrite(ledpin, LOW);
  radioReceiveSetup();
}

void loop()
{
  //  reset();
  radioReceiveLoop();
  //  scan();
  //  fingerprint();
  // launch_attack();
}

#endif