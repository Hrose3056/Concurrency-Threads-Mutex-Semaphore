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
#include <sys/times.h>


#define NRES_TYPES 10
#define NJOBS 25
#define MAX_LEN 32
using namespace std;

/*
* Author: Hannah Desmarais
* CCID: hdesmara
*
* This is a program that simulates the concurrent execution of a set of jobs a certain amount of 
* times as specified by the user. It does so using nonshareable threads and semaphores, both 
* binary and counting. It will read data from a file that specifies resource types and the amount
* that they have available as well as jobs that require a certain number of those resources and run
* and idle for a specified amount of time in milliseconds. It has a monitor which will track what 
* phase each job is in which will print every x milliseconds as specified on the command line. To
* run this program, navigate to the folder and type:
* 	 "./a4w22 inputFileName monitorTime nExecutionsDesired"
*/
typedef struct job{
	string name;
	string status;
	int iter;
	int iterDone = 0;
	int busy;
	int idle;
	clock_t start;
	double waitTime;
	map <string,int> resNeeded;
	map <string,int> resHeld;
	bool notDone;
	unsigned long int tid;
} job;

typedef struct monitor{
	int time;
	vector<job> jobs;
} monitor;

sem_t critSem, check, print;
map<string,int> resAvail;

/*
* The getTime() function will capture the times of the user CPU, system CPU, and the terminated 
* children. It stores it in the tms struct provided by sys/time.h and returns it.
*
* Arguments:
*	-time: the clock_t time being captured
* Returns:
*	returns a struct with the user and system CPU times of the parent and it's children.
*/
struct tms getTime(clock_t &time){
	struct tms timeStruct;
	
	if ((time = times(&timeStruct)) == -1) // Capture times
		cout<< "times error"<< endl;
	
	return timeStruct;
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

/*
* The createJob function will initialize the jobs specified in the data file with the information
* specified
*
* Arguments:
*	token- This contains the tokens of the line as a vector of strings
*	jobs- This is a vector containing structs of the type job for all jobs specified in the file
*	tokenNum- The number of elements in token
*	iter- The number of iterations the user wants all jobs to do as specified on the command line
*		  as an int
*	resources- This is the map we are initializing that has a atring for a key and an int for the 
*			   available amount for this type of resource. This map is for manipulation by jobs
*	start- This is the time that the program started as a clock_t value
*/
void createJob(vector <string> &token, vector<job> &jobs, int tokenNum, int iter, 
	map<string, int> &resources, clock_t &start){
	job newJob;
	newJob.name = token[1];
	newJob.busy = atoi(&(token[2])[0]);
	newJob.idle = atoi(&(token[3])[0]);
	newJob.iter = iter;
	newJob.start = start;
	newJob.notDone = true;
	
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
			
			newJob.resNeeded[name] = atoi(&res.substr(find+1)[0]);
			newJob.resHeld[name] = 0;
		}
		else{
			printf("Error: ':' either not detected or in first or last position for ");
			printf("resource type %d\n for job %s", i , token[1]);
		}
		i++;
	}
	jobs.push_back(newJob);
}

/*
* The resourcesInit funtion will parse a line from the data file and store what it needs
* to create a new resource.
*
* Arguments:
*	token- This contains the tokens of the line as a vector of strings
*	resources- This is the map we are initializing that has a atring for a key and an int for the 
*			   available amount for this type of resource. This map is for manipulation by jobs
*	maxRes- This map is the same as resources except it is for keeping a copy of the max amount
*			available for this type of resource
*	tokenNum- The number of elements in token
*/
void resourcesInit(vector <string> &token, map <string, int> &resources, map<string,int> &maxRes,
int tokenNum){
	int i = 1;
	while (i < tokenNum){
		string resType(token[i]);
		size_t find = resType.find(":");
		
		if((find != resType.length()-1) && (find != 0) && (find != -1)){
			resources[resType.substr(0, find)] = atoi(&resType.substr(find+1)[0]);;
			maxRes[resType.substr(0, find)] = atoi(&resType.substr(find+1)[0]);;
		}
		else{
			printf("Error: ':' either not detected or in first or last position for ");
			printf("resource type %d\n", i);
		}
		i++;
	}
}

