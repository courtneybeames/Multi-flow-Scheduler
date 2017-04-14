#include <pthread.h>
#include <string.h>
#include <unistd.h>    
#include <stdio.h>      
#include <stdlib.h>     
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>

#define MAXFLOW 100


typedef struct _flow
{
    float arrivalTime ;
    float transTime ;
    int priority ;
    int id ;
} flow;


flow flowList[MAXFLOW];   // parse input in an array of flow
flow *queueList[MAXFLOW];  // store waiting flows while transmission pipe is occupied.
pthread_t thrList[MAXFLOW]; // each thread executes one flow
pthread_mutex_t trans_mtx = PTHREAD_MUTEX_INITIALIZER ; 
pthread_cond_t trans_cvar = PTHREAD_COND_INITIALIZER ;  
bool inPipe;
int waitingFlow;
int runningFlow;
double start;
struct timeval base, startTran, finTran;
int queueSize;

double get_time(struct timeval curTime){
	double startTransmission = curTime.tv_sec+(curTime.tv_usec/1000000.0);
    double transStartTime= startTransmission-start;
    return transStartTime;
}

//sorting function for queueList: sorts based on priority and transmission time
int cmpfunc(const void *a, const void *b){
    const flow *a1 = *(flow **)a;
    const flow *a2 = *(flow **)b;
    
    int priority = a1->priority - a2->priority;
    int trans = a1->transTime - a2->transTime;

    if (priority != 0){
    	return priority;
    }else if(trans != 0){
    	return trans;
    }
    return 0;

}

//adds flows to list and sorts
void addToList(flow *item){
 	queueList[queueSize]=item;
    queueSize++;

    qsort(queueList, queueSize, sizeof(flow*), cmpfunc);
}

//removes element 1 from the list
void rmvFromList(){
	int k;
	for(k=0; k<queueSize-1; k++){
		queueList[k]=queueList[k+1];
	}
	queueList[queueSize]=NULL;

	queueSize--;
}

//finds a pipe for each flow, if one not available, flows wait for a pipe
void requestPipe(flow *item) {
	//lock mutex
    pthread_mutex_lock(&trans_mtx);

    //if transmission pipe available && queue is empty 
    if (inPipe == false && queueSize == 0){
        gettimeofday(&startTran, NULL);
    	printf("Flow %d starts its transmission at time %.2f. \n", item->id, get_time(startTran));

        inPipe = true;
        runningFlow=item->id;
        pthread_mutex_unlock(&trans_mtx);

        return;
    }

    addToList(item);

	while(inPipe==true || item->id != queueList[0]->id){
		printf("Flow %d waits for the finish of flow %2d. \n", item->id, runningFlow);
		pthread_cond_wait(&trans_cvar, &trans_mtx);
	} 
        
    gettimeofday(&startTran, NULL);
	printf("Flow %d starts its transmission at time %.2f. \n", item->id, get_time(startTran));

	runningFlow=queueList[0]->id;
	inPipe=true;

	rmvFromList();

    pthread_mutex_unlock(&trans_mtx); 
}

//sets inPipe to false to notify nothing is in the pipe
//broadcasts to tell other waiting threads that nothing is in the pipe
//prints end transmission time
void releasePipe(flow * item) {
	inPipe=false;

	gettimeofday(&finTran, NULL);
	printf("Flow %d ends its transmission at time %.2f. \n", item->id, get_time(finTran));

	pthread_cond_broadcast(&trans_cvar);

	

}

// entry point for each thread created
void *thrFunction(void *flowItem) {
    flow *item = (flow *)flowItem ;

    // wait for arrival
    usleep(item->arrivalTime*1000000.00);
    printf("Flow %1d arrives: arrival time (%.1f), transmission time (%1.f), priority (%1d). \n", 
    	item->id, item->arrivalTime, item->transTime, item->priority);

    requestPipe(item) ;

    // sleep for transmission time
    usleep(item->transTime*1000000.00);

    releasePipe(item) ;

    return NULL;
}


int main(int argc, char *argv[]) {
	inPipe=false;

	queueSize=0;

    // file handling
    char * token;
    char input[100];

    FILE *fp = fopen(argv[1], "r"); // read fild

    // read number of flows
    
    fgets(input, sizeof(input)-1, fp);
    token=strtok(input,"\n");
    int numFlows=atoi(token);

    int i;
    for(i=0; i<numFlows; i++) {
        fgets(input, sizeof(input)-1, fp); // read line by line
        token = strtok(input,",\n:");
        flowList[i].id=atoi(token);
        token = strtok(NULL, ",\n:");
        flowList[i].arrivalTime=atoi(token)/10.0;
        token = strtok(NULL, ",\n:");
        flowList[i].transTime=atoi(token)/10.0;
        token = strtok(NULL, ",\n:");
        flowList[i].priority=atoi(token);
        token = strtok(NULL, ",\n:");

    }

    fclose(fp); // release file descriptor

    //getting start time
	gettimeofday(&base, NULL);
	start= base.tv_sec+(base.tv_usec/1000000.0);

    for(i=0; i<numFlows; i++){
        // create a thread for each flow 
        pthread_create(&thrList[i], NULL, thrFunction, (void *)&flowList[i]) ;
	}

    // wait for all threads to terminate
    for(i=0; i<numFlows; i++){
        pthread_join(thrList[i], NULL);
	}

    // destroy mutex & condition variable
    pthread_mutex_destroy(&trans_mtx);
    pthread_cond_destroy(&trans_cvar);

    return 0;
}