#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sys/time.h>
#include <errno.h>

#define NRES_TYPES 10
#define NJOBS 25
#define MAX_LEN 32
using namespace std;

typedef struct resource{
	string name;
	int avail;
	int held;
}

/*
* This function will take in a string and split it based on the field delimiters provided. It will
* fill the outToken array with the seperated strings and return the total number of tokens created.
* 
* Arguments:
*	inStr- the string being parsed
*	token- the container for the parsed string
*	fieldDelim- the characters used to split the string
* Returns:
*	0 if no tokens are found or the number of tokens found
*/
int split(string inStr, char token[][MAX_LEN], char fieldDelim[]){
	int i, count;
	char *tokenp;
	
	count = 0;
	
	for (i = 0; i < MAX_LEN; i++)
		memset(token[i], 0 , sizeof(token[i]));
	
	string inStrCpy = inStr; //create a copy of the string passed to the function
	if((tokenp = strtok(&inStr[0], fieldDelim)) == NULL){
		return 0; //return 0 if no token is found
	}
	//store first token if found in if statement above
	strcpy(token[count], tokenp);
	count++;
	
	// This loop captures each token in the string and stores them in token. 
	while((tokenp = strtok(NULL, fieldDelim))!= NULL) {
		strcpy(token[count], tokenp);
		count++;
	}
	
	inStr = inStrCpy;
	return count;
}

void resourcesInit(char token[][MAX_LEN], vector <resource> &resources){
	int i = 1;
	while ((string)token[i] != NULL){
		string resType(token[i]);
		size_t find = resType.find(":");
		
		if((find != resType.length()-1) && (find != 0) && (find != -1)){
		resource newRes;
		newRes.name = &resType.substr(0, find+1);
		newRes.avail = atoi(&resType.substr(find+1));
		
		resources.push_back(newRes);
		}
		else{
			printf("Error: ':' either not detected or in first or last position for ");
			printf("resource type %d\n", i);
		}
		i++;
	}
	
}

int main(int argc, char *argv[]){
	if (argc != 4){
		printf("Invalid number of arguments!\n");
	}
	
	int monTime = atoi(argv[2]);
	int n = atoi(argv[3]);
	
	FILE *file;
	if ((file = fopen(argv[2], "r")) == NULL){
		printf("FOPEN FAILED! %s\n", strerror(errno));
		return 0;
	}
	
	vector <resource> resources;
	for(;;){
		char unparsed[MAX_LEN], *line;
		if((line = fgets(unparsed, MAX_LEN, file)) != NULL){
			//If the first character of the line is a '#', it is a comment. Carry on to next line.
			if (line[0] == '#') continue;
			
			bool r = false;
			//If it is a job, make the max rows NJOBS+1 to account for token 'job'
			if (line[0] != 'r') char token [NJOBS+1][MAX_LEN];
			//If it is a resource, make the max rows NRES_TYPES+1 to account for token 'resource'
			else {
				char token [NRES_TYPES+1][MAX_LEN];
				r = true;
			}
			
			char delim[1] = {' '};
			int tokenNum = (split((string) line, token, delim));
			
			//Check that enough info is provided to do whatever type it is
			if (r && tokenNum < 2){
				printf("Resource does not have enough parameters in data file\n");
				break;
			}
			if (!r && tokenNum < 5){
				printf("Job does not have enough parameters in data file\n")
				break;
			}
			
			int busy, idle;
			if (!resource){
				string name(token[1]);
				busy = atoi(token[2]);
				idle = atoi(token[3]);
			}
			else{
				//get resources
				resourcesInit(token, resources);
			}
			
		}
	}
	
}