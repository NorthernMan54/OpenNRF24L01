// receiver_connex.ino

/*
 * Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>
 * Updated 2020 TMRh20
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

/**
 * Channel scanner and Continuous Carrier Wave Output
 *
 * Example to detect interference on the various channels available.
 * This is a good diagnostic tool to check whether you're picking a
 * good channel for your application.
 *
 * Run this sketch on two devices. On one device, start CCW output by sending a 'g'
 * character over Serial. The other device scanning should detect the output of the sending
 * device on the given channel. Adjust channel and output power of CCW below.
 *
 * Inspired by cpixip.
 * See http://arduino.cc/forum/index.php/topic,54795.0.html
 */

#include "Arduino.h"
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "attack.h"

//
// Hardware configuration
//

// Set up nRF24L01 radio on SPI bus plus pins 7 & 8

RF24 radio(CE_GPIO, CSN_GPIO);

#define JSON_MSG_BUFFER 40 // Maximum

char payload[JSON_MSG_BUFFER];

//
// Channel info
//

const uint8_t num_channels = 80;
uint8_t values[num_channels];
uint64_t address;

uint8_t writeRegister(uint8_t reg, uint8_t value)
{
    uint8_t status;

    digitalWrite(CSN_GPIO, LOW);
    status = SPI.transfer(W_REGISTER | (REGISTER_MASK & reg));
    SPI.transfer(value);
    digitalWrite(CSN_GPIO, HIGH);
    return status;
}

uint8_t writeRegister(uint8_t reg, const uint8_t *buf, uint8_t len)
{
    uint8_t status;

    digitalWrite(CSN_GPIO, LOW);
    status = SPI.transfer(W_REGISTER | (REGISTER_MASK & reg));
    while (len--)
        SPI.transfer(*buf++);
    digitalWrite(CSN_GPIO, HIGH);

    return status;
}

//
// Setup
//

void setup(void)
{
    //
    // Print preamble
    //

    Serial.begin(921600);
    Serial.println(F("\n\rRF24/examples/scanner/"));

    //
    // Setup and configure rf radio
    //

    //   address = 0x76F1A15BA;  // address of my logitech mouse
    // address = 0xBA151A6F07; // address of my logitech mouse
    // addresss of my connex A0 92 70 4E 2D or 09 27 04 E2 D9

    //   connex_scanner output count - address
    // 325 A0 92 70 4E 2D 9C 
    // 359 A0 92 70 4E 2D 9A 
    // 362 A0 92 70 4E 2D 98 
    // 422 A0 92 70 4E 2D 9E 

    // 0xd9 e2 04 27 09

    address = 0x092704E2D9; // address of my connex baseboard
    address = 0xd9e2042709; // address of my connex baseboard

    address = 0x92704E2D9C; // address of my connex baseboard
    address = 0x9C2D4E7092; // address of my connex baseboard

    address = 0xA092704E2D; // address of my connex baseboard
    address = 0x2D4E7092A0; // address of my connex baseboard

    Serial.print(" a: ");
    for (int j = 0; j < 5; j++)
    {
        Serial.print((uint8_t)(address >> (8 * j) & 0xff), HEX);
        Serial.print(" ");
    }
    Serial.println("");
    radio.begin();
    /*
    radio.setAutoAck(false);
  //  radio.disableCRC();
    radio.setAddressWidth(5);
    radio.openReadingPipe(0, address);

    radio.startListening();
    radio.stopListening();
    radio.printDetails();
    */

    // the order of the following is VERY IMPORTANT
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
    radio.setChannel(0);
    // RF24 doesn't ever fully set this -- only certain bits of it
    writeRegister(EN_RXADDR, 0x00);
    // RF24 doesn't have a native way to change MAC...
    // 0x00 is "invalid" according to the datasheet, but Travis Goodspeed found it works :)
    // writeRegister(SETUP_AW, 0x00);
    radio.setAddressWidth(5);
    radio.openReadingPipe(0, address);
    radio.disableCRC();
    radio.startListening();
    radio.stopListening();
    radio.printDetails();

    //delay(1000);
    // Print out header, high then low digit
    int i = 0;
    while (i < num_channels)
    {
        Serial.print(i >> 4, HEX);
        ++i;
    }
    Serial.println();
    i = 0;
    while (i < num_channels)
    {
        Serial.print(i & 0xf, HEX);
        ++i;
    }
    Serial.println();
    //delay(1000);
}

//
// Loop
//

const int num_reps = 100;
bool constCarrierMode = 0;

void loop(void)
{
    /****************************************/
    // Send g over Serial to begin CCW output
    // Configure the channel and power level below
    if (Serial.available())
    {
        char c = Serial.read();
        if (c == 'g')
        {
            constCarrierMode = 1;
            radio.stopListening();
            delay(2);
            Serial.println("Starting Carrier Out");
            radio.startConstCarrier(RF24_PA_LOW, 40);
        }
        else if (c == 'e')
        {
            constCarrierMode = 0;
            radio.stopConstCarrier();
            Serial.println("Stopping Carrier Out");
        }
    }
    /****************************************/

    if (constCarrierMode == 0)
    {
        // Clear measurement values
        memset(values, 0, sizeof(values));

        // Scan all channels num_reps times
        int rep_counter = num_reps;
        while (rep_counter--)
        {
            int i = num_channels;
            while (i--)
            {
                // Select this channel
                radio.setChannel(i);

                // Listen for a little
                radio.startListening();
                delayMicroseconds(128);

                // Did we get a carrier?
                if (radio.testCarrier())
                {
                    ++values[i];
                    delayMicroseconds(2280);
                    uint8_t pipe;
                    if (radio.available(&pipe))
                    {
                        Serial.println("");                     // is there a payload? get the pipe number that recieved it
                        uint8_t bytes = radio.getPayloadSize(); // get the size of the payload
                        radio.read(&payload, bytes);            // fetch payload from FIFO
                        Serial.print(F("On channel "));
                        Serial.print(i); // print the size of the payload
                        Serial.print(F(" received "));
                        Serial.print(bytes); // print the size of the payload
                        Serial.print(F(" bytes on pipe "));
                        Serial.print(pipe); // print the pipe number
                        Serial.print(F(": "));
                        for (int j = 0; j < bytes; j++)
                        {
                            Serial.print(payload[j], HEX);
                            Serial.print(" ");
                        }
                        Serial.println("");
                    }
                }
                radio.stopListening();
            }
        }

        // Print out channel measurements, clamped to a single hex digit
      int i = 0;
      uint8_t busiestChannel = 0;
      uint8_t maxUsage = 0;
      while (i <= num_channels)
      {
        if (values[i] >= 16)
        {
          Serial.print("*");
        }
        else
        {
          Serial.print(min((uint8_t)0xf, values[i]), HEX);
        }
        if (values[i] > maxUsage)
        {
          busiestChannel = i;
          maxUsage = values[i];
        }
        ++i;
      }
      Serial.print(" busy: ");
      Serial.print((uint8_t)busiestChannel);
#ifdef KBS250
      Serial.print(" speed: 250Kbs count: ");
#endif
#ifdef MBS1
      Serial.print(" speed: 1Mbs count: ");
#endif
#ifdef MBS2
      Serial.print(" speed: 2Mbs count: ");
#endif
      Serial.print((uint8_t)maxUsage);
      Serial.print(" uptime: ");
      Serial.println(millis() / 1000);
    } //If constCarrierMode == 0
}