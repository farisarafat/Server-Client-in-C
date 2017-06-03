#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "libnetfiles.h"
#include <pthread.h>
#include <stdbool.h>

int fdtracker[512] = {0};
int fdcount = 0;
fdTables * table = NULL;
/*
void printTheFDTable(fdTables *insertNode){
    printf("****We are printing out the table!*****\n");
    fdTables * copy = table;
    while(copy!=NULL){
    	printf("copy->ci->fileName-> %s, copy->ci->fileMode-> %d\n",copy->ci->fileName, copy->ci->fd);
	copy = copy->next;
    }
printf("****End of printing out the table!*****\n");
}
*/
//Insert Node into LL
void inserToLL(fdTables *insertNode){
    fdTables * copy = table;
    if(copy->next == NULL){
        table->next = insertNode;
	return;
    }
    int mover = 0;
    while(copy->next != NULL){
        copy = copy->next;
	mover++;
    }

    if(copy->next == NULL){
        copy->next = insertNode;
        return;
    }
}

//Remove Node from the FD Table
void removeNode(clientInfo * target){
    fdTables * ptr = table;
    fdTables * prev = NULL;


    while(ptr!= NULL){
	if(target->fd == ptr->ci->fd){
            //remove node
	    if(ptr->next == NULL){
		if(prev == NULL){
		    table = NULL;
	            break;
		}
                prev->next = NULL;
	        break;
            }
	    if(prev == NULL){
		table = table->next;
		break;
            }
            prev->next = ptr->next;
            break;
        }
        if(prev == NULL){
            prev = ptr;
            ptr = ptr->next;
            continue;
        }
        prev = prev->next;
        ptr = ptr->next;
    }
}

//Checks if the file is already in use or not if it is it checks the file mode and it determines whetether or not the client is allowed to use it or not. 
int checkifExists(clientInfo * packet){

    if(table == NULL){
        table = malloc(sizeof(struct fdTable));
        table->ci = packet;
	table->next = NULL;
         return 1;
    }

    fdTables * copy = table;

    while(copy != NULL){
	printf("Comparing table: %s with packet %s\n",copy->ci->fileName, packet->fileName);
        if((strcmp(copy->ci->fileName, packet->fileName) == 0)){// && copy->ci->clientfd != packet->clientfd){
	    if(copy->ci->filemode == 0){
                //UNRESTRICTED
               //add to linked list
                break;
            }
            if(copy->ci->filemode == 1){
                //EXCLUSIVE
                //if write or red/write deny access
		if(packet->flags>=1){

                     printf("Client has been denied access since a previous file has Exclusive mode on!\n");
                    return DENIED_ACCESS;
                }
                else{
                    return 1;
                }
            }
            if(copy->ci->filemode == 2){
                //Transaction Mode

		printf("Client has been denied access since a previous file has Transaction mode on!\n");
                return DENIED_ACCESS;
            }

        }
         copy = copy->next;
    }
	fdTables * newNode = malloc(sizeof(struct fdTable));
        newNode->ci = packet;
        inserToLL(newNode);

        return 1;
}

//opens file on the remote machine and returns the FD and any errno. It also checks if the files has been used as well.
clientInfo * n_open(clientInfo * packet, char buffer[MAXBUFFERSIZE], int errnum){

      int check_path;
      if ((check_path = recv(packet->clientfd, buffer, MAXBUFFERSIZE, 0)) == -1) { // getting file name to open
        perror("NetOpen: Could not receive path");
        exit(1);
      }

      buffer[check_path] = '\0';
      printf("NetOpen: Received path: %s\n", buffer);

      //Adding file path into struct
      packet->fileName =  malloc(check_path);
     strcpy(packet->fileName, buffer);

      printf("NetOpen: Waiting for flags message...\n");
      int flagsrecv;
      int check_flags;
      if ((check_flags = recv(packet->clientfd, &flagsrecv, sizeof(int), 0)) == -1) {
        perror("NetOpen: Issue with receiving flags");
      }
      int flags = ntohl(flagsrecv);
      printf("NetOpen: Received flags: %i\n", flags);
      packet->flags = flags;

      // try actually opening the file and then sending the result FD back
      printf("NetOpen: Trying to open the file\n");
      int result = open(buffer, flags);
      if(result != -1){
           packet->fd = -1*result;
      }
       int exists;
      if(result!= -1){
         exists = checkifExists(packet);
      }
      if(exists < 0){//exists is -215
        result = -1;
        printf("Error: NetOpen: Cannot open file since there is another file open with a more restricted mode,\n");
      }else{
        printf("NetOpen: Sending netopen result: %i\n", result);
      }

      int resultpayload = result;
      if (send(packet->clientfd, &resultpayload, sizeof(int), 0) == -1) {
        perror("NetOpen: there was an issue sending the fd/data");
      }
      // if there was an error getting the resulting FD
      if (result == -1){
        // send serrno across the wire
        if(exists < 0){
            errnum = DENIED_ACCESS;
        }else{
            printf("NetOpen: Sending errno :%d\n", errno);
            errnum = errno;
        }
        if (send(packet->clientfd, &errnum, sizeof(errnum), 0) == -1) {
          perror("NetOpen: issue with sending errno");
        }
      }
      // Valid FD for opening file. Add to tracker
      if (fdcount<512) {
        fdtracker[fdcount] = resultpayload;
      }
      else {
        printf("NetOpen: Too many files open. Unable to save FD.\n");
        fdcount++;
      }

      return packet;
}

