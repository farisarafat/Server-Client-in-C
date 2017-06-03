#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include "libnetfiles.h"

/***PORT NO. IS 10111***/


//this is to make sure if netserverinit() was called if it wasnt none of the other codes should work!
int ifNetInitServerCalled = 0;
struct addrinfo clientSideInfo;
struct addrinfo *serverSideInfo;
int connectMode; //this is determined by the file mode in netserverinit();
char ipAddrString[INET_ADDRSTRLEN];

//connection method that is used for all the net file methods
int connectMethod(int socketDescript){
    socketDescript = socket(serverSideInfo->ai_family, serverSideInfo->ai_socktype, serverSideInfo->ai_protocol);
    int connectStatus = connect(socketDescript, serverSideInfo->ai_addr, serverSideInfo->ai_addrlen);

    if(connectStatus != 0){
        perror("Connection Error");
        return -1;
    }

    inet_ntop(serverSideInfo->ai_family, (void *)((struct sockaddr *)serverSideInfo->ai_addr), ipAddrString, sizeof(ipAddrString));

    return socketDescript;


}

//sends path name and read write flags to the server to be evaluated
int netopen(const char *pathname, int flags){
    if(ifNetInitServerCalled == 0){
        fprintf(stderr, "netopen: netopen() called before netserverinit()\n");
        exit(-1);
    }
    int sockDescript = -1;
    sockDescript = connectMethod(sockDescript);
    printf("netOpen: Connected to %s\n", ipAddrString);

    //This helps synchronizes
   sleep(1);
    /***Make a seperate method for sending pathname and flags from the code below!***/

    printf("NetOpen: Sending File Mode.\n");
    int openMdMsg = htonl(connectMode);
    if(send(sockDescript, &openMdMsg, sizeof(openMdMsg), 0) == -1){
        perror("Netopen, Unable to send File Mode.");
    }

    //This helps synchronizes
    sleep(1);

    printf("netopen: Sending over NETOPEN.\n");
    int msgOpen = htonl(NETOPEN);

    if(send(sockDescript, &msgOpen, sizeof(msgOpen), 0) == -1){
        perror("netopen: Message Type could not be read");
    }

   sleep(1);

    printf("NetOpen: Sending Path.\n");
    if(send(sockDescript, pathname, strlen(pathname), 0) == -1){
        perror("netopen: could not send path");
    }

    sleep(1);

    printf("NetOpen: Sending Flags\n");
    int flagspayload = htonl(flags);
    if(send(sockDescript, &flagspayload, sizeof(flagspayload), 0) == -1){
  	perror("netopen could not send flags");
    }

    printf("NetOpen: waiting to receive result.\n");

    int resultFileDescr;
    int resultMessage;
    if((resultMessage = recv(sockDescript, &resultFileDescr, sizeof(resultFileDescr), 0)) == -1){
        perror("netopen could not recieve Result");
        exit(-1);
    }

    printf("NetOpen: recieved result FD!\n");

    int resultFromServer = resultFileDescr;
    int errorType;

    if(resultFromServer == -1){
        if((resultMessage = recv(sockDescript, &errorType, sizeof(errorType),0)) == -1){
            perror("netOpen could not recieve Error");
        }
        if(errorType == DENIED_ACCESS){
	    errno = 13;
            perror("Could not open file due to restrictions.");
        }
        else{
            errno = errorType;
            perror("Error With Result from Server!");
        }
    }
    close(sockDescript);
    if(resultFromServer == -1){
       return resultFromServer;
     }	
    return -1*resultFromServer;
}

