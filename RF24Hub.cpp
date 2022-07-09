/*
	dkosasih 2014 
*/

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <curl/curl.h>
#include <RF24/RF24.h>

using namespace std;
//
// Hardware configuration
//

// CE Pin, CSN Pin, SPI Speed

// Setup for GPIO 22 CE and GPIO 25 CSN with SPI Speed @ 1Mhz
//RF24 radio(RPI_V2_GPIO_P1_22, RPI_V2_GPIO_P1_18, BCM2835_SPI_SPEED_1MHZ);

// Setup for GPIO 22 CE and CE0 CSN with SPI Speed @ 4Mhz
//RF24 radio(RPI_V2_GPIO_P1_15, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_4MHZ); 

// Setup for GPIO 22 CE and CE1 CSN with SPI Speed @ 8Mhz
//RF24 radio(RPI_V2_GPIO_P1_15, RPI_V2_GPIO_P1_24, BCM2835_SPI_SPEED_8MHZ); 
RF24 radio(RPI_BPLUS_GPIO_J8_15, RPI_BPLUS_GPIO_J8_24, BCM2835_SPI_SPEED_8MHZ);  


// Radio pipe addresses for the 2 nodes to communicate.
//uint64_t talking_pipes[] = { 0xF0F0F0F0D2LL, 0xF0F0F0F0C3LL, 0xF0F0F0F0B4LL, 0xF0F0F0F0A5LL, 0xF0F0F0F096LL };
//uint64_t listening_pipes[] = { 0xF0F0F0F0E1LL, 0x3A3A3A3AC3LL, 0x3A3A3A3AB4LL, 0x3A3A3A3AA5LL, 0x3A3A3A3A96LL };
vector<string> nodes;
vector<uint64_t> talking_pipes;
vector<uint64_t> listening_pipes;

// The various roles supported by this sketch
typedef enum { 
	role_sender = 1, role_receiver = 2 }
role_e;

// The role of the current running sketch
role_e role;

const int maxPayloadSize = 350;
const int chunkCapacity = 30;
const char *packetSizeCode = "packet-size";

char receive_payload[maxPayloadSize + 1]; // +1 to allow room for a terminating NULL char

void initialize(){
	string line;
	char *parsed;
	char theMessage[maxPayloadSize]="";
	const char *delim = "||";
	
	int i = 0;
	
	ifstream ifile("rpi-hub.config");
	if(ifile){
		if(ifile.is_open())
		{
			while (getline(ifile,line))
			{
				parsed = strtok(const_cast<char*>(line.c_str()), delim);
				printf("pipe: %s\n\r", parsed);
				strcpy(theMessage, parsed);
				nodes.push_back(string(theMessage));

				parsed = strtok(NULL, delim);
				printf("talk pipe: %s\n\r", parsed);
				talking_pipes.push_back(strtoull(parsed,NULL,0));
				
				parsed = strtok(NULL, delim);
				printf("listen pipe: %s\n\r", parsed);
				listening_pipes.push_back(strtoull(parsed,NULL,0));
				
				//opening listening pipe
				radio.openReadingPipe((i+1),listening_pipes[i]);
				
                i++;
			}
			ifile.close();
		}
	}

}

uint8_t findPipeIndex(string nodeName)
{
	//printf("looking for node: %s\n ", nodeName.c_str());
	uint8_t retVal=99;
	uint8_t ss=0;
	uint8_t nodesize=nodes.size();
	while(ss<nodesize)
	{
		//if(strcmp(nodeName.c_str(), nodes.at(i).c_str())==0){
		if( nodeName==nodes.at(ss)){
			return ss;
		}
		ss++;
	}
	return retVal;
}

void makeCurlReq(string itemID, string message){
    CURL *curl;
    CURLcode res;
    
    /* In windows, this will init the winsock stuff */
    curl_global_init(CURL_GLOBAL_ALL);
    
    /* get a curl handle */
    curl = curl_easy_init();
    if(curl) {
        /* First set the URL that is about to receive our POST. This URL can
         just as well be a https:// URL if that is what should receive the
         data. */
		string urlToSend = "http://localhost:8080/rest/items/" + itemID; 
        curl_easy_setopt(curl, CURLOPT_URL, urlToSend.c_str());
        /* Now specify the POST data */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, message.c_str());
        /* Set Header type*/
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type:text/plain");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n\r",
                    curl_easy_strerror(res));
        
        /* always cleanup */
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
}

void splitAndSend(char *msg, const int arraySize, const int len)
{
  printf("Array size: %i\n\r", arraySize);
  int numPackets = ((int)arraySize / len);
  int P_iterator = 0;

  printf("num_Packets: %i\n\r", numPackets);

  while (P_iterator <= numPackets)
  {
	if(numPackets >  0 && P_iterator == 0) {
		char numstr[21];
		sprintf(numstr, ":%i", (numPackets + 1));
		string pkg = string(packetSizeCode) + numstr;
		printf("%s\n\r", pkg.c_str());
		radio.write(pkg.c_str(), pkg.length());
	}

	char t[len + 1];
    char *ptr1 = msg + P_iterator * (len);
    strncpy(t, ptr1, len);

    t[len] = '\0';
    printf("%s\n\r", t);
    radio.write(&t, sizeof(t));
    P_iterator++;
  }
}