//From using the file descriptor above in the open method above we read how many bytes the client want into their buffer.
clientInfo * n_read(clientInfo * packet, char buffer[MAXBUFFERSIZE], int errnum){

      int filedesrecv;
      int filedesmsg;

      if((filedesmsg = recv(packet->clientfd, &filedesrecv, sizeof(filedesrecv), 0)) == -1){
        perror("NetRead: was not able to receive message");
      }

      int readfd = ntohl(filedesrecv);
      printf("NetRead: Received File Descriptor: %i\n", readfd);
      if(readfd == -1){
	readfd = -1;
      }
      else if(readfd < 0) {
        printf("The read file descriptor is: %d\n", readfd);
        readfd = readfd * -1;
      }

      printf("NetRead: Waiting for nthbyte size...\n");
      size_t nbyterecv;
      int nbytemsg;

      if((nbytemsg = recv(packet->clientfd, &nbyterecv, sizeof(nbyterecv), 0)) == -1)  {
        perror("NetRead: did not receive any number of bytes after reading");
      }

      size_t numBytesToBeRead = ntohl(nbyterecv);
      printf("NetRead: Received nbytes that are to be read: %d\n", numBytesToBeRead);

     sleep(1);

      printf("NetRead: Trying to read the file\n");
      if(readfd == -1){
        int readresultpayload = -1;
        if (send(packet->clientfd, &readresultpayload, sizeof(readresultpayload), 0) == -1) {
            perror("ERROR: NetRead: Issue in sending result!");
        }
        
      	// send errno across the wire
	errno = 9;
        printf("NetRead: Sending errno :%d\n", errno);
        errnum = errno;//htonl(errno);
        if (send(packet->clientfd, &errnum, sizeof(errnum), 0) == -1) {
          perror("NetRead: was not able to send errno");
        }
        return packet;
	   
      }

      char buffer2[MAXBUFFERSIZE];
      bzero(buffer, MAXBUFFERSIZE);

      int readresult = 0;
      int bitsReadIn = 0;
      int usedbitsReadin = 0;
      if(numBytesToBeRead > MAXBUFFERSIZE){ // check this again
          while(numBytesToBeRead != 0){

                if(numBytesToBeRead < MAXBUFFERSIZE){
                    //bzero(buffer, MAXBUFFERSIZE);
                    bitsReadIn += read(readfd, buffer2, numBytesToBeRead-1);
                    strcat(buffer2,"\0" );
                    readresult += bitsReadIn;
		    readresult++;
                    numBytesToBeRead = numBytesToBeRead - bitsReadIn;
                    usedbitsReadin = 1;
                    break;
                }
                if(numBytesToBeRead > MAXBUFFERSIZE){
                    readresult += read(readfd, buffer, MAXBUFFERSIZE);
                    numBytesToBeRead = numBytesToBeRead - MAXBUFFERSIZE;

                }
          }
      }
      else{
        readresult = read(readfd, buffer, numBytesToBeRead);
      }
      printf("NetRead: Buffer Result: %s\n", buffer);
      int readresultpayload = readresult;
      if (send(packet->clientfd, &readresultpayload, sizeof(readresultpayload), 0) == -1) {
        perror("ERROR: NetRead: Issue in sending result!");
      }
      // if there was an error getting the resulting size
      if(readresult == -1){
        // send errno across the wire
        printf("NetRead: Sending errno :%d\n", errno);
        errnum = errno;//htonl(errno);
        if (send(packet->clientfd, &errnum, sizeof(errnum), 0) == -1) {
          perror("NetRead: was not able to send errno");
        }
        return packet;
      }
    // sending the string that was read by the buffer

    if(usedbitsReadin == 1){
	if(strcmp(buffer2, "\0") == 0){
		printf("NetRead: Sending buffer: %s\n", buffer);
	    if (send(packet->clientfd, buffer, readresult, 0) == -1) {
                perror("ERROR: NetRead: Issue with sending the readin buffer");
            }
        }
	else{
	    printf("NetRead: Sending buffer: %s\n", buffer2);
	    if (send(packet->clientfd, buffer2, bitsReadIn, 0) == -1) {
                perror("ERROR: NetRead: Issue with sending the readin buffer");
            }
        }
    }else{
	printf("NetRead: Sending buffer: %s\n", buffer);
        if (send(packet->clientfd, buffer, readresult, 0) == -1) {
            perror("ERROR: NetRead: Issue with sending the readin buffer");
        }
    }
    return packet;
}