//in this method we have a buffer and the required bytes we want to read from the opened file
ssize_t netread(int filedes, void *buf, size_t nbyte){
    if(ifNetInitServerCalled == 0){
        fprintf(stderr, "netread: netread() called before netserverinit()\n");
        exit(-1);
    }

    int sockDescript = -1;
    sockDescript = connectMethod(sockDescript);
    printf("Netread: Connected to %s\n", ipAddrString);

    sleep(1);

    printf("NetRead: Sending File Mode.\n");

     int msgOpen = htonl(NETOPEN);

    if(send(sockDescript, &msgOpen, sizeof(msgOpen), 0) == -1){
        perror("netOpen: Message Type");
    }

    sleep(1);

    printf("NetRead: Sending NETREAD.\n");
    int netreadMsg = htonl(NETREAD);
    if(send(sockDescript, &netreadMsg, sizeof(int), 0) == -1){
        perror("netread send could not send NETREAD");
    }

    sleep(1);

    printf("NetRead: Sending File Descriptor. \n");
    int filedescrpay = htonl(filedes);
    if(send(sockDescript, &filedescrpay, sizeof(int),0) == -1){
        perror("netread send payload problem");
    }

    sleep(1);

    //sending the size the client wanted
    printf("NetRead: Sending nbyte to Server: %d\n", nbyte);
    int sizeload = htonl(nbyte);
    if(send(sockDescript, &sizeload, sizeof(int), 0) == -1){
        perror("netread send sizepayload failed");
    }

    //Here we set up to receive the result from the server

    printf("NetRead: waiting to receive result\n");
    int resultSize;
    int resultMessage;
    if((resultMessage = recv(sockDescript, &resultSize, sizeof(resultSize),0)) == -1){
        perror("netread could not receive result");
    }


    int result = resultSize;
    printf("NetRead: Received result size: %d\n", result);


    printf("NetRead: Checking for any errors.\n");
    if(result == -1){
        int err;

        if((resultMessage = recv(sockDescript, &err, sizeof(err), 0)) == -1){
            perror("netread received an error");
        }
	errno = err;
        printf("Errno Is: %s\n", strerror(errno));
    }
    else{
        printf("netread: No errors found, receive resulting string\n");
        char resultString[MAXBUFFERSIZE];
        if(recv(sockDescript, buf, nbyte, 0) == -1){
            perror("netread result String error");
        }
    }

    close(sockDescript);
    return result;
}

//in this method we have a buffer and the required bytes we want to write from the opened file
ssize_t netwrite(int filedes, const void *buf, size_t nbyte){
    if(ifNetInitServerCalled == 0){
        fprintf(stderr, "netwrite: netwrite() called before netserverinit()\n");
        exit(-1);
    }
    int sockDescript = -1;
    sockDescript = connectMethod(sockDescript);
    printf("netwrite: Connected to %s\n", ipAddrString);

    sleep(1);

    printf("netwrite: Sending File Mode.\n");
    int modeMsg = htonl(connectMode);
    if(send(sockDescript, &modeMsg, sizeof(modeMsg), 0) == -1){
        perror("netwrite could not send file mode");
    }

    sleep(1);

    printf("netwrite: Sending NETWRITE message.\n");
    int netType = htonl(NETWRITE);
    if(send(sockDescript, &netType, sizeof(int), 0) == -1){
        perror("netwrite could not send NETWRITE to server!\n");
    }

    sleep(1);

    printf("netWrite: sending File Descriptor.\n");
    int filedescrpay = htonl(filedes);
    if(send(sockDescript, &filedescrpay, sizeof(int),0) == -1){
        perror("netwrite send file descriptor problem");
    }

    sleep(1);

    //sending byte size
    printf("netwrite: Sending nbyte to Server: %d\n", nbyte);
    int sizeload = htonl(nbyte);
    if(send(sockDescript, &sizeload, sizeof(int), 0) == -1){
        perror("netwrite send size failed to send");
    }

    sleep(1);

    //Send String
    printf("netwrite: Sending String.\n");
    if(send(sockDescript, buf, strlen(buf), 0) == -1){
        perror("netwrite could not send over string");
    }

    printf("netwrite: Waiting to receive the results!\n");
    int resultSize;
    int resultMsg;
    if((resultMsg = recv(sockDescript, &resultSize, sizeof(resultSize),0)) == -1){
        perror("netwrite problem with receiving results from server");
    }

    int result = ntohl(resultSize);
    printf("netwrite: Received result size of: %d\n", result);

    //check result and see if any errors
    printf("netwrite: checking for errors.\n");
    if(result == -1){
        int errorResult;

        if((resultMsg = recv(sockDescript, &errorResult, sizeof(errorResult), 0)) == -1){
            perror("netwrite received an error!");
        }


	errno = ntohl(errorResult);
	perror("netwrite: errno received");

    }
    close(sockDescript);
    return result;
}

