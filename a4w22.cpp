#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sys/time.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <vector>
#include <map>
#include <sys/time.h>
#include <signal.h>

#define NRES_TYPES 10
#define NJOBS 25
#define MAX_LEN 32
using namespace std;

typedef struct job{
	string name;
	string status;
	int iter;
	int busy;
	int idle;
	map <string,int> resNeeded;
	map <string,int> resAvail;
	bool notDone;
} job;

typedef struct monitor{
	int time;
	vector<job> jobs;
} monitor;

sem_t critSem, checkRes;

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
int split(string inStr, vector <string> &token){
	int count;
	char *tokenp;
	
	count = 0;
	string inStrCpy = inStr; //create a copy of the string passed to the function
	if((tokenp = strtok(&inStr[0], " ")) == NULL){
		return 0; //return 0 if no token is found
	}
	//store first token if found in if statement above
	token.push_back((string) tokenp);
	count++;
	
	// This loop captures each token in the string and stores them in token. 
	while((tokenp = strtok(NULL, " "))!= NULL) {
		token.push_back((string) tokenp);
		count++;
	}
	
	inStr = inStrCpy;
	return count;
}

void createJob(vector <string> &token, vector<job> &jobs, int tokenNum, int iter, 
	map<string, int> &resources){
	job newJob;
	newJob.name = token[1];
	newJob.busy = atoi(&(token[2])[0]);
	newJob.idle = atoi(&(token[3])[0]);
	newJob.iter = iter;
	
	int i = 4;
	while(i < tokenNum){
		string res = (token[i]);
		size_t find = res.find(":");
		
		if((find != res.length()-1) && (find != 0) && (find != -1)){
			string name = &res.substr(0, find)[0];
			
			bool findRes = false;
			if ((resources.find(name)) != resources.end()) findRes = true;
			
			if (findRes == false){
				printf("Resource %s not found in resources available ", &name[0]);
				printf("for job %s\n", &newJob.name[0]);
				return;
			}
			
			newJob.resAvail = resources;
			newJob.resNeeded[name] = atoi(&res.substr(find+1)[0]);
		}
		else{
			printf("Error: ':' either not detected or in first or last position for ");
			printf("resource type %d\n for job %s", i , token[1]);
		}
		i++;
	}
	jobs.push_back(newJob);
}

void resourcesInit(vector <string> &token, map <string, int> &resources, int tokenNum){
	int i = 1;
	while (i < tokenNum){
		string resType(token[i]);
		size_t find = resType.find(":");
		
		if((find != resType.length()-1) && (find != 0) && (find != -1)){
			resources[resType.substr(0, find)] = atoi(&resType.substr(find+1)[0]);;
		}
		else{
			printf("Error: ':' either not detected or in first or last position for ");
			printf("resource type %d\n", i);
		}
		i++;
	}
	
}

/*
* The alarm_handler function is a signal handler which will recieve the timer signal
*
* Arguments:
*	signum- The signal recieved as an integer
*/
void alarm_handler(int signum){
	return;
}

void* jobManager(void* args){
	struct job *currJob = (struct job*) args;
	currJob->status = "WAIT";
	
	map<string,int>::iterator it;
	for (int i = 0; i < currJob->iter; i++){
		bool waiting;
		
		while(waiting){
			bool canAquire;
			sem_wait(&checkRes);
			for (it = currJob->resNeeded.begin(); it != currJob->resNeeded.end(); it++){
				string name = it->first;
				
				//check to see if the resource is needed by the job
				if ((currJob->resAvail.find(name)) == currJob->resAvail.end()) continue;
				//if the resource is found, check to see if we there's enough of the resource available
				else if ((it->second) > currJob->resAvail[name]){
					canAquire = false; 
					break;
				}
			}
			
			if (canAquire == false){
				sem_post(&checkRes);
				continue;
			}
			else {
				for(it = currJob->resNeeded.begin(); it != currJob->resNeeded.end(); i++){
					string name = it->first;
					
					//take resources
					currJob->resAvail[name] = (currJob->resAvail[name]) - it->second;
				}
				sem_post(&checkRes);
				waiting = false;
			}
		}
		
		currJob->status = "RUN";
		
		sem_wait(&critSem);
		
		struct itimerval timer;
		//convert milliseconds to seconds for timer
		timer.it_value.tv_sec= (currJob->busy) / 1000;
		
		// Catch alarm signal
		if (signal(SIGALRM, alarm_handler) == SIG_ERR){
			printf("SIGALRM error");
			break;
		}
		
		//Enter busy period
		if (setitimer(ITIMER_REAL,&timer, NULL) == -1){
			printf("Error calling setitimer: %s\n", strerror(errno));
			break;
		}
		
		for(it = currJob->resNeeded.begin(); it != currJob->resNeeded.end(); it++){
			string name = it->first;
			
			//Return resources
			currJob->resAvail[name] = (currJob->resAvail[name]) + it->second;
		}
		
		printf("job: %s (tid = %lu, iter= %d, time= \n", &currJob->name[0], pthread_self(), i+1);
		sem_post(&critSem);
		//change status to idle and enter idle period
		currJob->status = "IDLE";
		
		//convert milliseconds to seconds for timer
		timer.it_value.tv_sec= (currJob->idle) / 1000;
		
		// Catch alarm signal
		if (signal(SIGALRM, alarm_handler) == SIG_ERR){
			printf("SIGALRM error");
			break;
		}
		
		//Enter idle period
		if (setitimer(ITIMER_REAL,&timer, NULL) == -1){
			printf("Error calling setitimer: %s\n", strerror(errno));
			break;
		}
		
	}
	currJob->notDone = false;
}

