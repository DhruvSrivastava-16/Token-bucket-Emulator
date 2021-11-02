#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<time.h>
#include<sys/time.h>
#include<pthread.h>
#include<signal.h>
#include <sys/stat.h>
#include<string.h>
#include<math.h>

#include "cs402.h"
#include "my402list.h"




/*------------------------------Global variables: Declarations and Initializations------------------------------*/

/*--------pthread variables--------*/
pthread_mutex_t m =PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv =PTHREAD_COND_INITIALIZER;
pthread_t packet_thread, token_thread, server_1, server_2, signal_thread;

/*----Signal Mask----*/
sigset_t set;

/*--------Emulation Parameters--------*/
double lambda=1.0, mu=0.35,r=1.5;
long packet_inter_arrival_time, token_inter_arrival_time, packet_service_time;
int B=10,P=3,num=20;
char* tsfile;
int num_tokens=0;
int time_to_quit=0;
int no_more_packets=0;
int time_to_quit_gracefully=0;
int packets_arrived=0;
int completed_packets=0;
int dropped_packets=0;
int removed_packets=0;
int dropped_tokens=0;
int total_tokens=0;
My402List queue_1;
My402List queue_2;

/*----Global timestamp and time variables----*/
struct timeval emulation_start_timestamp;
struct timeval emulation_end_timestamp; 
double emulation_start_time=0.0;
double duration_of_emulation;
double total_measured_inter_arrival_time=0.0;
double total_measured_service_time=0.0;
double total_packets_time_in_system=0.0;
double total_packets_time_in_system_square=0.0;
double total_time_in_Q1=0.0;
double total_time_in_Q2=0.0;
double total_time_in_S1=0.0;
double total_time_in_S2=0.0;



/*--------Packet Structure--------*/
typedef struct tagPacket{
	struct timeval arrival_time;
	struct timeval enter_Q1_time;
	struct timeval depart_Q1_time;
	struct timeval enter_Q2_time;
	struct timeval depart_Q2_time;
	struct timeval begin_service_at_server;
	struct timeval leave_server;
	double time_in_Q1; 
	double time_in_Q2;
	double time_in_server;
	double time_in_system;
	int packet_no;
	long inter_arrival_time;
	int tokens_required;
	long service_time;
}Packet;

/*------------------------------Printing and I/O-------------------------------*/

void PrintEmulationStatistics(){

if(packets_arrived==0){
printf("\taverage packet inter-arrival time = N/A- no packets arrived.\n");
}
else{
double avg_packet_inter_arrival_time= total_measured_inter_arrival_time/(packets_arrived);    
printf("\taverage packet inter-arrival time = %.6g\n", avg_packet_inter_arrival_time/1000);
}

if(completed_packets==0){
printf("\taverage packet service time = N/A- No packets were served\n\n");   
}
else{
double avg_service_time= total_measured_service_time/(completed_packets);
printf("\taverage packet service time = %.6g\n\n", avg_service_time/1000);
    
}

double avg_packets_in_Q1 = total_time_in_Q1/duration_of_emulation;
double avg_packets_in_Q2 = total_time_in_Q2/duration_of_emulation;
double avg_packets_in_S1 = total_time_in_S1/duration_of_emulation;
double avg_packets_in_S2 = total_time_in_S2/duration_of_emulation;
printf("\taverage number of packets in Q1 = %.6g\n", avg_packets_in_Q1);
printf("\taverage number of packets in Q2 = %.6g\n", avg_packets_in_Q2);
printf("\taverage number of packets in S1 = %.6g\n", avg_packets_in_S1);
printf("\taverage number of packets in S2 = %.6g\n\n", avg_packets_in_S2);

if(completed_packets==0){
printf("\taverage time a packet spent in system = N/A- no packets were served\n");
printf("\tstandard deviation for time spent in system = N/A- no packets were served\n\n");   
}
else{
double avg_time_packet_in_system = total_packets_time_in_system/(completed_packets);
double std_dev_avg_time_packet_in_system=0.0;
double variance = ((total_packets_time_in_system_square)/(completed_packets))-(avg_time_packet_in_system*avg_time_packet_in_system);
if(variance>=0){
std_dev_avg_time_packet_in_system = sqrt(variance); 
} 
printf("\taverage time a packet spent in system = %.6g\n", avg_time_packet_in_system/1000);
printf("\tstandard deviation for time spent in system = %.6g\n\n", std_dev_avg_time_packet_in_system/1000);  
}
if(total_tokens==0){
printf("\ttoken drop probability = N/A- no tokens arrived\n");  
}
else{
double token_drop_probability = dropped_tokens*1.0/total_tokens;
//printf("No of tokens arrived= %d\n", total_tokens);
printf("\ttoken drop probability = %.6g\n", token_drop_probability);    
}

if(packets_arrived==0){
 printf("\tpacket drop probability = N/A- no packets arrived\n");   
}
else{
  double packet_drop_probability = dropped_packets*1.0/packets_arrived;
  printf("\tpacket drop probability = %.6g\n", packet_drop_probability);  
}

}


