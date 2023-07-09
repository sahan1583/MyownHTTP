#define __USE_XOPEN
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#define BUFFER 2000


int statusFlag=200;
char statusMsg[100]="OK";
char expires[100];

void fileOpenFailed(){

    if(errno==EACCES){
        statusFlag=403;
    }
    else if(errno==ENOENT){
        statusFlag=404;
    }
    else{
        statusFlag=500;
    }
}

void GetStatusMsg(){
    if(statusFlag==200){
        strcpy(statusMsg, "OK");
    }
    else if(statusFlag==400){
        strcpy(statusMsg, "Bad Request");
    }
    else if(statusFlag==403){
        strcpy(statusMsg, "Forbidden");
    }
    else if(statusFlag==404){
        strcpy(statusMsg, "Not Found");
    }
    else if(statusFlag==304){
        strcpy(statusMsg, "Not Modified");
    }
}


int checkIfEqual(char *s1, char *s2, int toChars){

    if(strlen(s1)<toChars || strlen(s2)<toChars){
        return 0;
    }
    for(int i=0;i<toChars;++i){
        if(s1[i]!=s2[i]){
            return 0;
        }
    }
    return 1;
}


int recvRequest(int newsockfd,int *putFlag, char *url, char *req, char *acceptType, char *contentType, char *response, char *ifmodifiedsince){

    // this is to recieve the request from the client in chunks of BUFFER bytes
    char buf[BUFFER];

    int reqType=0;     // flag for request type, if 0 then request type is not yet recieved
    int reqIndex=0;    // index for request type

    int urlFlag=0;     // flag for url, if 0 then url is not yet recieved
    int urlIndex=0;    // index for url

    int bodyStart=0;   // flag for body, if 0 then body is not yet started

    char lineBuf[BUFFER*2+1];   // buffer for each line of the request header
    int lineIndex=0;            // index for line buffer

    int contentLength=0;    // content length of the request body

    int responseIndex=0;        // index for response buffer
    int firstLine=0;


    // to check if the request header is over, 
    // if the number of newlines is 2 and the number of backslashes is 2
    // then the request header is over, and the body starts
    // if we encounter \r then we increment numOfbackSlash
    int numOfNewlines=0, numOfbackSlash=0; 

    FILE *fp;
    while(1){
        // recieve BUFFER size chunks of data from the client
        int recvSize = recv(newsockfd, buf, BUFFER, 0);
        if(recvSize <= 0){
            printf("Cannot receive\n");
            return 0;
        }

        if(statusFlag!=200){
            GetStatusMsg();
            contentLength-=recvSize;
            if(contentLength<=0){
                return 0;
            }
        }

        if(bodyStart==0){ 
            for(int i=0;i<recvSize;++i){

                if(buf[i]=='\n' && bodyStart==0){
                    numOfNewlines++;
                    // if we get \r\n\r\n then the request header is over
                    if(numOfNewlines==2 && numOfbackSlash==2){
                        bodyStart=1;
                        if(contentLength==0){
                            if(*putFlag==1){
                                fclose(fp);
                            }
                            response[responseIndex] = '\0';
                            if(statusFlag!=200)return 0;
                            return 1;
                        }
                        continue;
                    }
                }
                else if(bodyStart==0 && buf[i]!='\r'){
                    numOfNewlines=0;
                    numOfbackSlash=0;
                }
                if(bodyStart==0 && buf[i]=='\r'){
                    if(numOfbackSlash!=numOfNewlines){
                        statusFlag=400;
                        GetStatusMsg();
                    }
                    numOfbackSlash++;
                }
                if(reqType==0 && buf[i]==' '){
                    reqType=1;
                    req[reqIndex++] = '\0';
                    // if it is a put request then we set the putFlag to 1
                    if(checkIfEqual(req,"PUT",3)){
                        *putFlag=1;
                    }
                    lineBuf[lineIndex++] = ' ';
                    response[responseIndex++] = ' ';
                    continue;
                }
                else if(reqType==0){
                    req[reqIndex++] = buf[i];
                }
                if(reqType && urlFlag==0 && buf[i]==' '){
                    urlFlag=1;                 
                    url[urlIndex] = '\0';

                    if(*putFlag==1){
                        fp = fopen(url, "wb");
                        if(fp==NULL){
                            fileOpenFailed();
                            GetStatusMsg();
                            return 0;
                        }
                    } 
                    lineBuf[lineIndex++] = ' ';
                    response[responseIndex++] = ' ';
                    continue;
                }
                else if(reqType && urlFlag==0){
                    url[urlIndex++] = buf[i];
                }
                if(bodyStart==0){

                    lineBuf[lineIndex++] = buf[i];
                    response[responseIndex++] = buf[i];

                    if(buf[i]=='\n'){

                        lineBuf[lineIndex-2] = '\0';
                        if(firstLine==0){
                            firstLine=1;
                            if(checkIfEqual(lineBuf,"GET",3)==0 && checkIfEqual(lineBuf, "PUT", 3)==0){
                                statusFlag=400;
                                GetStatusMsg();
                            }
                        }
                        else{
                            char *ptr = strstr(lineBuf, ":");
                            if(ptr!=NULL){
                                ptr++;
                                ptr++;
                                if(ptr[0]=='\0'){
                                    statusFlag=400;
                                    GetStatusMsg();
                                }
                            }
                        }
                        if(checkIfEqual(lineBuf,"Content-Length:",15)){
                            contentLength = atoi(lineBuf+16);
                        }
                        else if(checkIfEqual(lineBuf,"Content-Type:",13)){
                            strcpy(contentType, lineBuf+14);
                            contentType[strlen(contentType)] = '\0';
                        }
                        else if(checkIfEqual(lineBuf,"Accept:", 7)){
                            strcpy(acceptType, lineBuf+8);
                            acceptType[strlen(acceptType)] = '\0';
                        }
                        else if(checkIfEqual(lineBuf,"If-Modified-Since:", 18)){
                            strcpy(ifmodifiedsince, lineBuf+19);
                            ifmodifiedsince[strlen(ifmodifiedsince)] = '\0';
                        }
                        lineIndex=0;
                    }
                }
                else{
                    if(statusFlag!=200){
                        GetStatusMsg();
                        contentLength--;
                        if(contentLength<=0){
                            return 0;
                        }
                    }
                    fwrite(buf+i, 1, 1, fp);
                    contentLength--;
                    if(contentLength==0){
                        fclose(fp);
                        response[responseIndex] = '\0';
                        return 1;
                    }
                }
            }
        }
        // body has started, so we write the data to the file
        else{
            fwrite(buf, 1, recvSize, fp);
            contentLength-=recvSize;
            if(contentLength<=0){
                fclose(fp);
                response[responseIndex] = '\0';
                return 1;
            }
        }
    }
    response[responseIndex] = '\0';
    return 1;

}