//The write method takes in the amount of bytes the client wants to write and the buffer they passed in 
clientInfo * n_write(clientInfo * packet, char buffer[MAXBUFFERSIZE], int errnum){

      int fdrecv;
      int fdmsg;
      if ((fdmsg = recv(packet->clientfd, &fdrecv, sizeof(fdrecv), 0)) == -1) {
        perror("ERROR: NetWrite: Unable to receive message with fd");
      }
      int writefd = ntohl(fdrecv);
      printf("NetWrite: Received File Descriptor: %i\n", writefd);
      if(writefd == -1){
	writefd = -1;
      }
      else if(writefd < 0) {
        printf("%d\n", writefd);
        writefd = writefd * -1;
      }
      // Waiting for nbyte size
      printf("NetWrite: Waiting for nbyte size...\n");
      size_t writenbyterecv;
      int writenbytemsg;
      if ((writenbytemsg = recv(packet->clientfd, &writenbyterecv, sizeof(writenbyterecv), 0)) == -1) {
        perror("ERROR: NetWrite: was not able to receive the n-byte ");
      }
      size_t writenbyte = ntohl(writenbyterecv);
      printf("NetWrite: Received n-byte: %d\n", writenbyte);

      // wait for buffer
      printf("NetWrite: Waiting for string to write in file...\n");
      int stringmsg;
      if ((stringmsg = recv(packet->clientfd, buffer, MAXBUFFERSIZE, 0)) == -1) {
        perror("ERRROR: NetWrite: Did not receive a string!");
        exit(1);
      }
      buffer[stringmsg] = '\0';
      printf("NetWrite: Received string: %s\n", buffer);

      sleep(1);
      printf("NetWrite: Trying to write to the file\n");
      
          
      int writeresult = write(writefd, buffer, writenbyte);
      int writeresultpayload = htonl(writeresult);
      if (send(packet->clientfd, &writeresultpayload, sizeof(writeresultpayload), 0) == -1) {
        perror("ERROR: NetWrite: Unable to send result");
      }
      // if there was an error getting the resulting size
      if (writeresult == -1) {
        printf("NetWrite: Sending errno: %d\n", errno);
        errnum = htonl(errno);
        if (send(packet->clientfd, &errnum, sizeof(errnum), 0) == -1) {
          perror("ERROR: NetWrite: Issue with sending errno");
        }
      }
      return packet;
}

//this method closes the FD in which was recieved from the open method and returns 0 if successfull 
clientInfo * n_close(clientInfo * packet, char buffer[MAXBUFFERSIZE], int errnum){

      int receive_data;
      int close_data;
      if ((close_data = recv(packet->clientfd, &receive_data, sizeof(receive_data), 0)) == -1){
        perror("ERROR: NetClose: Was not able to receive fd to close it");
      }
      int conv_fd = ntohl(receive_data);
      packet->fd = conv_fd;
      printf("NetClose: Received File Descriptor: %i\n", conv_fd);
      if(conv_fd == -1){
	conv_fd = -1;
      }
      else if(conv_fd < 0){
        conv_fd = conv_fd * -1;
      }

      sleep(1);

      int closed = close(conv_fd);
      int conv_closedFD = htonl(closed);
      if(send(packet->clientfd, &conv_closedFD, sizeof(conv_closedFD), 0) == -1){
        perror("ERROR: NetClose: Issue with sending data/fd back");
      }

      if(closed == -1){
        printf("NetClose: Sending errno :%d\n", errno);
        errnum = htonl(errno);
        if(send(packet->clientfd, &errnum, sizeof(errnum), 0) == -1){
          perror("ERROR: NetClose: Issue with sending errno!");
        }
      }


      removeNode(packet);
      return packet;
}