/*
* The waiting function will force a job to wait as it enters a semaphore one at a time to check
* if its desired resources are available. If the resources are not, it will continue to wait.
* If they are available, the job will take the resources and return to the jobMonitor function to
* run.
*
* Arguments:
*	currJob- This is a struct of the type job containing all information needed for the job.
*/
void waiting(job &currJob){
	bool waiting = true;
	currJob.status = "WAIT";
	
	while(waiting){
		bool aquired = true;
		map<string,int>::iterator it;
		
		// Enter waiting semaphore so only one thread may access the resources at a time
		// so that no two threads try to take the same resource.
		if(sem_wait(&check) < 0) {
				printf("Sem_wait error: %s\n", strerror(errno));
				exit(0);
		}
		
		for (it = currJob.resNeeded.begin(); it != currJob.resNeeded.end(); it++){
			string name = it->first;
				
			//check to see if the resource is needed by the job
			if (resAvail.find(name) == resAvail.end()){
				printf("resource does not exist in job\n");
				continue;
			}
			
			//if the resource is found, check to see if we there's enough of the resource available
			if ((it->second) > (resAvail[name])){
				aquired = false;
				break;
			}
		}
		
		if(aquired == false){
			// Exit the waiting semaphore to allow another thread to check
			if(sem_post(&check) < 0){
				printf("Waiting sem_post error: %s\n", strerror(errno));
			}
		}
		else{
			for(it = currJob.resNeeded.begin(); it != currJob.resNeeded.end(); it++){
				string name = it->first;
			
				// Take resources needed
				resAvail[name] = (resAvail[name]) - it->second;
				currJob.resHeld[name] = currJob.resHeld[name] + currJob.resNeeded[name];
			}
			waiting = false;
			
			// Exit the waiting semaphore to allow another thread to check
			if(sem_post(&check) < 0){
				printf("Waiting sem_post error: %s\n", strerror(errno));
			}
		}
	}
}

/*
* jobManager is a function called upon creation of a job thread. This function will run a job
* for the specified amount of times, change the state as it does so, and make the thread sleep for
* amounts of time specified in the data file.
*
* Arguments:
*	args- This contains any arguments (in this case a monitor struct) as a void pointer passed upon
*		  creation of the thread
*/
void* jobManager(void* args){
	struct job *currJob = (struct job*) args;
	currJob->tid = pthread_self();
	static long clockTick = 0;

	map<string,int>::iterator it;
	for (int i = 0; i < currJob->iter; i++){
		clock_t startW;
		struct tms startT = getTime(startW);
		
		static long clockTick = 0;
		if (clockTick == 0) //fetch clock ticks per second
			if ((clockTick = sysconf(_SC_CLK_TCK)) < 0) perror("sysconf error");
	
		waiting(*currJob);
		
		//Get the time elapsed while in wait
		clock_t endW;
		struct tms endT = getTime(endW);
		if (clockTick == 0) //fetch clock ticks per second
			if ((clockTick = sysconf(_SC_CLK_TCK)) < 0) perror("sysconf error");
		
		currJob->waitTime += (((endW - startW)/(double) clockTick)*1000);
		
		//Start the job. Jobs can run concurrently.
		if(sem_wait(&critSem) < 0){
			printf("Run sem_wait error: %s\n", strerror(errno));
		}
		currJob->status = "RUN";
		//convert milliseconds to seconds and nanoseconds for timer
		struct timespec busy;
		busy.tv_sec = (currJob->busy)/1000;
		busy.tv_nsec = ((currJob->busy)%1000) * 1000000;
		
		if(nanosleep(&busy, &busy) < 0){
			printf("Nanosleep error: %s\n", strerror(errno));
		}
		
		
		//change status to idle and enter idle period
		currJob->status = "IDLE";
		for(it = currJob->resNeeded.begin(); it != currJob->resNeeded.end(); it++){
			string name = it->first;
			
			//Return resources
			resAvail[name] = (resAvail[name]) + it->second;
			currJob->resHeld[name] = (currJob->resHeld[name]) - (currJob->resNeeded[name]);
		}
		if(sem_post(&critSem) < 0){
			printf("Run sem_wait error: %s\n", strerror(errno));
		}
		
		
		// Enter printing critical section so only one thread may print at a time
		if(sem_wait(&print) < 0){
			printf("Run print sem_wait error: %s\n", strerror(errno));
		}
		//get the time this job iteration occurred at
		clock_t jobTime;
		struct tms jobT = getTime(jobTime);
		
		printf("job: %s (tid = %lu, iter= %d, time= ", &currJob->name[0], pthread_self(), i+1);
		printf(" %5.0f msec)\n", ((jobTime - currJob->start) / (double) clockTick)*1000);
		if(sem_post(&print) < 0){
			printf("Run sem_wait error: %s\n", strerror(errno));
		}
		
		currJob->iterDone++;
		
		// Convert milliseconds to seconds and nanoseconds for timer
		struct timespec idle;
		idle.tv_sec = (currJob->idle)/1000;
		idle.tv_nsec = ((currJob->idle)%1000) * 1000000;
		
		// Stay idle for alloted time
		if(nanosleep(&idle, &idle) < 0){
			printf("Nanosleep error: %s\n", strerror(errno));
		}
		
	}
	currJob->notDone = false;
}

/*
* monitorManager is the function called upon the creation of a monitor thread. It will print the
* status of each job at the time of the call.
*
* Arguments:
*	args- This contains any arguments (in this case a monitor struct) as a void pointer passed upon
*		  creation of the thread
*/
void* monitorManager(void* args){
	//get vector of jobs
	struct monitor * mon= (struct monitor*) args;
	
	sem_wait(&print);
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
	sem_post(&print);
}

