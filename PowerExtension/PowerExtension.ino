#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

//
// Hardware configuration
//

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10

RF24 radio(9,10);
const short relayShort = 4;

//
// Topology
//

// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = { 
  0xF2F0F0F0D3LL, 0xF1F0F0F0C3LL };

// The various roles supported by this sketch
typedef enum { 
  role_sender = 1, role_receiver } 
role_e;

//Door Status
typedef enum {
  ON=0, OFF=1} 
SwitchStats;

// FriendlyName of the door state
const char* switchStatsFriendlyName[] = {
  "ON\n", "OFF\n"};

// The role of the current running sketch
role_e role;

//Status of the door currently
SwitchStats switchStats;


const int maxPayloadSize = 32;
const int minPayloadSize = 4;
const int payloadIncrementBy = 2;
int nextPayloadSize = minPayloadSize;
char receive_payload[maxPayloadSize+1]; // +1 to allow room for a terminating NULL char

void setup(void)
{
  //
  // Role
  //

  pinMode(relayShort, OUTPUT);
  digitalWrite(relayShort, HIGH);
  delay(20);

  switchStats = ON;

  // read the address pin, establish our role
  role = role_receiver;

  //
  // Print preamble
  //

  Serial.begin(57600);
  printf_begin();

  //
  // Setup and configure rf radio
  //

  radio.begin();

  // We will be using the Ack Payload feature, so please enable it
  radio.enableAckPayload();

  //All other setup
  radio.setDataRate(RF24_250KBPS);
  radio.setPALevel(RF24_PA_MAX);
  radio.setChannel(90);
  radio.enableDynamicPayloads();
  radio.setRetries(15,15);
  radio.setCRCLength(RF24_CRC_16);
  //

  //
  // Start listening
  //

  radio.openWritingPipe(pipes[0]);
  radio.openReadingPipe(1,pipes[1]);

  radio.startListening();
  //
  // Dump the configuration of the rf unit for debugging
  //
  radio.printDetails();
}

char message[32]="test payload";
void loop(void)
{
  if (role == role_sender)
  {
    // The payload will always be the same, what will change is how much of it we send.
    char send_payload[32] = "";
    strcpy(send_payload, message);

    // First, stop listening so we can talk.
    radio.stopListening();

    // Take the time, and send it.  This will block until complete
    printf("Now sending length %i...",nextPayloadSize);
    radio.write( send_payload, sizeof(send_payload) );

    // Now, continue listening
    radio.startListening();

    // Wait here until we get a response, or timeout
    unsigned long started_waiting_at = millis();
    bool timeout = false;
    while ( ! radio.available() && ! timeout )
      if (millis() - started_waiting_at > 500 )
        timeout = true;

    // Describe the results
    if ( timeout )
    {
      printf("Failed, response timed out.\n\r");
    }
    else
    {
      // Grab the response, compare, and send to debugging spew
      uint8_t len = radio.getDynamicPayloadSize();
      radio.read( receive_payload, len );

      // Put a zero at the end for easy printing
      receive_payload[len] = 0;

      // Spew it
      printf("Got response size=%i value=%s\n\r",len,receive_payload);
    }

    // Update size for next time.
    nextPayloadSize += payloadIncrementBy;
    if ( nextPayloadSize > maxPayloadSize )
      nextPayloadSize = minPayloadSize;

    // Try again 1s later
    delay(1000);

    //start listening again
    radio.startListening();
    role = role_receiver;
  }
  
  //
  // Pong back role.  Receive each packet, dump it out, and send it back
  //

  if ( role == role_receiver )
  {
    // if there is data ready
    if ( radio.available() )
    {
      // Dump the payloads until we've gotten everything
      uint8_t len;
      while (radio.available())
      {
        // Fetch the payload, and see if this was the last one.
        len = radio.getDynamicPayloadSize();
        radio.read( receive_payload, len );

        // Put a zero at the end for easy printing
        receive_payload[len] = 0;

        // Spew it
        printf("Got payload size=%i value=%s\n\r",len,receive_payload);
      }

      if(String(receive_payload)=="green" && switchStats == OFF){
        digitalWrite(relayShort, HIGH);
        switchStats = ON;
      }
      else if(String(receive_payload)=="off" && switchStats == ON){
        digitalWrite(relayShort,LOW);	
        switchStats = OFF;  
    }
      delay(1000);
    
      // First, stop listening so we can talk
      radio.stopListening();  
      
      // Send the final one back.
      String(switchStatsFriendlyName[switchStats]).toCharArray(receive_payload,32);
      
      radio.write( receive_payload, sizeof(receive_payload) );
      printf("Sent response.\n\r");

      // Now, resume listening so we catch the next packets.
      radio.startListening();
    }
  }
}