/*------------------------------Time Arithematic-------------------------------*/
double TimeElapsed(struct timeval *t1, struct timeval *t2){
	struct timeval res;
	time_t time_elapsed_usec;
	double time_elapsed_msec;
	timersub(t1,t2,&res);
	time_elapsed_usec = res.tv_sec*1000000+ res.tv_usec;
	time_elapsed_msec = time_elapsed_usec/1000.0;
	return time_elapsed_msec;
}
/*------------------------------Cleanup----------------------------------------*/
void CleanupQueue(My402List* Q, int queue_no){
    Packet* packet_to_be_removed;
    struct timeval packet_removal_timestamp;
    double print_timestamp;
    My402ListElem* ptr;
    ptr=My402ListFirst(Q);
  while(ptr!=NULL){
    packet_to_be_removed= (Packet*)ptr->obj;
    My402ListUnlink(Q, ptr);
    removed_packets++;
    gettimeofday(&packet_removal_timestamp, NULL);
    print_timestamp = TimeElapsed(&packet_removal_timestamp,&emulation_start_timestamp);
    printf("%012.3lfms: p%d removed from Q%d\n",print_timestamp,packet_to_be_removed->packet_no, queue_no);
    ptr=My402ListFirst(Q);
  }
}
/*------------------------------Packet Generation------------------------------*/
Packet* NewPacket(int packet_no,int inter_arrival_time_val, int P_val, int service_time_val){
	Packet* newPacket = (Packet*)malloc(sizeof(Packet));
	newPacket->packet_no= packet_no;
	newPacket->inter_arrival_time= inter_arrival_time_val;
	newPacket->tokens_required= P_val;
	newPacket->service_time = service_time_val;

	return newPacket;
}

/*------------------------------Parsing and Validation------------------------------*/
int isValidInteger(char* number_str){

int len_str= strlen(number_str);
for(int i=0; i<len_str; i++){
	if(number_str[i]<48 || number_str[i]>57){
		return 0;
	}
}
return 1;
}

int isValidDouble(char* number_str){
	int len_str= strlen(number_str);
    int decimal_pt_count=0;
	for(int i=0; i<len_str; i++){
		if((number_str[i]<48 && number_str[i]!=46) || number_str[i]>57 ){
			return 0;
		}
        if(number_str[i]==46){
            decimal_pt_count++;
            if(i==0){
                //fprintf(stderr, "Leading Decimal point\n");
                return 0;
            }
            else if(i==len_str-1){
                //fprintf(stderr, "Trailing Decimal point\n");
                return 0;
            }

            /*else{
                if(number_str[i-1]<48 && number_str[i-1]>57){
                    fprintf(stderr, "Non numeric character before the decimal point\n");
                    return 0;
                }
                if(number_str[i+1]<48 && number_str[i+1]>57){
                    fprintf(stderr, "Non numeric character after the decimal point\n");
                    return 0;
                }
            }*/
        }
	}
    //printf("Decimal point count=%d\n", decimal_pt_count);
    if(decimal_pt_count>1){
        //fprintf(stderr, "ERROR: Invalid no. of decimal points\n");
        return 0;
    }
	return 1;
}

int ParseTraceFile(char* line, unsigned int line_no){
    //char* packet_intr_arr_str; 
    //char* P_str;
    //char* packet_serv_str;
    //char buf[strlen(line)];
    char* line_ptr;
    //int space_count=0;
    int i;
	if(sscanf(line, "%ld %d %ld", &packet_inter_arrival_time, &P, &packet_service_time)!=3){
            fprintf(stderr, "ERROR: Malformed Tracefile.\n");
            fprintf(stderr, "(Line no. %d): Invalid values entered for this line.\n",line_no);
            exit(1);
        }
    for(i=0; i<strlen(line);i++){
        if(line[i]=='\t'){
            line[i]=' ';
        }
    }

    line_ptr= line;
    char* value;
    int num_values=0;
    char* space_ptr= strchr(line_ptr,' ');
    while(space_ptr!=NULL){
        if(line_ptr[0]!=' '){
            space_ptr= strchr(line_ptr,' ');
            if(space_ptr!=NULL){
           *space_ptr++='\0';  
            }
            value = line_ptr;
            num_values++;
            //printf("Value= %s\n", value);
            if(!isValidInteger(value)){
            fprintf(stderr, "ERROR: Malformed Tracefile.\n");
            fprintf(stderr, "(Line no. %d): Invalid value entered for field %d of this line.\n",line_no, num_values);
            exit(1);  
            }
            
            line_ptr= space_ptr;
            //space_ptr= strchr(line_ptr,' ');
        }
        else{
            line_ptr++;
        }
    }
    //printf("Line=%s\n",line );
    //printf("Num of values= %d\n", num_values);
    if(num_values!=3){
       fprintf(stderr, "ERROR: Malformed Tracefile.\n");
       fprintf(stderr, "(Line no. %d):Invalid Format- Wrong number of values entered.\n",line_no);
       exit(1);   
    }
	return 1;
}


/*------------------------------Child Thread Routines-----------------------------*/