//in this method we want to close the file we opned earlier 
 int netclose(int fd){
    if(ifNetInitServerCalled == 0){
        fprintf(stderr, "netwrite: netwrite() called before netserverinit()\n");
        exit(-1);
    }

    int sockDescript = -1;
    sockDescript = connectMethod(sockDescript);
    printf("netclose: Connected to %s\n", ipAddrString);

    sleep(1);

    //sending file mode
    printf("netclose: Sending File Mode.\n");

    int modeMsg = htonl(connectMode);
    if(send(sockDescript, &modeMsg, sizeof(modeMsg),0) == -1){
        perror("netclose could not send file mode");
    }

    sleep(1);

    printf("netclose: Sending NETCLOSE.\n");
    int msgCLOSE = htonl(NETCLOSE);
    if(send(sockDescript, &msgCLOSE, sizeof(int),0) == -1){
        perror("netclose cant send NETCLOSE");
    }

    sleep(1);

    //file descriptor
    printf("netclose: Sending File Descriptor.\n");
    int filedescrpay = htonl(fd);
    if(send(sockDescript, &filedescrpay, sizeof(int),0) == -1){
        perror("netwrite send file descriptor problem");
    }

    //receive results
    printf("Waiting to receive results...\n");
    int resultClose;
    int resultMessage;

    if((resultMessage = recv(sockDescript, &resultClose, sizeof(resultClose),0))== -1){
        perror("netclose could not receive results");
    }
    int result = resultClose;
    printf("netclose: Recieved close result of: %d\n", result);
    if(result == -1){
         int err;

        if((resultMessage = recv(sockDescript, &err, sizeof(err), 0)) == -1){
            perror("netread received an error");
        }
	errno = ntohl(err);
        perror("Problem while trying to NetClose");
    }

    close(sockDescript);
    return result;

 }
// the library function opens a connection to the server and sends a message telling us if it was successful or not 
 int netserverinit(char *hostname, int filemode){
    int setSock = -1;
    int status;

    memset(&clientSideInfo,0, sizeof(clientSideInfo));
    clientSideInfo.ai_family = AF_INET; //IPv4
    clientSideInfo.ai_socktype = SOCK_STREAM; //TCP socket stream
    clientSideInfo.ai_flags = AI_PASSIVE;

    //Checks if file mode is valid
    if(filemode < 0 || filemode > 2){
        fprintf(stderr, "filemode: %s\n", "Invalid File Mode");
        h_errno = INVALID_FILE_MODE;
        return -1;
    }

    //Checks that host-name exists
    printf("Reviewing If Address Info Is Valid For: %s with FileMode: %x\n", hostname, filemode);
    if((status = getaddrinfo(hostname, PORT, &clientSideInfo, &serverSideInfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
	h_errno = HOST_NOT_FOUND;
        return -1;
    }

    connectMode = filemode;

    printf("%s\n", "Success! Hostname found!");

    ifNetInitServerCalled = 1;
    return 0;
 }

//int main(){
    //char res[1000];
    //netserverinit("localhost", 1);   
    //int fd = netopen("./cow.txt",  O_RDWR);
    //printf("FD is: %d\n", fd);
    //netread(fd , res, 1200);
   // netwrite(fd, "porn rocks", 5000);
    //netclose(fd2);
   //printf("The resulting string from read is: %d\n", fd);
//}