//switch statment for the different file methods
void *fileCalls(void *element) {

  clientInfo *packet = (clientInfo *)element;
  char buffer[MAXBUFFERSIZE];
  int errnum;
        // NetOpen: 1 NetRead: 2 NetWrite: 3 NetClose: 4
  switch (packet->msg_type) {

    case NETOPEN: // every netopen call, we malloc one table position to include a client

        printf("NetOpen: Request from IP: %s\n", packet->ip_address);
        printf("NetOpen: Waiting for path message...\n");
        packet = n_open(packet, buffer, errnum);
        printf("NetOpen: Finished Operation.\n");
        close(packet->clientfd);
        break;

    case NETREAD:

      printf("NetRead: Requested from IP: %s\n", packet->ip_address);
      printf("NetRead: Waiting for File Descriptor...\n");
      packet = n_read(packet, buffer, errnum);
      printf("NetRead: Finished Operation.\n");
      close(packet->clientfd);
      break;

    case NETWRITE:

      printf("NetWrite: Request from %s\n", packet->ip_address);
      printf("NetWrite: Waiting for File Descriptor...\n");
      packet = n_write(packet, buffer, errnum);
      printf("NetWrite: Finished Operation.\n");
      close(packet->clientfd);
      break;

    case NETCLOSE:

      printf("NetClose requested from %s\n", packet->ip_address);
      printf("NetClose: Waiting for File Descriptor...\n");
      packet = n_close(packet, buffer, errnum);
      printf("NetClose: Finished Operation.\n");
      close(packet->clientfd);
      break;
  }
}

//the main method starts the server and sets all required parameters and then creates a packet and sends it through a pthread
int main(int argc, char * argv[]){

  int sockfd;
  int recfd;
  struct addrinfo hints;
  struct addrinfo *res;

  printf("Starting up netfileserver!\n");

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  getaddrinfo(NULL, PORT, &hints, &res);

  printf("Setting up socket descriptor...\n");
  sockfd = socket(res->ai_family, res->ai_socktype, 0);

  int reuse = 1;

  if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0){
      perror("Setting sockopt failed");
  }

  printf("Binding socket descriptor to IP Address Info...\n");
  bind(sockfd, res->ai_addr, res->ai_addrlen);

  listen(sockfd, 10);

  char currentAddr[INET_ADDRSTRLEN];
  struct sockaddr_storage clientAddr;
  socklen_t addr_size;
  while (1) {
    // accept() waits for connections to come in to connect to
    printf("Waiting to accept new connections...\n");
    addr_size = sizeof clientAddr;
    recfd = accept(sockfd, (struct sockaddr *)&clientAddr, &addr_size);
    if (recfd == -1) {
      perror("accept");
    }

    // inet_ntop puts the client's ip into a string to work with
    inet_ntop(clientAddr.ss_family, (void *)((struct sockaddr *)res->ai_addr), currentAddr, sizeof currentAddr);
    printf("Client connected: %s\n", currentAddr);

    // Set up message to pass to msg_handler()
    clientInfo *packet = malloc(sizeof(clientInfo));
    strcpy(packet->ip_address, currentAddr);
    packet->clientfd = recfd;


    int filemode;
    int modemsg;
    if ((modemsg = recv(recfd, &filemode, sizeof(filemode), 0)) == -1) { // RECEIVING THE CORRESPONDING FILE MODE
      perror("ERROR: Issue with receiving the file mode from the user");
    }
    int mode = ntohl(filemode);
    packet->filemode = mode;
    printf("SUCCESS: Received File mode %d from %s\n", mode, currentAddr);

    // Receiving the first message from the client
    int msg_type;
    int msg;
    if ((msg = recv(recfd, &msg_type, sizeof(int), 0)) == -1) {
      perror("ERROR: Issue with first receiving open/close/read/write code");
      exit(1);
    }
    packet->msg_type = ntohl(msg_type);
    printf("Received: %x from %s\n", packet->msg_type, currentAddr);

    pthread_t child; // CREATING A THREAD FOR EVERY SOCKET BEING CREATED
    pthread_create(&child, NULL, fileCalls, (void *)packet);
  }
  return 0;
}