void* handleSignal(void* signal_mask){
	int sig;
	double print_timestamp;
	struct timeval sigint_caught;
	sigwait(&set,&sig);
	
	pthread_mutex_lock(&m);
	//printf("Signal Thread Got the Mutex.\n");
    gettimeofday(&sigint_caught,NULL);
	print_timestamp = TimeElapsed(&sigint_caught, &emulation_start_timestamp);
	printf("\n%012.3lfms: SIGINT caught, no new packets or tokens will be allowed\n",print_timestamp);
    time_to_quit_gracefully=1;
	pthread_cond_broadcast(&cv);
	//Cancel Packet Thread
	pthread_cancel(packet_thread);
	//Cancel Token Thread
	pthread_cancel(token_thread);
	//Cleanup
	//My402ListUnlinkAll(&queue_1);
	//My402ListUnlinkAll(&queue_2);
	pthread_mutex_unlock(&m);	
	return ((void*)1);
}
void* startPacketThread(void* mode){
pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,0);
int emulationMode = (int)mode;
int packetNo = 0;
double print_timestamp;
double thread_busytime;
long sleep_time;
struct timeval current_time;
struct timeval previous_packet_arrival;
double measured_inter_arrival_time;
My402List* Q1;
My402List* Q2;
Q1 = &queue_1;
Q2= &queue_2;
if(emulationMode==1){
/*Trace Driven Emulation*/
unsigned int line_no=0;
int packetNo = 0;
char line[2000];
FILE *fp;
fp = fopen(tsfile,"r");

      if(!fp){
        fprintf(stderr, "ERROR:Input File- ");
        perror(tsfile);
        exit(1);
      }
      struct stat stat_path;
      if(stat(tsfile,&stat_path)!=0){
        fprintf(stderr, "ERROR: Invalid Input File.\n");
        exit(1);
      }
      if(!S_ISREG(stat_path.st_mode)){
        fprintf(stderr, "ERROR: Invalid Input File.\nInput file %s is a directory.\n",tsfile);
        exit(1);
      }

      while(fgets(line,sizeof(line), fp)){
    	line_no++;
        if(line[strlen(line)-1] != '\n'){
            fprintf(stderr,"ERROR: Invalid Tracefile.\n");
            fprintf(stderr,"(Line no. %d): Invalid Format- Line is not terminating with next line character.\n",line_no);
            exit(1);
        }
        else{
            if(line[strlen(line)-2]==' '){
            fprintf(stderr,"ERROR: Invalid Tracefile.\n");
            fprintf(stderr,"(Line no. %d): Invalid Format- Line has trailing whitespaces.\n",line_no);
            exit(1);   
            }
        }

        if(line[0]==' '){
          fprintf(stderr,"ERROR: Invalid Tracefile.\n");
          fprintf(stderr,"(Line no. %d): Invalid Format- Line has leading whitespaces.\n",line_no);
          exit(1);  
        }
    	line[strlen(line)-1] ='\0';
    	if(strlen(line)>1024){
      /*error: user entered more than 1024 character for a line.*/
        fprintf(stderr,"ERROR: Invalid Tracefile.\n");
        fprintf(stderr,"(Line no. %d): Invalid Format-user exceeded max number of characters allowed for a line.\nMaximum number of characters allowed for an input line are 1024 characters.\n",line_no);
        exit(1);
    }
    if(line_no==1){
		if(isValidInteger(line)!=1){
			fprintf(stderr, "ERROR: Malformed Tracefile.\n");
  			fprintf(stderr, "(Line no. %d): Invalid Number of Packets field- non integer characters present.\n",line_no);
  			exit(1);
		}
		num = atoi(line);
		continue;
	}

    if(!ParseTraceFile(line,line_no)){
    	/*error in parsing*/
      	exit(1);
    }
    //Sleep
    if(packetNo==0){
    	gettimeofday(&current_time,NULL);
    	thread_busytime = TimeElapsed(&current_time,&emulation_start_timestamp);
    	//printf("Packet Thread busytime=%lf\n",thread_busytime);
    	sleep_time= (packet_inter_arrival_time-thread_busytime)*1000;
    	//printf("Packet Thread Sleep Time=%u\n",sleep_time);
    }
    else{
    	gettimeofday(&current_time,NULL);
    	thread_busytime = TimeElapsed(&current_time,&previous_packet_arrival);
    	//printf("Packet Thread busytime=%lf\n",thread_busytime);
    	sleep_time= (packet_inter_arrival_time-thread_busytime)*1000; ///-(thread_busytime*1000);
    	//printf("Packet Thread Sleep Time=%u\n",sleep_time);

    }
    
    if(sleep_time>0){
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,0);
        usleep(sleep_time);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,0);   
        }
        else{
           pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,0);
           pthread_testcancel();
           pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,0); 
        }
    packetNo++;
    packets_arrived++;
    Packet* packet = NewPacket(packetNo, packet_inter_arrival_time*1000, P, packet_service_time*1000);
    
    pthread_mutex_lock(&m);
    gettimeofday(&(packet->arrival_time),NULL);
    if(packetNo==1){
    measured_inter_arrival_time = TimeElapsed(&(packet->arrival_time), &emulation_start_timestamp);	
    }
    else{
	measured_inter_arrival_time = TimeElapsed(&(packet->arrival_time), &previous_packet_arrival);
    }
    total_measured_inter_arrival_time+= measured_inter_arrival_time;
    previous_packet_arrival= packet->arrival_time;
    print_timestamp = TimeElapsed(&(packet->arrival_time), &emulation_start_timestamp);
    printf("%012.3lfms: p%d arrives, needs %d tokens, inter-arrival time = %.3lfms",print_timestamp,packetNo,packet->tokens_required, measured_inter_arrival_time);
    /*printf("\n\tPacket no.=%d\n", packet->packet_no);
    printf("\tInter Arrival Time=%ld\n", packet->inter_arrival_time);
    printf("\tTokens Required=%d\n", packet->tokens_required);
    printf("\tRequested Service Time=%ld\n\n", packet->service_time);*/
    if(packet->tokens_required>B){
    	printf(", dropped\n");
    	dropped_packets++;
    	pthread_mutex_unlock(&m);
    	continue;
    }
    else{
    	printf("\n");
    }

    if(My402ListAppend(Q1,(void*)packet)!=1){
      /*error in Append*/
      fprintf(stderr,"ERROR: Append.\n");
      fprintf(stderr, "Error in enqueing packet no. %d into Q1.", packet->packet_no);
      exit(1);
    }
    gettimeofday(&(packet->enter_Q1_time),NULL);
    print_timestamp = TimeElapsed(&(packet->enter_Q1_time), &emulation_start_timestamp);
    printf("%012.3lfms: p%d enters Q1\n",print_timestamp, packet->packet_no);
    if(Q1->num_members==1){
    	if(packet->tokens_required<= num_tokens){
    		num_tokens= num_tokens-(packet->tokens_required);
    		My402ListUnlink(Q1, My402ListFirst(Q1));
    		gettimeofday(&(packet->depart_Q1_time),NULL);
    		print_timestamp = TimeElapsed(&(packet->depart_Q1_time), &emulation_start_timestamp);
    		packet->time_in_Q1 = TimeElapsed(&(packet->depart_Q1_time),&(packet->enter_Q1_time));
    		
    		printf("%012.3lfms: p%d leaves Q1, time in Q1 = %.3lfms, token bucket now has %d tokens\n",print_timestamp,packetNo, packet->time_in_Q1, num_tokens);
    	if(My402ListAppend(Q2,(void*)packet)!=1){
      /*error in Append*/
      fprintf(stderr,"ERROR: Append.\n");
      fprintf(stderr, "Error in enqueing packet no. %d into Q2.", packet->packet_no);
      exit(1);
    }
    gettimeofday(&(packet->enter_Q2_time),NULL);
    print_timestamp = TimeElapsed(&(packet->enter_Q2_time), &emulation_start_timestamp);
    printf("%012.3lfms: p%d enters Q2\n",print_timestamp, packet->packet_no);
    pthread_cond_broadcast(&cv);
    	}
    	
    }
   pthread_mutex_unlock(&m);
}
//printf("No of Lines= %d\n", line_no);
if(line_no-1!=num){
fprintf(stderr, "ERROR: Malformed Tracefile.\n");
fprintf(stderr, "Invalid Format- %d number of packets are mentioned but parameter values for %d packets is given.\n",num, line_no-1);
exit(1);
}
}