string getPayload(){
  // Dump the payloads until we've gotten everything
  uint8_t len;
  
  //[maxPayloadSize + 1];
  string finalMessage;
  unsigned long started_waiting_at = millis();

  uint8_t radioIterator = 0;
  uint8_t packetCount = 1;

  while (radio.available() || radioIterator < packetCount)
  {
    char receive_payload[50];
    if(radio.available()) {
      // Fetch the payload, and see if this was the last one.
      len = radio.getDynamicPayloadSize();
      radio.read(receive_payload, len);
       // Spew it
      printf("Got payload size=%i value=%s\n\r", len, receive_payload);
    }

    if (millis() - started_waiting_at > 2500) {
      radioIterator = packetCount;
      printf("timeout; payload is now: %s\n\r", receive_payload);
    }

    // sender is sending packet size information
    if(radioIterator == 0 && 
      strncmp(receive_payload, packetSizeCode, sizeof(packetSizeCode)) == 0) {
        strtok(receive_payload, ":");
        packetCount = atoi(strtok(NULL, ":"));
        printf("number of packets changed to: %i\n\r", packetCount);

        radioIterator++;
    }

    // only concatenate string when packet count is still as the sender says
    if(strncmp(receive_payload, packetSizeCode, sizeof(packetSizeCode)) != 0) {
      finalMessage.append(receive_payload);

      radioIterator++;
    }
  }
  printf("no more data\n\r");

  return finalMessage;
}

int main(int argc, char** argv){
	// Refer to RF24.h or nRF24L01 DS for settings
	radio.begin();
	radio.enableDynamicPayloads();
	radio.setAutoAck(1);
	radio.setRetries(15,15);
	radio.setDataRate(RF24_250KBPS);
	radio.setPALevel(RF24_PA_MAX);
	radio.setChannel(90);
	radio.setCRCLength(RF24_CRC_16);

	initialize();

	//
	// Start listening
	//
	radio.startListening();

	//
	// Dump the configuration of the rf unit for debugging
	//
	radio.printDetails();
	
	printf("Output below : \n");
	delay(1);

	role=role_receiver;

    int talkingPipeIndex = 99;
    
    //
	// forever loop
	while (1)
	{
        fflush(stdout);

		string line;
		char *parsed;
		const char *delim = "||";
		char sendToNode[maxPayloadSize] = "";
		
		ifstream ifile("outbox");
		if(ifile){
			if(ifile.is_open())
			{
				while (getline(ifile,line))
				{
					parsed = strtok(const_cast<char*>(line.c_str()), delim);
					printf("pipe: %s\n\r", parsed);
					strcpy(sendToNode, parsed);

					//sender_pipe = strtoull(parsed,NULL,0);
					parsed = strtok(NULL, delim);
					printf("Message: %s\n\r", parsed);
				
					role = role_sender;
				}
				ifile.close();
				remove("outbox");
			}
		}
        
		if (role == role_sender)
		{
			talkingPipeIndex = findPipeIndex(sendToNode);
			printf("talkingpipe: %i", talkingPipeIndex);
			if (talkingPipeIndex != 99)
			{
				// First, stop listening so we can talk.
                radio.stopListening();
                
                radio.openWritingPipe(talking_pipes[talkingPipeIndex]);
				printf("about to write\n\r");

				splitAndSend(parsed, strlen(parsed), chunkCapacity);

				delay(100);

                // Now, continue listening
                radio.startListening();
                
                delay(100);
                
                // Wait here until we get a response, or timeout
                unsigned long started_waiting_at = millis();
                bool timeout = false;
				uint8_t pipe_numAck = 1;
                while (!radio.available(&pipe_numAck) && !timeout){
                   if (millis() - started_waiting_at > 2500 )
                        timeout = true;
                }
                // Describe the results
                if ( timeout )
                {
                    printf("Failed, response timed out.\n\r");
                }
                else
                {
                    printf("num ack: %i; talkpipe: %i\n\r", (pipe_numAck), (talkingPipeIndex+1));
					//only accept the connection from the sending pair
					if(pipe_numAck == (talkingPipeIndex+1)){
						// Grab the response, compare, and send to debugging spew                    
						uint8_t len = radio.getDynamicPayloadSize();
						radio.read( receive_payload, len );
						
						// Put a zero at the end for easy printing
						receive_payload[len] = 0;
						
						// Spew it
						printf("Got response size=%i value=%s\n\r",len,receive_payload);
					}
                }
                
                // Try again 1ms later
                delay(100);
                
                role = role_receiver;
                radio.startListening();
			}
		}

		//
		// Pong back role.  Receive each packet, dump it out, and send it back
		//
		//char receivePayload[maxPayloadSize];
		if ( role == role_receiver )
		{
			// if there is data ready
			uint8_t pipe_num=1;
			if ( radio.available(&pipe_num) )
			{
				string payload = getPayload();
				
				// First, stop listening so we can talk
				radio.stopListening();

				// Open the correct pipe for writing
				radio.openWritingPipe(talking_pipes[pipe_num-1]);

				// Retain the low 2 bytes to identify the pipe for the spew
				uint16_t pipe_id = talking_pipes[pipe_num-1] & 0xffff;

				// Send the final one back.
				char ok[3] = "OK";
				radio.write(ok, 3);
				printf("Sent response to %04x.\n\r",pipe_id);

                //Making a local CURL call to OPENHAB
				makeCurlReq(nodes[pipe_num-1], payload);
				
				// Now, resume listening so we catch the next packets.
				radio.startListening();
			}
		}
	}
	return 0;
}
