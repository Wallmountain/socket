#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>

#define ERR_EXIT(m) \
        do \
        { \
                perror(m); \
                exit(EXIT_FAILURE); \
        } while(0)
int sockfd;

void tcpsend(int portno, char* filename);
void tcprecv(int portno, char* IP);
void udpsend(int portno,char *filename);
void udprecv(int portno);
void error(const char *msg);
void change(char* buffer, long int byte);
struct timespec diff(struct timespec start, struct timespec end);
long int Pow(int a);
long int stringtoint(char* buffer);

int main(int argc, char *argv[]){
	// classify tcp ,uap and send ,recv 
    if (argc == 6 && !memcmp(argv[2],"send", sizeof(char)*5)){
        if (!memcmp(argv[1],"tcp", sizeof(char)*4)) tcpsend(atoi(argv[4]), argv[5]);
        else if (!memcmp(argv[1],"udp", sizeof(char)*4)) udpsend(atoi(argv[4]), argv[5]);
        else error("ERROR, transport usage\n");
    }
    else if (argc == 5 && !memcmp(argv[2],"recv", sizeof(char)*5)){
        if (!memcmp(argv[1],"tcp", sizeof(char)*4)) tcprecv(atoi(argv[4]), argv[3]);
        else if (!memcmp(argv[1],"udp", sizeof(char)*4)) udprecv(atoi(argv[4]));
        else error("ERROR, transport usage\n");
    }
    else error("ERROR, wrong usage\n");
    return 0;
}