else if(emulationMode==0){
	/*Deterministic Mode*/
	for(int i=1;i<=num;i++){
		
		 if(i==1){
    	gettimeofday(&current_time,NULL);
    	thread_busytime = TimeElapsed(&current_time,&emulation_start_timestamp);
    	//printf("Packet Thread busytime=%lf\n",thread_busytime);
    	sleep_time= (packet_inter_arrival_time)-(thread_busytime*1000.0);
    	//printf("Packet Thread Sleep Time=%ld\n",sleep_time);
    }
    else{
    	gettimeofday(&current_time,NULL);
    	thread_busytime = TimeElapsed(&current_time,&previous_packet_arrival);
    	//printf("Packet Thread busytime=%lf\n",thread_busytime);
    	sleep_time= (packet_inter_arrival_time)-(thread_busytime*1000.0); ///-(thread_busytime*1000);
    	//printf("Packet Thread Sleep Time=%ld\n",sleep_time);

    }
        if(sleep_time>0){
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,0);
        usleep(sleep_time);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,0);   
        }
        else{
           pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,0);
           pthread_testcancel();
           pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,0); 
        }

		packetNo=i;
        packets_arrived=i;
		Packet* packet = NewPacket(packetNo, packet_inter_arrival_time, P, packet_service_time);

    	pthread_mutex_lock(&m);
	gettimeofday(&(packet->arrival_time),NULL);
    if(packetNo==1){
    measured_inter_arrival_time = TimeElapsed(&(packet->arrival_time), &emulation_start_timestamp);	
    }
    else{
	measured_inter_arrival_time = TimeElapsed(&(packet->arrival_time), &previous_packet_arrival);
    }
    /*printf("%012.3lfms: p%d arrives, needs %d tokens, inter-arrival time = %.3lfms",print_timestamp,packetNo,packet->tokens_required, measured_inter_arrival_time);
    printf("\n\tPacket no.=%d\n", packet->packet_no);
    printf("\tInter Arrival Time=%ld\n", packet->inter_arrival_time);
    printf("\tTokens Required=%d\n", packet->tokens_required);
    printf("\tRequested Service Time=%ld\n\n", packet->service_time);*/
    total_measured_inter_arrival_time+= measured_inter_arrival_time;
    previous_packet_arrival= packet->arrival_time;
    print_timestamp = TimeElapsed(&(packet->arrival_time), &emulation_start_timestamp);
    printf("%012.3lfms: p%d arrives, needs %d tokens, inter-arrival time = %.3lfms",print_timestamp,packetNo,packet->tokens_required, measured_inter_arrival_time);
    
    if(packet->tokens_required>B){
    	printf(", dropped\n");
        dropped_packets++;
    	pthread_mutex_unlock(&m);
    	continue;
    }
    else{
    	printf("\n");
    }
    	if(My402ListAppend(Q1,(void*)packet)!=1){
      /*error in Append*/
      fprintf(stderr,"ERROR: Append.\n");
      fprintf(stderr, "Error in enqueing packet no. %d into Q1.", packet->packet_no);
      exit(1);
    }
    
    gettimeofday(&(packet->enter_Q1_time),NULL);
    print_timestamp = TimeElapsed(&(packet->enter_Q1_time), &emulation_start_timestamp);
    printf("%012.3lfms: p%d enters Q1\n",print_timestamp, packet->packet_no);

    if(Q1->num_members==1){
    	if(packet->tokens_required<= num_tokens){
    		num_tokens= num_tokens-packet->tokens_required;
    		My402ListUnlink(Q1, My402ListFirst(Q1));
    		gettimeofday(&(packet->depart_Q1_time),NULL);
    		print_timestamp = TimeElapsed(&(packet->depart_Q1_time), &emulation_start_timestamp);
    		packet->time_in_Q1 = TimeElapsed(&(packet->depart_Q1_time),&(packet->enter_Q1_time));
    		printf("%012.3lfms: p%d leaves Q1, time in Q1 = %.3lfms, token bucket now has %d token\n",print_timestamp,packetNo, packet->time_in_Q1, num_tokens);
    	if(My402ListAppend(Q2,(void*)packet)!=1){
      /*error in Append*/
      fprintf(stderr,"ERROR: Append.\n");
      fprintf(stderr, "Error in enqueing packet no. %d into Q2.",packet->packet_no);
      exit(1);
    }
    gettimeofday(&(packet->enter_Q2_time),NULL);
    print_timestamp = TimeElapsed(&(packet->enter_Q2_time), &emulation_start_timestamp);
    printf("%012.3lfms: p%d enters Q2\n",print_timestamp, packet->packet_no);
    pthread_cond_broadcast(&cv);
    	}
    	
    }
    	pthread_mutex_unlock(&m);
	}
}
no_more_packets=1;
pthread_exit((void*)1);
}

