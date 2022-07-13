#include <ArduinoJson.h>

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

//
// Hardware configuration
//

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10

RF24 radio(9, 10);
const short red = 6;
const short green = 5;
const short blue = 3;

//
// Topology
//

// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = {
    0xF2F0F0F0D3LL, 0xF1F0F0F0C3LL};

// Door Status
typedef enum
{
  NOMEETING = 0,
  MIC = 1,
  CAMERA = 2,
  MICANDCAMERA = 3
} SwitchStats;

// FriendlyName of the door state
const char *switchStatsFriendlyName[] = {
    "No meeting\n", "Mic on\n", "Video on\n", "Video and mic on\n"};

// Status of the door currently
SwitchStats switchStats;

const int maxPayloadSize = 255;
const int minPayloadSize = 4;
const int packageSize = 32;
const int payloadIncrementBy = 2;
const char *packetSizeCode = "packet-size";
int nextPayloadSize = minPayloadSize;

void setup(void)
{
  setInitialState();

  configureRF();
}

void setInitialState()
{
  pinMode(red, OUTPUT);
  pinMode(green, OUTPUT);
  pinMode(blue, OUTPUT);
  analogWrite(red, 0);
  analogWrite(green, 0);
  analogWrite(blue, 0);
  delay(20);

  switchStats = NOMEETING;
}

void configureRF()
{
  Serial.begin(57600);
  printf_begin();

  // Setup and configure rf radio
  radio.begin();

  // We will be using the Ack Payload feature, so please enable it
  radio.enableAckPayload();

  // All other setup
  radio.setDataRate(RF24_250KBPS);
  radio.setPALevel(RF24_PA_MAX);
  radio.setChannel(90);
  radio.enableDynamicPayloads();
  radio.setRetries(15, 15);
  radio.setCRCLength(RF24_CRC_16);
  //

  // Start listening
  radio.openWritingPipe(pipes[0]);
  radio.openReadingPipe(1, pipes[1]);

  radio.startListening();

  // Dump the configuration of the rf unit for debugging
  radio.printDetails();
}

void loop(void)
{
  // if there is data ready
  if (radio.available())
  {
    String payload = getPayload();

    /*
     * NOTE: stop listening and send ack payload back when necessary
     */

    decideSwitchMode(payload);
    delay(1000);
  }
  runLightShow();
}

String getPayload()
{
  // Dump the payloads until we've gotten everything
  uint8_t len;

  //[maxPayloadSize + 1];
  String finalMessage;
  unsigned long started_waiting_at = millis();

  uint8_t radioIterator = 0;
  uint8_t packetCount = 1;

  while (radio.available() || radioIterator < packetCount)
  {
    char receive_payload[50];
    if (radio.available())
    {
      // Fetch the payload, and see if this was the last one.
      len = radio.getDynamicPayloadSize();
      radio.read(receive_payload, len);

      receive_payload[len] = '\0';

      // Spew it
      printf("Got payload size=%i value=%s\n\r", len, receive_payload);
    }

    if (millis() - started_waiting_at > 2500)
    {
      radioIterator = packetCount;
      printf("timeout; payload is now: %s\n\r", receive_payload);
    }

    // sender is sending packet size information
    if (radioIterator == 0 &&
        strncmp(receive_payload, packetSizeCode, sizeof(packetSizeCode)) == 0)
    {
      strtok(receive_payload, ":");
      packetCount = atoi(strtok(NULL, ":"));
      printf("number of packets changed to: %i\n\r", packetCount);

      radioIterator++;
    }

    // only concatenate string when packet count is still as the sender says
    if (strncmp(receive_payload, packetSizeCode, sizeof(packetSizeCode)) != 0 && strcmp(receive_payload, "") != 0)
    {
      finalMessage.concat(receive_payload);

      radioIterator++;
    }

    memset(receive_payload, 0, sizeof(receive_payload));
  }
  printf("no more data\n\r");

  return finalMessage;
}

void decideSwitchMode(String jsonString)
{
  DynamicJsonDocument doc(350);
  char *jsonChar = const_cast<char *>(jsonString.c_str());

  printf("JSON covert: %s", jsonString.c_str());
  auto error = deserializeJson(doc, jsonChar);
  if (error)
  {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(error.c_str());
    return;
  }

  const int noMeetRed = doc["noMeeting"]["red"];
  const int micRed = doc["mic"]["red"];

  printf("nomeet: %i, mic: %i; camera: %s\n\r", noMeetRed, micRed, doc["camera"]);

  // if (jsonString == "green")
  // {
  //   printf("decideSwitchMode in green: %s\n\r", jsonString);
  //   switchStats = MIC;
  //   return;
  // }

  // if (jsonString == "off") {
  //   printf("decideSwitchMode off: %s\n\r", jsonString);
  //   switchStats = NOMEETING;
  //   return;
  // }
}

void runLightShow()
{
  switch (switchStats)
  {
  case MIC:
    analogWrite(red, 255);
    delay(1000);
    analogWrite(red, 0);
    delay(1000);
    break;
  }
}