/*
* The printTerminate will only run when all jobs are done. When called, it will print out all the
* information about the jobs as well as the resources.
*
* Arguments:
*	maxRes- This is a map for resources containing an untouched int corresponding to the max number
*			this resource has available.
*	resources- This is a map containing the resources that were manipulated by the jobs with an int
*			   for how many are available.
*	jobs- This is a vector containing structs of the type job for all jobs specified in the file
*	start- This is the time that the program started as a clock_t value
*/
void printTerminate(map<string, int> &maxRes, map<string, int> &resources, vector<job> &jobs,
clock_t &start){
	printf("\nSystem resources:\n");
	
	map<string,int>::iterator it;
	
	for (it = maxRes.begin(); it != maxRes.end(); it++){
		printf("       %s:    (maxAvail= %d, held= ", &it->first[0], it->second);
		int diff = (it->second) - resources[it->first];
		printf("%d)\n", diff);
	}
	
	printf("\nSystem Jobs:\n");
	for(int i=0; i < jobs.size(); i++){
		printf("[%d] %s (%s, runTime= ", i, &jobs[i].name[0], &jobs[i].status[0]);
		printf("%d msec, idleTime= %d msec)\n", jobs[i].busy, jobs[i].idle);
		printf("(tid= %lu)\n", jobs[i].tid);
		
		for(it = jobs[i].resNeeded.begin(); it != jobs[i].resNeeded.end(); it++){
			string name = it->first;
			printf("	%s: (needed= %d, held= %d)\n", &name[0], it->second,jobs[i].resHeld[name]);
		}
	
		printf("	(RUN= %d, WAIT: %5.0f)\n\n", jobs[i].iterDone, jobs[i].waitTime);
	}
}

/*
* This function starts the program and reads the file to decide which calls to make.
*
* Arguments: 
*	argc- The number of arguments provided as an integer
*	argv- The parsed arguments as an array of C-strings
* Returns:
*	0 on error.
*/
int main(int argc, char *argv[]){
	clock_t start;
	struct tms startP = getTime(start);
	static long clockTick = 0;
	if (clockTick == 0) //fetch clock ticks per second
		if ((clockTick = sysconf(_SC_CLK_TCK)) < 0) perror("sysconf error");
	
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
	
	map<string,int> maxRes;
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
			
			//If not a resource line, create a new job. Else create resources
			if (r == false){
				createJob(token, jobs, tokenNum, n, resAvail, start);
			}
			else{
				// Ensure that resources has not already been specified
				if(resAvail.empty()){
					// Get resources
					resourcesInit(token, resAvail, maxRes, tokenNum);
				}
				else{
					printf("Error: Resources can only be allocated once per data file.\n");
				}
			}
			
		}
		else break;
	}
	fclose(file);
	
	/*
	* Initialize semaphores. All except critSem may only allow one thread in at a time to
	* prevent deadlocks or overlapping prints. critSem may have as many threads enter as
	* there are jobs.
	*/
	sem_init(&critSem, 0, jobs.size());
	sem_init(&check, 0, 1);
	sem_init(&print, 0, 1);
	pthread_t threads[jobs.size()+1];
	for(int i = 0; i < jobs.size(); i++){
		if (pthread_create(&threads[i], NULL, &jobManager, &jobs[i]) != 0){
			printf("Create Job Thread Error: %s\n", strerror(errno));
		}
	}
	
	struct monitor monStruct;
	monStruct.time = monTime;
	
	bool terminate = false;
	while (terminate == false){
		monStruct.jobs = jobs;
	
		//Check to see if all jobs are done. If yes, we can terminate
		for (int i = 0; i < jobs.size(); i++){
			if(jobs[i].notDone == true) break;
			else terminate = true;
		}
		if (terminate != true){
			if (pthread_create(&threads[jobs.size()], NULL, &monitorManager, &monStruct) != 0){
				printf("Create Monitor Thread Error: %s\n", strerror(errno));
			}
			if (pthread_join(threads[jobs.size()], NULL) != 0){
				printf("Monitor join error\n");
				break;
			}
		}
		
		// Sleep for time indicated on command line
		struct timespec monSleep;
		monSleep.tv_sec = (monStruct.time)/1000;
		monSleep.tv_nsec = ((monStruct.time)%1000) * 1000000;
			
		if(nanosleep(&monSleep, &monSleep) < 0){
			printf("Nanosleep error: %s\n", strerror(errno));
		}
	}
	
	// Join all threads
	for(int i = 0; i < jobs.size()-1; i++){
		if(pthread_join(threads[i], NULL) != 0){
			printf("Join Error: %s\n", strerror(errno));
		}
	}
	
	// Print the terminate text and destroy semaphores for cleanliness
	printTerminate(maxRes, resAvail, jobs, start);
	sem_destroy(&check);
	sem_destroy(&print);
	sem_destroy(&critSem);

	//Get total process time
	clock_t end;
	struct tms endT = getTime(end);
	printf("Running = %7.0f msec\n", ((end - start) / (double) clockTick)*1000);
}