void* startTokenThread(void* arg1){
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,0);
	My402ListElem* head_Q1;
	Packet* first_packet; 
	My402List* Q1;
	My402List* Q2;
	int tokenNo;
	double print_timestamp;
	long sleep_time;
	struct timeval token_arrival;
	struct timeval previous_token_arrival;
	struct timeval current_time;
	double token_thread_busy_time;
	for(;;){ 

	if(tokenNo==0){
        gettimeofday(&current_time,NULL);
        token_thread_busy_time= TimeElapsed(&current_time,&emulation_start_timestamp);
        //printf("Token Thread busytime=%lf\n",token_thread_busy_time);
    	sleep_time= (token_inter_arrival_time)-(round(token_thread_busy_time*1000));
        //printf("Sleep Time for Token Thread=%ld\n",sleep_time);
    }
    else{
    	gettimeofday(&current_time,NULL);
    	token_thread_busy_time = TimeElapsed(&current_time,&previous_token_arrival);
    	//printf("Token Thread busytime=%lf\n",token_thread_busy_time);
    	sleep_time= (token_inter_arrival_time)-(round(token_thread_busy_time*1000)); ///-(thread_busytime*1000);
        //printf("Sleep Time for Token Thread=%ld\n",sleep_time);
    }
    if(sleep_time>0){
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,0);
    usleep(sleep_time);
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,0);   
    }
    else{
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,0);
    pthread_testcancel();
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,0);
    }
    
	pthread_mutex_lock(&m);
    Q1 = &queue_1;
    Q2= &queue_2;
    if(Q1->num_members==0){
        if(no_more_packets){
            time_to_quit=1;
            pthread_cond_broadcast(&cv);
            pthread_mutex_unlock(&m);
            break;
        }
    }
	total_tokens++;
	tokenNo++;
	gettimeofday(&token_arrival,NULL);
	previous_token_arrival = token_arrival;
	print_timestamp = TimeElapsed(&token_arrival, &emulation_start_timestamp);
	if(num_tokens<B){
		num_tokens++; //Token Arrival event
		printf("%012.3lfms: token t%d arrives, token bucket now has %d tokens\n",print_timestamp, tokenNo, num_tokens);
	}
	else{
		dropped_tokens++;
		printf("%012.3lfms: token t%d arrives, dropped\n",print_timestamp, tokenNo);
	}
	
	if(Q1->num_members>0){
	head_Q1= My402ListFirst(Q1);
	first_packet = (Packet*)head_Q1->obj;
	if(first_packet->tokens_required <=num_tokens){
		My402ListUnlink(Q1, head_Q1);
		num_tokens=0;
		gettimeofday(&(first_packet->depart_Q1_time),NULL);
		print_timestamp = TimeElapsed(&(first_packet->depart_Q1_time), &emulation_start_timestamp);
		first_packet->time_in_Q1 = TimeElapsed(&(first_packet->depart_Q1_time),&(first_packet->enter_Q1_time));
		printf("%012.3lfms: p%d leaves Q1, time in Q1 = %.3lfms, token bucket now has %d token\n",print_timestamp,first_packet->packet_no, first_packet->time_in_Q1, num_tokens);
		
		/*-------Packet Enters Q2------*/
		if(My402ListAppend(Q2,(void*)first_packet)!=1){
      		/*error in Append*/
      fprintf(stderr,"ERROR: Append.\n");
      fprintf(stderr, "Error in enqueing packet no. %d into Q2.",first_packet->packet_no);
      exit(1);
    }
    gettimeofday(&(first_packet->enter_Q2_time),NULL);
    print_timestamp = TimeElapsed(&(first_packet->enter_Q2_time), &emulation_start_timestamp);
    printf("%012.3lfms: p%d enters Q2\n",print_timestamp, first_packet->packet_no);

    pthread_cond_broadcast(&cv); //Waking Up the Server Threads


    if(no_more_packets && My402ListLength(Q1)==0){
    	time_to_quit=1;
    	pthread_mutex_unlock(&m);
    	break;
    }
	}	
	}
	
	pthread_mutex_unlock(&m);	
	}

	pthread_exit((void*)1);	 
}

