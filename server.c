// gcc -o s server.c -lpthread

#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h> 
#include <sys/socket.h> 
#include <netinet/in.h>
#include <string.h>
#include<pthread.h>
#include <semaphore.h> 
#define PORT 8080
#define USERNAME_LEN 100
#define MSG_LEN 512

char otherUserName[USERNAME_LEN] = {0};
char myUserName[USERNAME_LEN] = {0};
int isClosed = 0;
sem_t mutex;
sem_t accessList;

typedef struct msg {
  char * msgLine;
  struct msg * next;
} msg;

typedef struct list { 
  struct msg *front, *rear; 
	int count;
}list; 

msg * newMsg(char * message, char * username) { 
    msg * temp = (msg *) malloc(sizeof(msg)); 
    temp->next = NULL;
    temp->msgLine = (char *) malloc (MSG_LEN);
    sprintf(temp->msgLine, "%s - %s", username, message);
    return temp; 
}

list * createList() { 
    list * l = (list *)malloc(sizeof(list)); 
    l->front = l->rear = NULL;
    l->count = 0;    
    return l; 
}

void addMsg(list * msgList, char * message, char * username) {

  int shouldAdd = 1;
  sem_wait(&mutex);
  shouldAdd = !isClosed;
  sem_post(&mutex);

  if(!shouldAdd) {
    return;
  }

  msg * incomingMsg = newMsg(message, username);
  if (msgList->rear == NULL) { 
      msgList->front = msgList->rear = incomingMsg; 
      return; 
  } 

  msgList->rear->next = incomingMsg; 
  msgList->rear = incomingMsg; 
  msgList->count += 1;
}

void printMessages(list * l){
  msg * temp = l->front;
  while (temp != NULL) {
    printf("%s\n", temp->msgLine);
    temp = temp->next;
  }
}


int initServer() {

  struct sockaddr_in server_addr;
  int socket_fd, bindStatus;
  socket_fd = socket(AF_INET, SOCK_STREAM, 0);

  if(socket_fd < 0){
    printf("Socket creation failed.\n");
    exit(1);
  } else {
    printf("Socket creation succeeded.\n");
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port =  htons(PORT);

  bindStatus = bind(socket_fd, (struct sockaddr*) & server_addr, sizeof(server_addr)) < 0;
  if (bindStatus < 0) {
    printf("Error while binding.\n");
    exit(1);
  } else {
    printf("Bind to port %d successful.\n", PORT);
  }

  if(listen(socket_fd, 5) == 0){
    printf("Started listening on port %d\nWaiting for a chat request.\n", PORT);
  }else{
    printf("Something went wrong when attempting to listen.\n");
  }

  return socket_fd;
}

void exchangeUserNames(char *otherUserName, char *myUserName, int newSocket) {

    recv(newSocket, otherUserName, 100, 0);
    printf("Other server contacted us. Their username is %s\n", otherUserName);
    send(newSocket, myUserName, 100, 0);
    printf("We sent them our username, now waiting for their welcome message.\n");

}

void exchangeWelcomeMessage(char * otherUserName, char *myUserName, int newSocket) {

    char welcomeMessage[512] = {0};
    char * welcomeReply = "Thanks for connecting. Lets chat!";

    recv(newSocket, welcomeMessage, 512, 0);
    printf("%s - %s\n", otherUserName, welcomeMessage);
    printf("Sending default welcome reply..\n");
    send(newSocket, welcomeReply, 512, 0);
    printf("%s - %s\n", myUserName, welcomeReply);

}

void reformatMsg(char * msg) {
  int i = 0;
  msg[i] = ' ';
  while(msg[i] != '%') {
    i++;
  }
  msg[i] = ' ';
}

list * msgs;

void refreshChat(){
  system("clear");
  printf("------ Chat Record ------\n");
  printMessages(msgs);
  printf("--------------------------\n");
  printf("Enter 1 to send new message:\n");
}

void *send_thread(void * arg) {

  int *fd = (int *) arg;
  int client_fd = * fd;
  int size = MSG_LEN;
  printf("Enter 1 to send a new message:\n");

  while (1) {

    char * reply = (char *) malloc (size);
    int isTyping = 0;
    scanf("%d", &isTyping);

    if(isTyping == 1) {
      sem_wait(&accessList);
      printf("Enter message:\n");
      getdelim(&reply, (size_t * )&size, '%', stdin);
      reformatMsg(reply);
      addMsg(msgs, reply, myUserName);
      refreshChat();
      sem_post(&accessList);
      send(client_fd, reply, size, 0);
    } else {
      refreshChat();
      free(reply);
      continue;
    }    
    

    if(strcmp(reply, " Good Bye! ") == 0) {
      free(reply);
      sem_wait(&mutex);
      isClosed = 1;
      sem_post(&mutex);
      break;
    }

    free(reply);
  }
}

void *recv_thread(void * arg) {

  int *fd = (int *) arg;
  int client_fd = * fd;
  int size = MSG_LEN;

  while (1) {

    char * message = (char *) malloc (size);
    recv(client_fd, message, size, 0);
    sem_wait(&accessList);
      addMsg(msgs, message, otherUserName);
      refreshChat();
    sem_post(&accessList);

    if(strcmp(message, " Good Bye! ") == 0) {
      free(message);
      sem_wait(&mutex);
      isClosed = 1;
      sem_post(&mutex);
      break;
    }

    free(message);
  }
}

int main(){

  system("clear");
  printf("Initializing chat app client\n");

	sem_init(&mutex, 0, 1);
	sem_init(&accessList, 0, 1);

  msgs = createList();

  printf("Enter your username:\n");
  scanf("%s", myUserName);
  printf("Username set. Now initializing server.\n");

  int socket_fd = initServer();
  struct sockaddr_in client_addr;
  socklen_t cliSize;
  int newSocket = accept(socket_fd, (struct sockaddr*)&client_addr, &cliSize);
  if(newSocket < 0){
    printf("Something went wrong while accepting");
    exit(1);
  }

  exchangeUserNames(otherUserName, myUserName, newSocket);
  exchangeWelcomeMessage(otherUserName, myUserName, newSocket);

  pthread_t sender, receiver;

  pthread_create(&sender, NULL, send_thread, &newSocket);
  pthread_create(&receiver, NULL, recv_thread, &newSocket);

  while (1) {
    int shouldBreak = 0;
    sem_wait(&mutex);
    shouldBreak = isClosed;
	  sem_post(&mutex);

    if(shouldBreak) {
      pthread_cancel(sender);
      pthread_cancel(receiver);
      system("clear");
      printf("------ Chat Record ------\n");
      printMessages(msgs);
      break;
    }
  }

  printf("\nClosing chat. Thanks!");
  close(newSocket);
  close(socket_fd);
  sem_destroy(&mutex); 
	sem_destroy(&accessList); 
  free(msgs);
  return 0;
}
