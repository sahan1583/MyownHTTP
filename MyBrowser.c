#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/poll.h>
#define BUFFER 2000


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

void recvResponse(int sockfd, int putFlag,char *contentType, char *filename, char *statusCode){

    char buf[BUFFER];

    int versionFlag=0;                        // flag to tell whether version is taken or not
    int statusIndex=0;                        // index for storing status
    int statusFlag=0;                               
    int bodyStart=0;
    char lineBuf[BUFFER*2+1];                 // stores each line till body is encountered
    int lineIndex=0;

    int contentLength=0;

    int numOfNewlines=0, numOfbackSlash=0;    

    char response[5*BUFFER+1];
    int responseIndex=0;
    FILE *fp;                                 // open file to store the body of get request's response

    struct pollfd fds[1];
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    int ret = poll(fds, 1, 3000);             // if server didnt respond with reponse, Timeout

    if(ret==0){
        printf("Server Timeout\n");
        strcpy(statusCode, "408");
        return;
    }
    else if(ret<0){
        printf("Cannot poll\n");
        strcpy(statusCode, "408");
        return;
    }
    while(1){                                
        int recvSize = recv(sockfd, buf, BUFFER, 0);
        if(recvSize < 0){
            printf("Cannot receive\n");
            return;
        }
        if(recvSize == 0){
            if(bodyStart==1){
                fclose(fp);
            }
            break;
        }
        if(bodyStart==0){
            // if body is not yet started
            // then we are still receiving headers
            for(int i=0;i<recvSize;++i){
                if(buf[i]=='\n' && bodyStart==0){
                    // if we encounter 2 newlines, then we need to get 
                    // 2 backslashes to tell if the body will start
                    numOfNewlines++;
                    if(numOfNewlines==2 && numOfbackSlash==2){
                        bodyStart=1;
                        if(contentLength==0){
                            response[responseIndex] = '\0';
                            printf("Response:\n%s\n", response);
                            return;
                        }
                        continue;
                    }
                }
                // if any other than \r is found when current is not \n, 
                // then reset the variables
                else if(bodyStart==0 && buf[i]!='\r'){
                    numOfNewlines=0;
                    numOfbackSlash=0;
                }
                if(buf[i]=='\r'){
                    numOfbackSlash++;
                }
                // this is to get the version
                if(versionFlag==0 && buf[i]==' '){
                    versionFlag=1;
                    lineBuf[lineIndex++] = buf[i];
                    response[responseIndex++] = buf[i];
                    continue;
                }
                if(versionFlag && statusFlag==0 && buf[i]==' '){
                    statusFlag=1;
                    statusCode[statusIndex] = '\0';
                    char newfilename[100];
                    if(putFlag==0){
                        if(strcmp(statusCode, "200")){
                            strcpy(filename, "./error.html");
                        }
                        else{
                            strcpy(newfilename, "./");
                            strcpy(newfilename, filename);
                            strcpy(filename, newfilename);
                        }
                        fp = fopen(filename, "wb");
                        if(fp==NULL){
                            printf("Cannot open file\n");
                            return;
                        }
                    }
                    else{
                        fp = fopen("./error.html", "wb");
                        if(fp==NULL){
                            printf("Cannot open file\n");
                            return;
                        }
                    }
                    response[responseIndex++] = buf[i];
                    lineBuf[lineIndex++] = buf[i];
                    continue;
                }
                else if(versionFlag && statusFlag==0){
                    statusCode[statusIndex++] = buf[i];
                }
                // if current body didnt yet start then check for 
                // content length and content type
                if(bodyStart==0){
                    lineBuf[lineIndex++] = buf[i];
                    response[responseIndex++] = buf[i];
                    if(buf[i]=='\n'){
                        lineBuf[lineIndex-2] = '\0';
                        if(checkIfEqual(lineBuf,"Content-Length:",15)){
                            contentLength = atoi(lineBuf+16);
                        }
                        else if(checkIfEqual(lineBuf,"Content-Type:",13)){
                            strcpy(contentType, lineBuf+14);
                            contentType[strlen(contentType)]='\0';
                        }
                        lineIndex=0;
                    }
                }
                // else write to the file, if done writing then return
                else{
                    fwrite(buf+i, 1, 1, fp);
                    contentLength--;
                    if(contentLength==0){
                        fclose(fp);
                        response[responseIndex] = '\0';
                        printf("Response:\n%s\n", response);
                        return;
                    }
                }
            }
        }
        // write the recieved part to the file as body has already started
        else{
            fwrite(buf, 1, recvSize, fp);
            contentLength-=recvSize;
            if(contentLength==0){
                fclose(fp);
                response[responseIndex] = '\0';
                printf("Response:\n%s\n", response);
                return;
            }
        }
    }

    response[responseIndex] = '\0';
    printf("Response:\n%s\n", response);

}