void* startServers(void* server_no){
	/*Variables Declaration*/
	My402List* Q2;
	Packet* packet_to_be_serviced;
	My402ListElem* head_Q2;
	double serviceTime;
	double print_timestamp;
	int sleep_time;
	double measured_service_time;
	int serverNo= (int)server_no;

	for(;;){

	pthread_mutex_lock(&m);
	Q2 = &queue_2;
	while(My402ListLength(Q2)==0 && !time_to_quit && !time_to_quit_gracefully){
		pthread_cond_wait(&cv,&m);
	}

	/*If time to quite due to user abort interrupt- quit gracefully*/
	if(time_to_quit_gracefully){
		pthread_cond_broadcast(&cv);
		pthread_mutex_unlock(&m);
		break;
	}

	/*If time to quit due to end of emulation-quit*/
	if(time_to_quit && My402ListLength(Q2)==0){
		pthread_cond_broadcast(&cv);
		pthread_mutex_unlock(&m);
		break;
	}

	/*Service Head Packet of Q2*/
	head_Q2 = My402ListFirst(Q2);
	packet_to_be_serviced = (Packet*)head_Q2->obj;
	serviceTime = (packet_to_be_serviced->service_time)/1000.0;
	sleep_time = packet_to_be_serviced->service_time;
	
	/*DEQUE- Packet to be serviced from Q2 */

	My402ListUnlink(Q2, head_Q2);
	gettimeofday(&(packet_to_be_serviced->depart_Q2_time),NULL);
	print_timestamp = TimeElapsed(&(packet_to_be_serviced->depart_Q2_time), &emulation_start_timestamp);
	packet_to_be_serviced->time_in_Q2 = TimeElapsed(&(packet_to_be_serviced->depart_Q2_time),&(packet_to_be_serviced->enter_Q2_time));
	printf("%012.3lfms: p%d leaves Q2, time in Q2 = %.3lfms\n",print_timestamp,packet_to_be_serviced->packet_no, packet_to_be_serviced->time_in_Q2);
	
	/*Begin service at server*/
	gettimeofday(&(packet_to_be_serviced->begin_service_at_server),NULL);
	print_timestamp = TimeElapsed(&(packet_to_be_serviced->begin_service_at_server), &emulation_start_timestamp);
	printf("%012.3lfms: p%d begins service at S%d, requesting %.0lfms of service\n",print_timestamp,packet_to_be_serviced->packet_no, serverNo, serviceTime);
	if(sleep_time>0){
    pthread_mutex_unlock(&m);
    usleep(sleep_time);
    pthread_mutex_lock(&m);   
    }

	/*Packet Leaves Server*/
    completed_packets++;
	gettimeofday(&(packet_to_be_serviced->leave_server),NULL);
	print_timestamp = TimeElapsed(&(packet_to_be_serviced->leave_server), &emulation_start_timestamp);
	packet_to_be_serviced->time_in_server = TimeElapsed(&(packet_to_be_serviced->leave_server), &(packet_to_be_serviced->begin_service_at_server));
	measured_service_time = packet_to_be_serviced->time_in_server;
    
    /*Packet Time in System*/
    packet_to_be_serviced->time_in_system = TimeElapsed(&(packet_to_be_serviced->leave_server), &(packet_to_be_serviced->arrival_time));
	printf("%012.3lfms: p%d departs from S%d, service time = %.3lfms, time in system  = %.3lfms\n", print_timestamp, packet_to_be_serviced->packet_no, serverNo, measured_service_time,packet_to_be_serviced->time_in_system);

    /*--------Running Sums--------*/
    total_time_in_Q1+= packet_to_be_serviced->time_in_Q1;
    total_time_in_Q2+=packet_to_be_serviced->time_in_Q2;
    total_measured_service_time+= measured_service_time;
	if(serverNo==1){
		total_time_in_S1+= packet_to_be_serviced->time_in_server;
	}
	else if(serverNo==2){
		total_time_in_S2+= packet_to_be_serviced->time_in_server;
	}

	total_packets_time_in_system+= packet_to_be_serviced->time_in_system;
	total_packets_time_in_system_square+=((packet_to_be_serviced->time_in_system)*(packet_to_be_serviced->time_in_system));
	
	pthread_mutex_unlock(&m);
	}

	pthread_exit((void*)1);

}

