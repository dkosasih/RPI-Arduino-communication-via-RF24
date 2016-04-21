#define __STDC_FORMAT_MACROS
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <RF24/RF24.h>
#include <inttypes.h>

using namespace std;

int main(int argc, char** argv) 
{
	char* sendingPipe="0x0000000000";
	char* messagea="";

	for(int i =0;i<argc;i++){
		if(strcmp(argv[i],"-p")==0){
			sendingPipe = argv[i+1];
		}else 
		if(strcmp(argv[i],"-m")==0){
			messagea=argv[i+1];
		}
	}


	if((strcmp(sendingPipe,"0x0000000000")==0) ||  (strcmp(messagea,"")==0))
	{
		return 0;
	}
	else
	{
		string fileLocation = "/home/userabc/RF24Hub/outbox";
		ofstream myfile(fileLocation);
		if(myfile.is_open())
		{
			myfile << sendingPipe;
			myfile << "||";
			myfile << messagea;
		}
	}
	return 0;
}

