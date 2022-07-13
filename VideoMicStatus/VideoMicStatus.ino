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
const short blue = 5;
const short green = 3;

//
// Topology
//

// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = {
    0xF2F0F0F0D3LL, 0xF1F0F0F0C3LL};

typedef enum
{
  OFF = 0,
  SOLID = 1,
  PULSE = 2,
  HEARTBEAT = 3,
  BLINK = 4
} LightVariation;

class Light {
  public: 
    int red;
    int green;
    int blue;
    LightVariation variation;
};

// typedef struct DeviceStatus {
//   Light noMeeting;
//   Light mic;
//   Light camera;
// };

// FriendlyName of the door state
const char *switchStatsFriendlyName[] = {
    "No meeting\n", "Mic on\n", "Video on\n", "Video and mic on\n"};

// Status of the door currently
// SwitchStats switchStats;
const char *packetSizeCode = "packet-size";

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

  // switchStats = NOMEETING;
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

void setLightValues(Light light, int override = -1){
  if(override == -1) {
    analogWrite(red, light.red);
    analogWrite(green, light.green);
    analogWrite(blue, light.blue);
    return;
  }
  analogWrite(red, override);
  analogWrite(green, override);
  analogWrite(blue, override);
}

void loop(void)
{
  Light light;
  setLightValues(light, 0);

  // if there is data ready
  if (radio.available())
  {
    String payload = getPayload();

    /*
     * NOTE: stop listening and send ack payload back when necessary
     */


    light = decideSwitchMode(payload, light);
    delay(1000);
  }
  runLightShow(light);
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

Light decideSwitchMode(String jsonString, Light light)
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

  if (doc != NULL)
  {

    light.red = doc["red"];
    light.green = doc["green"];
    light.blue = doc["blue"];
    light.variation = doc["variation"];
    // printf("decideswitch - red: %i, green: %i; blue: %i; variation:%i\n\r", light.red, light.green, light.blue, light.variation);
    
  }

  return light;
}

void runLightShow(Light light)
{
  if(light.variation == 0){
    // printf("lightshow off - red: %i, green: %i; blue: %i; variation:%i\n\r", light.red, light.green, light.blue, light.variation);
    setLightValues(light, 0);
    return;
  }

  if (light.variation == SOLID)
  {
    // printf("lightshow solid - red: %i, green: %i; blue: %i; variation:%i\n\r", light.red, light.green, light.blue, light.variation);
    setLightValues(light);
    delay(2000);
    return;
  }

  if (light.variation == PULSE)
  {
    setLightValues(light);
    delay(2000);
    setLightValues(light, 0);
    delay(700);
    return;
  }

  if (light.variation == HEARTBEAT)
  {
    setLightValues(light, 0);
    delay(2000);
    setLightValues(light);
    delay(200);
    setLightValues(light,0);
    delay(200);
    setLightValues(light);
    delay(200);
    setLightValues(light,0);
    return;
  }

  if (light.variation == BLINK)
  {
    setLightValues(light);
    delay(800);
    setLightValues(light, 0);
    delay(800);
    return;
  }
}