/*------------------------------Start Emulation------------------------------*/
void StartEmulation(int mode){

    /*--------Printing Emulation Parameters--------*/
	printf("\nEmulation Parameters:\n");
	token_inter_arrival_time= (round((1.0/r)*1000))*(1000);
    if(token_inter_arrival_time>10000000){
        token_inter_arrival_time= 10000000;
    }
	//printf("token arrival time= %u\n", token_inter_arrival_time);
	if(mode==0){
        //Deterministic Mode
	printf("\tnumber to arrive= %d\n",num);
	printf("\tlambda= %0.6g\n",lambda);
	printf("\tmu= %0.6g\n",mu);
	printf("\tP= %d\n",P);
	packet_inter_arrival_time= (round((1.0/lambda)*1000))*(1000);
	if(packet_inter_arrival_time>10000000){
			packet_inter_arrival_time=10000000;
		}
    if(packet_inter_arrival_time)
	packet_service_time= (round((1.0/mu)*1000))*(1000);
	if(packet_service_time>10000000){
			packet_service_time=10000000;
		}
	//printf("Packet Inter Arrival Time= %ldus\n",packet_inter_arrival_time);
	
	//printf("Packet Service Time= %ldus\n",packet_service_time);
	}
    //printf("Token Inter Arrival Time= %ldus\n",token_inter_arrival_time);
	printf("\tr= %0.6g\n",r);
	printf("\tB= %d\n",B);

	if(mode==1){
        //Trace Driven Mode
		//printf("Trace Driven Mode\n");
		printf("\ttsfile= %s\n", tsfile);
	}

    /*--------Queue Initializations--------*/
    	if (My402ListInit(&queue_1)!=1)
    {
    	/*error*/
        fprintf(stderr, "ERROR: Q1 Initialization.\n");
        exit(1);
    }
    if (My402ListInit(&queue_2)!=1)
    {
    	/*error*/
        fprintf(stderr, "ERROR: Q2 Initialization.\n");
        exit(1);
    }
    /*--------Thread Return Codes--------*/
	void* packetThreadReturnCode =  (void*)0;
	void* tokenThreadReturnCode =  (void*)0;
	void* server1ReturnCode =  (void*)0;
	void* server2ReturnCode =  (void*)0;
	//void* signalThreadReturnCode = (void*)0;

    /*--------Signal Masking--------*/
	sigemptyset(&set);
	sigaddset(&set,SIGINT);
	sigprocmask(SIG_BLOCK, &set, 0);

    /*--------Emulation Begins--------*/
	gettimeofday(&emulation_start_timestamp, NULL);
    emulation_start_time= TimeElapsed(&emulation_start_timestamp, &emulation_start_timestamp);
	printf("\n%012.3lfms: emulation begins\n", emulation_start_time);

	/*--------Creating and Joining Threads--------*/
    //pthread_t packet_thread, token_thread, server_1, server_2, signal_thread;
	pthread_create(&packet_thread,0, startPacketThread, (void*)mode);
	pthread_create(&token_thread,0, startTokenThread, (void*)0);
	pthread_create(&server_1,0, startServers, (void*)1);
	pthread_create(&server_2,0, startServers, (void*)2);
	pthread_create(&signal_thread,0,handleSignal, (void*)0);

	pthread_join(packet_thread, &packetThreadReturnCode);
	pthread_join(token_thread, &tokenThreadReturnCode);
	pthread_join(server_1, &server1ReturnCode);
	pthread_join(server_2, &server2ReturnCode);
	//pthread_join(signal_thread, &signalThreadReturnCode);
    
    /*--------Cleanup--------*/
    //printf("num of elements in queue 1: %d\n", My402ListLength(&queue_1));
    //printf("num of elements in queue 2: %d\n", My402ListLength(&queue_2));
    if(My402ListLength(&queue_1)!=0){
        CleanupQueue(&queue_1,1);   
    }
    if(My402ListLength(&queue_2)!=0){
        CleanupQueue(&queue_2,2);   
    }
    /*--------End of Emulation--------*/
	gettimeofday(&emulation_end_timestamp,NULL);
	duration_of_emulation= TimeElapsed(&emulation_end_timestamp, &emulation_start_timestamp);
	printf("%012.3lfms: emulation ends\n", duration_of_emulation);
    //printf("num of elements in queue 1: %d\n", My402ListLength(&queue_1));
    //printf("num of elements in queue 2: %d\n", My402ListLength(&queue_2));
    /*if(My402ListEmpty(&queue_1)){
        printf("Queue 1 all cleaned up\n");
    }*/
    /*if(My402ListEmpty(&queue_2)){
        printf("Queue 2 all cleaned up\n");
    }*/

    /*------Printing Statistics-------*/
	printf("\nStatistics:\n");
	PrintEmulationStatistics();

}