void getUrlHostAcceptFilename(char *input, char *url, char *host, char* accept, char* filename, char* port, int putFlag, char *fullfilename){

    int n=strlen(input);

    int urlIndex=0;
    int hostIndex=0;
    int acceptIndex=0;
    int filenameIndex=0;
    int portIndex=0;

    int flag=0;
    int portFlag=0;
    int portStart=n;
    int slashEnd=-1;
    int spaceStart=-1;
    for(int i=0;i<n;++i){
        if(input[i]==' '){
            spaceStart=i;
            break;
        }
        if(input[i]=='/'){
            url[urlIndex++]='/';
            slashEnd=i;
            flag=1;
        }
        else if(input[i]==':'){
            portStart=i;
            portFlag=1;
        }
        else if(flag==0 && portFlag==0){
            host[hostIndex++]=input[i];
        }
        else if(flag==1 && portFlag==0){
            url[urlIndex++]=input[i];
        }
        else if(portFlag==1){
            port[portIndex++]=input[i];
        }
    }

    url[urlIndex]='\0';
    host[hostIndex]='\0';
    port[portIndex]='\0';

    // if nothing is given take 80 as default
    if(strcmp(port, "")==0){
        strcpy(port, "80");
    }

    int start, end;

    if(putFlag){
        start=spaceStart+1;
        end=n;
        int fullfilenameindex=0;
        for(int i=start;i<end;++i){
            // get fullfilepath to open the file
            fullfilename[fullfilenameindex++]=input[i];
        }
        fullfilename[fullfilenameindex]='\0';
        for(int i=n;i>=0;--i){
            if(input[i]=='/' || input[i]==' '){
                start=i+1;
                break;
            }
        }

    }
    else{
        start=slashEnd+1;
        end=portStart;
    }

    int acceptFlag=0;
    for(int i=start;i<end;++i){
        filename[filenameIndex++]=input[i];
        if(input[i]=='.'){
            acceptFlag=1;
        }
        else if(acceptFlag==1){
            accept[acceptIndex++]=input[i];
        }
    }

    accept[acceptIndex]='\0';
    filename[filenameIndex]='\0';

    // copy accept types according to file type
    if(strcmp(accept, "html")==0){
        strcpy(accept, "text/html");
    }
    else if(strcmp(accept, "pdf")==0){
        strcpy(accept, "application/pdf");
    }
    else if(strcmp(accept, "jpeg")==0){
        strcpy(accept, "image/jpeg");
    }
    else{
        strcpy(accept, "text/*");
    }

    if(putFlag){
        strcat(url, "/");              // for put request append file name to the url and send
        strcat(url, filename);
    }

}