void* monitorManager(void* args){
	//get vector of jobs
	struct monitor * mon= (struct monitor*) args;
	bool terminate = false;

	sem_wait(&checkRes);
	
	for (int i = 0; i < mon->jobs.size(); i++){
		if(mon->jobs[i].notDone == true) break;
		else terminate = true;
	}
	
	printf("monitor: [WAIT] ");
		
	for (int i = 0; i < mon->jobs.size(); i++){
		if (mon->jobs[i].status == "WAIT") printf("%s ", &mon->jobs[i].name[0]);
	}
		
	printf("\n         [RUN]  ");
	for (int i = 0; i < mon->jobs.size(); i++){
		if (mon->jobs[i].status == "RUN") printf("%s ", &mon->jobs[i].name[0]);
	}
		
	printf("\n         [IDLE] ");
	for (int i = 0; i < mon->jobs.size(); i++){
		if (mon->jobs[i].status == "IDLE") printf("%s ", &mon->jobs[i].name[0]);
	}
	printf("\n");
	sem_post(&checkRes);
		
	struct itimerval timer;
		
	//convert milliseconds to seconds for timer
	timer.it_value.tv_sec= (mon->time) / 1000;
	
	// Catch alarm signal
	if (signal(SIGALRM, alarm_handler) == SIG_ERR){
		printf("SIGALRM error");
	}
	
	//Enter idle period
	if (setitimer(ITIMER_REAL,&timer, NULL) == -1){
		printf("Error calling setitimer: %s\n", strerror(errno));
	}
}

int main(int argc, char *argv[]){
	if (argc != 4){
		printf("Invalid number of arguments!\n");
	}
	
	int monTime = atoi(argv[2]);
	int n = atoi(argv[3]);
	
	FILE *file;
	if ((file = fopen(argv[1], "r")) == NULL){
		printf("FOPEN FAILED! %s\n", strerror(errno));
		return 0;
	}
	
	map<string,int> resources;
	vector <job> jobs;
	for(;;){
		char unparsed[MAX_LEN], *line;
		if((line = fgets(unparsed, MAX_LEN, file)) != NULL){
			//If the first character of the line is a '#', it is a comment. Carry on to next line.
			if (line[0] == '#') continue;
			
			bool r = false;
			vector <string> token;
			int tokenNum = split(line, token);
			
			//If it is a resource
			if (token[0] == "resources") r = true;
			
			//Check that enough info is provided to do whatever type it is
			if ((r && (tokenNum < 2)) || (r && (tokenNum > NRES_TYPES+1))){
				printf("Resource has incorrect number of parameters in data file\n");
				break;
			}
			if ((!r && (tokenNum < 5)) || (!r && (tokenNum > NJOBS+4))){
				printf("Job does has incorrect number of parameters in data file\n");
				break;
			}
			
			
			if (r == false){
				createJob(token, jobs, tokenNum, n, resources);
			}
			else{
				if(resources.empty()){
					//get resources
					resourcesInit(token, resources, tokenNum);
				}
				else{
					printf("Error: Resources can only be allocated once per data file.\n");
				}
			}
			
		}
		else break;
	}
	fclose(file);
	
	sem_init(&critSem, 0, jobs.size()+1);
	sem_init(&checkRes, 0, 1);
	pthread_t threads[jobs.size()+1];
	for(int i = 0; i < jobs.size(); i++){
		if (pthread_create(&threads[i], NULL, &jobManager, &jobs[i]) != 0){
			printf("Create Job Thread Error: %s\n", strerror(errno));
		}
	}
	
	struct monitor monStruct;
	monStruct.time = monTime;
	monStruct.jobs = jobs; 
	if (pthread_create(&threads[jobs.size()], NULL, &monitorManager, &monStruct) != 0){
		printf("Create Monitor Thread Error: %s\n", strerror, errno);
	}
	
	for(int i = 0; i < (jobs.size()+1); i++){
		if(pthread_join(threads[i], NULL) != 0){
			printf("Join Error: %s\n", strerror(errno));
		}
		if(jobs[i].notDone == false) printf("%s done", &jobs[i].name[0]);
	}
	
}