// for tcp server
void tcpsend(int portno, char* filename){
    int newsockfd;
    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    
    if(access(filename,0)<0)  // Check if there is this file
        error("no this file");
        
    sockfd = socket(AF_INET, SOCK_STREAM, 0); //socket create 
    if (sockfd < 0)
        error("ERROR opening socket");
    
	// assign IP and port    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    // bind new socket 
    if (bind(sockfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR on binding");
        
    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    
    while(1){
    // server always runs to continue providing services to client 
        // accept data pockage from client
        newsockfd = accept(sockfd,(struct sockaddr *) & cli_addr,&clilen);
        if (newsockfd < 0)
            error("ERROR on accept");
        bzero(buffer,256);
        
        // send filename to client
        memcpy(buffer, filename, sizeof(filename)); 
        send(newsockfd, buffer, 255, 0);
        bzero(buffer, 256);
        
        FILE *file_fp = fopen(filename, "r"); // open file
        
		// calculate the size of file
        fseek(file_fp, 0 , SEEK_END);
        memset(buffer,'a',255);
        change(buffer, ftell(file_fp)); // change long int to string
        fseek(file_fp, 0, SEEK_SET);
        send(newsockfd, buffer, 255, 0); // send file size to client 
        
		bzero(buffer,256);
        int file_block_length;
        
        if(!file_fp)
            error(" open file failure\n");
	    else while( (file_block_length = fread(buffer, sizeof(char), 255, file_fp)) > 0){
             // if there is data in file , send them to client. 
            if (send(newsockfd, buffer, file_block_length, 0) < 0)
                 error("send file failed!\n");
             bzero(buffer, sizeof(buffer));
            }

        fclose(file_fp);
        close(newsockfd);
     }
     close(sockfd);
}

// for tcp recv
void tcprecv(int portno, char* IP){
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[256];
    long int step = 0;

    sockfd = socket(AF_INET, SOCK_STREAM, 0); //socket create
    if (sockfd < 0)
        error("ERROR opening socket");
        
    server = gethostbyname(IP);
    if (server == NULL)
        error("ERROR, no such host\n");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    // assign IP and port  
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    // connect to client socket
    if (connect(sockfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) <0)
        error("ERROR connecting");
    bzero(buffer, 256);
    
    // receive filename
    int length = recv(sockfd, buffer, 255, 0); 
    char *filename = malloc(length+1);
    memcpy(filename,buffer,length);
    bzero(buffer, 256);
    
    // the variable to restore time
    time_t now;
    struct timespec start, end, temp;
    int progress = 1;
    struct tm *timenow;

    time(&now); // time function reads the current time
    clock_gettime(CLOCK_MONOTONIC, &start); // calculate start time
    timenow = localtime(&now); // localtime function to get computer time from time
    printf("0%% %s", asctime(timenow));// asctime fuction to change time into string

    recv(sockfd, buffer, 255, 0); // receive file size
    long int file_size = stringtoint(buffer); // change it into long int 
    bzero(buffer, 256);

    FILE *fp = fopen(filename, "w+"); // open file
    if (!fp)
        error("File Can Not Open To Write!\n");
    // receive data from server into the buffer
    int readlength;
    while(readlength = recv(sockfd, buffer, 255, 0)){
        fwrite(buffer, sizeof(char), readlength, fp);// write content to file
        bzero(buffer, 256);
        step+=readlength; // record the file size currently received
        // check if the currently received file size has reached the record point
        while(progress<4){
            if(file_size*progress/4<step){
                time(&now);  // time function reads the current time 
                timenow = localtime(&now);  // localtime function to get computer time from time
                printf("%d%% %s", progress*25, asctime(timenow));  // asctime fuction to change time into string
                progress++;
            }
            else break;
        }
    }

    time(&now);  // time function reads the current time
    clock_gettime(CLOCK_MONOTONIC, &end); // calculate end time
    timenow = localtime(&now);  // localtime function to get computer time from time

    // calculate the actual time spent
    temp = diff(start, end);
    double dif = temp.tv_sec +(double)temp.tv_nsec/1000000000.0;
    
	// print out the final result
    printf("100%% %s", asctime(timenow));  // asctime fuction to change time into string
    printf("Total trans time: %fs\nfile size : %ldMB\n",dif,file_size/1024/1024);

    // close socket and file
    fclose(fp);
    close(sockfd);
}

// long int to string 
void change(char* buffer, long int byte){
    int index=0;
    while(byte!=0){
        *(buffer+index++)=byte%10+'0';
        byte/=10;
    }
}

// 10 pow
long int Pow(int a){
    if(a==0)return 1;
    return 10*Pow(a-1);
}

// string to long int
long int stringtoint(char* buffer){
    int total=*buffer-'0', index=0;
    while(*(buffer+(++index))!='a')
        total += Pow(index) * (*(buffer+index)-'0');
    return total;
}

// the function to calculate time difference 
struct timespec diff(struct timespec start, struct timespec end) {
  struct timespec temp;
  if ((end.tv_nsec-start.tv_nsec)<0) {
    temp.tv_sec = end.tv_sec-start.tv_sec-1;
    temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
  } else {
    temp.tv_sec = end.tv_sec-start.tv_sec;
    temp.tv_nsec = end.tv_nsec-start.tv_nsec;
  }
  return temp;
}

// for udp send
void udpsend(int portno,char *filename){
	// socket create
    if ((sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        ERR_EXIT("socket error");
        
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    int servlen = sizeof(servaddr);
    // assign IP and port
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(portno);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    // bind new socket
    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        ERR_EXIT("bind error");
    // Check if there is this file
    if(access(filename,0)<0)
        ERR_EXIT("no file");
        
    char buffer[256] = {0};
    while (1){ // server always runs to continue providing services to client
        int len = 0;
        bzero(buffer, 256);
        
        FILE *fp = fopen(filename,"r"); // open file
        // to start udp send
        len = recvfrom(sockfd, buffer, 255, 0, (struct sockaddr *)&servaddr, &servlen);
	    if(len <= -1)
        {
            if(errno == EINTR)
                continue;
            ERR_EXIT("recvfrom error");
        }
        
        // send filename
        memcpy(buffer, filename, sizeof(filename));
        sendto(sockfd, buffer, 255, 0, (struct sockaddr *)&servaddr, servlen);
        bzero(buffer, 256);

        // send file size
        fseek(fp, 0 , SEEK_END);
        memset(buffer,'a',256);
        change(buffer, ftell(fp)); // long int to char*
        fseek(fp, 0, SEEK_SET);
        sendto(sockfd, buffer, 255, 0, (struct sockaddr *)&servaddr, servlen); // send file size to client

        bzero(buffer,256);
        while((len = fread(buffer, sizeof(char), 255, fp)) > 0){ // if there is data in file , send them to client. 
            sendto(sockfd, buffer, 255, 0, (struct sockaddr *)&servaddr, servlen);
            bzero(buffer, 256);
        }
        printf("finish\n");
        fclose(fp);
    }
    close(sockfd);
}
void udprecv(int portno)
{
	// socket create
    if ((sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        ERR_EXIT("socket");
    
    // give recvfrom time limit 50ms
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 50000; // 50ms
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    long int step = 0;
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    int servlen = sizeof(servaddr);
    // assign IP and port
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(portno);
    servaddr.sin_addr.s_addr = INADDR_ANY;
    
    // start udp recv
    char buffer[256] = {0};
    sendto(sockfd, buffer, 255, 0, (struct sockaddr *)&servaddr, servlen);
    
    // receive filename
    int len = recvfrom(sockfd, buffer, 255, 0, NULL, NULL);
    char filename[len+1];
    memcpy(filename, buffer, len);
    FILE *fp = fopen(filename,"w+"); // open file
    
    bzero(buffer, 255);
    // receive file size
    recvfrom(sockfd, buffer, 255, 0, NULL, NULL);
    long int file_size = stringtoint(buffer); // string to long int 
    bzero(buffer, 255);

    // the variable to restore time
    time_t now;
    struct timespec start, end, temp;
    int progress = 1;
    struct tm *timenow;
    
    time(&now); // time function reads the current time
    clock_gettime(CLOCK_MONOTONIC, &start); // calculate start time
    timenow = localtime(&now); // localtime function to get computer time from time
    printf("0%% %s", asctime(timenow));// asctime fuction to change time into string
    
    // receive data from server into the buffer
    while ((len = recvfrom(sockfd, buffer, 255, 0, NULL, NULL))>0){
        fwrite(buffer, sizeof(char), len, fp); // write content to file
        bzero(buffer, 256);
        step += len; // record the file size currently received 
        // check if the currently received file size has reached the record point
        while(progress<4){
            if(file_size*progress/4<=step){
                time(&now);  // time function reads the current time
                timenow = localtime(&now); // localtime function to get computer time from time
                printf("%d%% %s", progress*25, asctime(timenow));// asctime fuction to change time into string
                ++progress;
            }
            else break;
        }
    }
    
    time(&now); // time function reads the current time
    clock_gettime(CLOCK_MONOTONIC, &end); // ­calculate end time
    timenow = localtime(&now);  // localtime function to get computer time from time

    // calculate the actual time spent
    temp = diff(start, end);
    double dif = temp.tv_sec +(double)temp.tv_nsec/1000000000.0;
    // print out the final result 
    printf("100%% %s", asctime(timenow));// asctime fuction to change time into string
    printf("Total trans time: %fs\nfile size : %ldMB\n",dif,file_size/1024/1024);
    printf("packet loss rate: %.1f%%\n",(float)(file_size-step)/file_size*100);
    fclose(fp);
    close(sockfd);
}
void error(const char *msg)
{
    perror(msg);
    exit(0);
}