int main(){


    while(1){

        char buf[BUFFER];
        printf("MyBrowser> ");
        scanf("%[^\n]s", buf);
        getchar();

        if(strcmp(buf, "QUIT")==0)break;

        char url[BUFFER];
        char host[BUFFER];
        char accept[BUFFER];
        char filename[BUFFER];
        char port[BUFFER];
        char fullfilename[BUFFER];

        // if encounter put, get then parse the string, else ask again
        int putFlag=0;

        if(checkIfEqual(buf, "PUT ", 4)){
            getUrlHostAcceptFilename(buf+11, url, host, accept, filename, port, 1, fullfilename);
            putFlag=1;
        }
        else if(checkIfEqual(buf, "GET ", 4)){
            getUrlHostAcceptFilename(buf+11, url, host, accept, filename, port, 0, fullfilename);
        }
        else continue;


        int portNumber = atoi(port);
        int sockfd;
        struct sockaddr_in serv_addr;

        if((sockfd=socket(AF_INET, SOCK_STREAM, 0))<0){
            printf("Socket creation failed\n");
            perror("Socket");
            return -1;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(portNumber);
        serv_addr.sin_addr.s_addr = inet_addr(host);

        if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0){
            printf("Connection failed\n");
            perror("Connect");
            return -1;
        }

        char request[BUFFER*5+1];

        char date[100];
        time_t t = time(NULL);
        struct tm tm = *gmtime(&t);
        // get current time in GMT
        strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S %Z", &tm);

        char if_modified_since[100];
        t -= 2*24*60*60;
        tm = *gmtime(&t);
        // get time 2 days ago in GMT for If-Modified-Since
        strftime(if_modified_since, sizeof(if_modified_since), "%a, %d %b %Y %H:%M:%S %Z", &tm);


        if(putFlag){
            int contentLength=0;
            FILE *fp=fopen(fullfilename, "rb");
            if(fp==NULL){
                printf("File not found\n");
                continue;
            }
            while(fgetc(fp)!=EOF){
                contentLength++;
            }
            fclose(fp);
            sprintf(request, "PUT %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\nDate: %s\r\nContent-Language: en-US\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n", url, host,portNumber ,date, contentLength, accept);
        }
        else{
            sprintf(request, "GET %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\nDate: %s\r\nAccept: %s\r\nAccept-Language: en-US,en;q=0.5\r\nIf-Modified-Since: %s\r\n\r\n", url, host, portNumber,date, accept, if_modified_since);
        }

        printf("\nRequest:\n%s\n", request);

        int sendSize = send(sockfd, request, strlen(request), 0);

        if(sendSize<0){
            printf("Send failed\n");
            perror("Send");
            return -1;
        }

        if(putFlag){

            FILE *fp2 = fopen(fullfilename, "rb");

            if(fp2==NULL){
                printf("File not found\n");
                continue;
            }

            char fileBuf[BUFFER];
            int fileBufSize;
            while((fileBufSize=fread(fileBuf, 1, BUFFER, fp2))>0){
                sendSize = send(sockfd, fileBuf, fileBufSize, 0);
                if(sendSize<0){
                    printf("Send failed\n");
                    perror("Send");
                    return -1;
                }
            } 
            fclose(fp2);

            char contentType[100];
            char statusCode[100];
            
            recvResponse(sockfd, putFlag, contentType, fullfilename, statusCode);
            if(strcmp(statusCode, "200") && strcmp(statusCode, "400") && strcmp(statusCode, "404") && strcmp(statusCode, "404")){
                printf("Unknown Error %s\n", statusCode);
            }
        }
        else{
            char contentType[100];
            char statusCode[100];

            recvResponse(sockfd, putFlag,contentType, filename, statusCode);
            if(strcmp(statusCode, "200") && strcmp(statusCode, "400") && strcmp(statusCode, "403") && strcmp(statusCode, "404")){
                printf("Unknown Error %s\n", statusCode);
            }
            else{
                printf("Status Code: %s\n", statusCode);
            }
            if(strcmp(statusCode, "200")){
                continue;
            }

            // if file recieved successfully, open it in the appropriate application
            int x=fork();

            if(x==0){
                if(strcmp("text/html", contentType)==0){
                    char *args[]={"firefox", filename, NULL};
                    execvp("firefox", args);
                }
                else if(strcmp("application/pdf", contentType)==0){
                    char *args[]={"firefox", filename, NULL};
                    execvp("firefox", args);
                }
                else if(strcmp("image/jpeg", contentType)==0){
                    char *args[]={"eog", filename, NULL};
                    execvp("eog", args);
                }
                else{
                    char *args[]={"gedit", filename, NULL};
                    execvp("gedit", args);
                }
                exit(0);
            }
            // paremt process waits for child to finish
            else{
                wait(NULL);
            }

        }
        close(sockfd);
    }

}