int main(){


    int sockfd;
    socklen_t clilen;
    struct sockaddr_in	cli_addr, serv_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0))<0){
        printf("Cannot create socket\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(8081);

    if(bind(sockfd, (struct sockaddr *)& serv_addr, sizeof(serv_addr))<0){
        printf("Cannot bind\n");
        return -1;
    }

    listen(sockfd, 5);

    while(1){

        clilen = sizeof(cli_addr);
        int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

        if(newsockfd < 0){
            printf("Cannot accept\n");
            return -1;
        }
        if(fork()==0){

            close(sockfd);
            // global variable to decide status code
            statusFlag=200;
            statusMsg[0]='O';
            statusMsg[1]='K';
            statusMsg[2]='\0';

            time_t t = time(NULL);
            struct tm tm = *localtime(&t);

            char url[BUFFER];
            char req[4];
            int putFlag = 0;
            char acceptType[100];
            char contentType[100];
            char response[5*BUFFER+1];
            char ifmodifiedsince[100];
            
            // recieve request from client
            int success = recvRequest(newsockfd,&putFlag, url, req, acceptType, contentType, response, ifmodifiedsince);
            // On successfull request, add to AccessLog.txt
            printf("\nRequest:\n%s", response);
            FILE *fp;
            fp=fopen("AccessLog.txt", "a");
            fprintf(fp, "%02d%02d%02d:%02d%02d%02d:%s:%d:%s:%s\n", tm.tm_mday,tm.tm_mon+1,(tm.tm_year+1900)%100, tm.tm_hour, tm.tm_min, tm.tm_sec, inet_ntoa(cli_addr.sin_addr), cli_addr.sin_port, req, url);
            fclose(fp);

            t += 3*24*60*60;
            tm = *gmtime(&t);
            strftime(expires, sizeof(expires), "%a, %d %b %Y %H:%M:%S %Z", &tm);

            struct stat st;
            stat(url, &st);

            struct tm tk;
            strptime(ifmodifiedsince, "%a, %d %b %Y %T %Z", &tk);
            time_t seconds = timegm(&tk);
            if(putFlag==0){

                if(seconds>=st.st_mtime){
                    success=0;
                    statusFlag=304;
                    GetStatusMsg();
                }
            }

            // if not success then send error response
            if(success==0){
                strcpy(acceptType, "text/html");
                strcpy(contentType, "text/html");
                GetStatusMsg();
                int contentLength = strlen(statusMsg);
                contentLength+=4;
                sprintf(response, "HTTP/1.1 %d %s\r\nExpires: %s\r\nCache-Control: no-store\r\nContent-Language: en-US\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n",statusFlag, statusMsg, expires, contentLength, acceptType);
                printf("\nResponse:\n%s\n", response);
                char temp[100];
                sprintf(temp,"%d ", statusFlag);
                strcat(response, temp);
                strcat(response, statusMsg);
                send(newsockfd, response, strlen(response), 0);
                close(newsockfd);
                exit(0);
            }

            if(putFlag==0){
                
                FILE *fp2;

                char response[BUFFER*5+1];
                int contentLength=0;
                            
                // open file and if failed get the status code and message
                fp2 = fopen(url, "rb");

                if(fp2==NULL){
                    fileOpenFailed();
                    GetStatusMsg();
                    contentLength=strlen(statusMsg)+4;
                    strcpy(acceptType, "text/html");
                }
                else{
                    while(fgetc(fp2)!=EOF){
                        contentLength++;
                    }
                    fclose(fp2);
                }

                // send it in reponse

                char lastModifiedTime[100];
                strftime(lastModifiedTime, sizeof(lastModifiedTime), "%a, %d %b %Y %H:%M:%S %Z", gmtime(&st.st_mtime));
                sprintf(response, "HTTP/1.1 %d %s\r\nExpires: %s\r\nCache-Control: no-store\r\nContent-Language: en-US\r\nContent-Length: %d\r\nContent-Type: %s\r\nLast-Modified: %s\r\n\r\n",statusFlag, statusMsg, expires, contentLength, acceptType, lastModifiedTime);
                
                printf("\nResponse:\n%s\n", response);
                send(newsockfd, response, strlen(response), 0);

                fp2 = fopen(url, "rb");

                if(fp2==NULL){
                    fileOpenFailed();
                    GetStatusMsg();
                    contentLength=strlen(statusMsg)+4;
                    char temp[100];
                    sprintf(temp,"%d ", statusFlag);
                    strcat(temp, statusMsg);
                    send(newsockfd, temp, contentLength, 0);
                    strcpy(acceptType, "text/html");
                }
                else{
                    char fileBuf[BUFFER];
                    int fileBufSize;
                    while((fileBufSize=fread(fileBuf, 1, BUFFER, fp2))>0){
                        int sendSize = send(newsockfd, fileBuf, fileBufSize, 0);
                        if(sendSize<0){
                            printf("Send failed\n");
                            perror("Send");
                            return -1;
                        }
                    }
                    fclose(fp2);
                }
            }
            else{
                char response[BUFFER*5+1];

                sprintf(response, "HTTP/1.1 %d %s\r\nExpires: %s\r\nCache-Control: no-store\r\nContent-Language: en-US\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n", statusFlag,statusMsg,expires, 0, contentType);
                printf("\nResponse:\n%s\n", response);
                send(newsockfd, response, strlen(response), 0);
            }
            close(newsockfd);
            exit(0);
        }
        close(newsockfd);
    }

}