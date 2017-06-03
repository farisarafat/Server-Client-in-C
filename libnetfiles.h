#define MAXBUFFERSIZE 512
#define PORT "10112"
#define INVALID_FILE_MODE -214
#define DENIED_ACCESS -215
#include <netinet/in.h>

int netserverinit(char *hostname, int filemode);
int netopen(const char *pathname, int flags);
ssize_t netread(int filedes, void *buf, size_t nbyte);
int connectMethod(int socketDescript);
ssize_t netwrite(int filedes, const void *buf, size_t nbyte);
int netclose(int fd);

#define NETOPEN 1
#define NETREAD 2
#define NETWRITE 3
#define NETCLOSE 4

// FILE MODES
#define UNRESTRICTED 0
#define EXCLUSIVE 1
#define TRANSACTION 2

typedef struct msgData {
  char ip_address[INET_ADDRSTRLEN];
  int clientfd;
  int msg_type; // associated with read write etc
  int filemode;
  int fd; //file descriptor
  int flags;
  char * fileName;
  //maybe add a unique number for every linked list element
} clientInfo;

//node for fdTable
typedef struct fdTable {
    struct msgData *ci;
    struct fdTable *next;

}fdTables;