/*------------------------------main()------------------------------*/
int main(int argc, char* argv[]){
	int i;
	int emulation_mode=0;
	//printf("argc=%d\n",argc);
	for(i=1;i<argc;i++){
		if(argv[i][0]=='-'){
			if(strcmp(argv[i],"-lambda")==0){
                if(i==argc-1){
                fprintf(stderr, "ERROR: Malformed Command- Missing lambda value.\nValue for '-lambda' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);  
            }
		if(strcmp(argv[i+1],"")==0){
                fprintf(stderr, "ERROR: Malformed Command- Missing lambda value.\nValue for '-lambda' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);	
		}
		if(argv[i+1][0]!='-'){
            if(!isValidDouble(argv[i+1])){
                fprintf(stderr, "ERROR: Invalid value entered for -lambda option.\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);
            }
			if(sscanf(argv[i+1],"%lf",&lambda)!=1){
					/*error- invalid lambda value*/
				fprintf(stderr, "ERROR: Invalid value entered for -lambda option.\n");
				fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
				exit(1);
			}
			
		}
		else{
				/*error- missing lambda value.*/
				fprintf(stderr, "ERROR: Malformed Command- Missing lambda value.\nValue for '-lambda' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);
			}
		}
		else if(strcmp(argv[i],"-mu")==0){
            if(i==argc-1){
                fprintf(stderr, "ERROR: Malformed Command- Missing mu value.\nValue for '-mu' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);  
            }
			if(strcmp(argv[i+1],"")==0){
                fprintf(stderr, "ERROR: Malformed Command- Missing mu value.\nValue for '-mu' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);
		}
			if(argv[i+1][0]!='-'){
                if(!isValidDouble(argv[i+1])){
                fprintf(stderr, "ERROR: Invalid value entered for -lambda option.\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);
            }
				if(sscanf(argv[i+1],"%lf",&mu)!=1){
					/*error- invalid mu value*/
				fprintf(stderr, "ERROR: Invalid value entered for -mu option.\n");
				fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
				exit(1);
				}
				
			}
			else{
			/*error- missing mu value.*/
				fprintf(stderr, "ERROR: Malformed Command- Missing mu value.\nValue for '-mu' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);
			}
		}
		else if(strcmp(argv[i],"-r")==0){
            if(i==argc-1){
                fprintf(stderr, "ERROR: Malformed Command- Missing r value.\nValue for '-r' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);  
            }
			if(strcmp(argv[i+1],"")==0){
                fprintf(stderr, "ERROR: Malformed Command- Missing r value.\nValue for '-r' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);	
		}
			if(argv[i+1][0]!='-'){
                if(!isValidDouble(argv[i+1])){
                fprintf(stderr, "ERROR: Invalid value entered for -lambda option.\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);
            }
				if(sscanf(argv[i+1],"%lf",&r)!=1){
						/*error- invalid r value*/
				fprintf(stderr, "ERROR: Invalid value entered for -r option.\n");
				fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
				exit(1);
				}
				
			}
			else{
				/*error- missing r value.*/
				fprintf(stderr, "ERROR: Malformed Command- Missing r value.\nValue for '-r' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);
			}
		}
		else if(strcmp(argv[i],"-B")==0){
            if(i==argc-1){
                fprintf(stderr, "ERROR: Malformed Command- Missing B value.\nValue for '-B' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);  
            }
			if(strcmp(argv[i+1],"")==0){
                fprintf(stderr, "ERROR: Malformed Command- Missing B value.\nValue for '-B' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);	
		}
			if(argv[i+1][0]!='-'){
                if(!isValidInteger(argv[i+1])){
                fprintf(stderr, "ERROR: Invalid value entered for -lambda option.\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);
            }
				if(sscanf(argv[i+1],"%d",&B)!=1){
				/*error- invalid B value.*/
				fprintf(stderr, "ERROR: Invalid value entered for -B option.\n");
				fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
				exit(1);
				}
			
			}
			else{
				/*error- missing B value.*/
				fprintf(stderr, "ERROR: Malformed Command- Missing B value.\nValue for '-B' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);
			}
		}
		else if(strcmp(argv[i],"-P")==0){
            if(i==argc-1){
                fprintf(stderr, "ERROR: Malformed Command- Missing P value.\nValue for '-P' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);  
            }
			if(strcmp(argv[i+1],"")==0){
                fprintf(stderr, "ERROR: Malformed Command- Missing P value.\nValue for '-P' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);	
		}
			if(argv[i+1][0]!='-'){
                if(!isValidInteger(argv[i+1])){
                fprintf(stderr, "ERROR: Invalid value entered for -lambda option.\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);
            }
				if(sscanf(argv[i+1],"%d",&P)!=1){
					/*error- invalid P value*/
				fprintf(stderr, "ERROR: Invalid value entered for -P option.\n");
				fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
				exit(1);
				}
			}
			else{
				/*error- missing P value.*/
				fprintf(stderr, "ERROR: Malformed Command- Missing P value.\nValue for '-P' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);
			}
		}
		else if(strcmp(argv[i],"-n")==0){
            if(i==argc-1){
                fprintf(stderr, "ERROR: Malformed Command- Missing n value.\nValue for '-n' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);
            }
			if(strcmp(argv[i+1],"")==0){
                fprintf(stderr, "ERROR: Malformed Command- Missing n value.\nValue for '-n' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);
		}
			if(argv[i+1][0]!='-'){
                if(!isValidInteger(argv[i+1])){
                fprintf(stderr, "ERROR: Invalid value entered for -lambda option.\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);
            }
				if(sscanf(argv[i+1],"%d",&num)!=1){
					/*error- invalid num value*/
				fprintf(stderr, "ERROR: Malformed Command, value entered for '-n' is not valid.\n");
				fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
				exit(1);
				}
			}
			else{
				/*error- missing  num value*/
				fprintf(stderr, "ERROR: Malformed Command- Missing n value.\nValue for '-n' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);
			}
		}
		else if(strcmp(argv[i],"-t")==0){
            if(i==argc-1){
                fprintf(stderr, "ERROR: Malformed Command- Missing tracefile.\nTracefile for '-t' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);  
            }
			if(strcmp(argv[i+1],"")==0){
		      fprintf(stderr, "ERROR: Malformed Command- Missing tracefile.\nTracefile for '-t' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);	
		}
			if(argv[i+1][0]!='-'){
				tsfile = argv[i+1];
				emulation_mode=1; //Trace-Driven Mode
                i++;
			}
			else{
				/*error- tsfile is missing.*/
				fprintf(stderr, "ERROR: Malformed Command- Missing tracefile.\nTracefile for '-t' option is not given\n");
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);
			}
		}
		else{
				fprintf(stderr, "ERROR: Malformed Command, '%s' is not a valid commandline option.\n", argv[i]);
				fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
				exit(1);	
		}
		}

        else{

            if(!isValidDouble(argv[i])){
                fprintf(stderr, "ERROR: Malformed Command, '%s' is not a valid commandline option.\n", argv[i]);
                fprintf(stderr, "Usage: ./warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-num num] [-t tsfile]\n");
                exit(1);
            }
        

        }
	}
	
	StartEmulation(emulation_mode);
	return 0;	
}
