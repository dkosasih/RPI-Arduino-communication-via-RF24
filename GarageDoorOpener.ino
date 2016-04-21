
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

//
// Hardware configuration
//

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10

RF24 radio(9,10);

// sets the role of this unit in hardware.  Connect to GND to be the 'pong' receiver
// Leave open to be the 'ping' transmitter
const int role_pin = 7;
const short relayShort = 3;

//
// Topology
//

// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = { 
  0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

//
// Role management
//
// Set up role.  This sketch uses the same software for all the nodes
// in this system.  Doing so greatly simplifies testing.  The hardware itself specifies
// which node it is.
//
// This is done through the role_pin
//

// The various roles supported by this sketch
typedef enum { 
  role_sender = 1, role_receiver } 
role_e;

//Door Status
typedef enum {
  Door_Open=0, Door_Open_for_10Mins=1, Door_Closed =2, Still_Open} 
DoorStats;

// The debug-friendly names of those roles
const char* role_friendly_name[] = { 
  "invalid", "Sender", "Receiver"};

// FriendlyName of the door state
const char* doorStatsFriendlyName[] = {
  "Door's Open\n", "Door's Open For More than 10Mins\n", "Door's Closed\n", "Door's Still Open\n"};

// The role of the current running sketch
role_e role;

//Status of the door currently
DoorStats doorStats;

//
// Payload
//

const int min_payload_size = 4;
const int max_payload_size = 32;
const int payload_size_increments_by = 2;
int next_payload_size = min_payload_size;

char receive_payload[max_payload_size+1]; // +1 to allow room for a terminating NULL char

void setup(void)
{
  //
  // Role
  //

  // set up the role pin
  pinMode(role_pin, INPUT);
  digitalWrite(role_pin,HIGH);
  delay(20); // Just to get a solid reading on the role pin

  pinMode(relayShort, OUTPUT);
  digitalWrite(relayShort, LOW);
  delay(20);

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

// the following variables are long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long lastDebounceTime = 0;  // the last time the output pin was toggled
long debounceDelay = 600000;    // the debounce time; increase if the output flickers; 600000 is 10 mins
bool justOpen = true;
bool stillOpen=false;
char message[32]="";

int lastRecordedPinRead = HIGH;
long readingDebounceDelay = 500; 
long statusCheckupDebounceDelay = 300000;//300000 is 5 minutes
long lastStatusCheckupTime = 0;
long lastStatusDebounceTime = 0;
int currentReading;
void loop(void)
{
  //only send the reading if there is a state change in between 0.5 secs
  if(millis()-lastStatusDebounceTime > readingDebounceDelay){
    currentReading = digitalRead(role_pin);
    if(lastRecordedPinRead!=currentReading){

      printf("lasr recorded %i\n\r",lastRecordedPinRead);
      printf("reading: %i\n\r",currentReading);
      role = role_sender;
      if(currentReading==HIGH){        
        String(doorStatsFriendlyName[Door_Open]).toCharArray(message,32);
      }
      else{
        stillOpen=false;
        String(doorStatsFriendlyName[Door_Closed]).toCharArray(message,32);
      }
    }  

    if(millis()-lastStatusCheckupTime > statusCheckupDebounceDelay){
      role = role_sender;
      if(currentReading==HIGH){
        if (stillOpen) {
          String(doorStatsFriendlyName[Still_Open]).toCharArray(message,32);
        }
        else{
          String(doorStatsFriendlyName[Door_Open]).toCharArray(message,32);
          stillOpen=true;
        }
      }
      else{
        String(doorStatsFriendlyName[Door_Closed]).toCharArray(message,32);
      }
      lastStatusCheckupTime = millis();
    }

    lastRecordedPinRead = currentReading;
    lastStatusDebounceTime = millis();
  }

  if (digitalRead(role_pin) && role != role_sender)
  {
    if (justOpen) {
      lastDebounceTime = millis();
      justOpen=false;
    }
    if ((millis() - lastDebounceTime) > debounceDelay) {
      role = role_sender;
      printf("ROLE: %s\n\r",role_friendly_name[role]);
      String(doorStatsFriendlyName[Door_Open_for_10Mins]).toCharArray(message,32);
      stillOpen=true;
      justOpen=true;
    }
  } 
  else if(!digitalRead(role_pin)){   
    justOpen=true;
  }


  //
  // Ping out role.  Repeatedly send the current time
  //
  if (role == role_sender)
  {
    // The payload will always be the same, what will change is how much of it we send.
    char send_payload[32] = "";
    strcpy(send_payload, message);

    // First, stop listening so we can talk.
    radio.stopListening();

    // Take the time, and send it.  This will block until complete
    printf("Now sending length %i...",next_payload_size);
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
    next_payload_size += payload_size_increments_by;
    if ( next_payload_size > max_payload_size )
      next_payload_size = min_payload_size;

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
      bool done = false;
      while (!done)
      {
        // Fetch the payload, and see if this was the last one.
        len = radio.getDynamicPayloadSize();
        done = radio.read( receive_payload, len );

        // Put a zero at the end for easy printing
        receive_payload[len] = 0;

        // Spew it
        printf("Got payload size=%i value=%s\n\r",len,receive_payload);
      }

      if(String(receive_payload)=="Open_Door"){
        digitalWrite(relayShort,LOW);
        delay(2000);
        digitalWrite(relayShort, HIGH);
      }
      else if(String(receive_payload)=="Close_Door"){
        digitalWrite(relayShort,LOW);		
        delay(2000);
        digitalWrite(relayShort, HIGH);
      }

      // First, stop listening so we can talk
      radio.stopListening();

      // Send the final one back.
      if(currentReading==HIGH){
        String(doorStatsFriendlyName[Door_Open]).toCharArray(receive_payload,32);
      }
      else{
        String(doorStatsFriendlyName[Door_Closed]).toCharArray(receive_payload,32);
      }
      radio.write( receive_payload, sizeof(receive_payload) );
      printf("Sent response.\n\r");

      // Now, resume listening so we catch the next packets.
      radio.startListening();
    }
  }
}
