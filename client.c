// gcc -o c client.c -lpthread

#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h> 
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h> 
#define PORT 8080
#define USERNAME_LEN 100
#define MSG_LEN 512
#define SERVER_IP "192.168.0.103"
#define CLIENT_IP "192.168.0.108"

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

int initClientSocket() {

  int clientSocket;
	clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(clientSocket < 0){
    printf("Socket creation failed.\n");
    exit(1);
	} else {
  	printf("Client Socket created successfully.\n");
	}

  struct sockaddr_in localaddr;
  localaddr.sin_family = AF_INET;
  localaddr.sin_addr.s_addr = inet_addr(CLIENT_IP);
  localaddr.sin_port = 8081;
  bind(clientSocket, (struct sockaddr *)&localaddr, sizeof(localaddr));

  return clientSocket;
}

void exchangeUserNames(char *myUserName, char *otherUserName, int client_fd) {

	int status;

	struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
  server_addr.sin_port =  htons (PORT);

	status = connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if(status < 0){
		printf("Failed to connect.\n");
		exit(1);
	}
	printf("Connection established. Now sending username.\n");
  send(client_fd, myUserName, 100, 0);
  recv(client_fd, otherUserName, 100, 0);
  printf("%s is on the other side.\n", otherUserName);

}

void exchangeWelcomeMessage(char * otherUserName, char *myUserName, int client_fd) {

  char welcomeReply[512] = {0};
  char * welcomeMessage = "Hi there, hoping to chat..";
    
	printf("Now sending default welcome message.\n");
  send(client_fd, welcomeMessage, 512, 0);
  printf("%s - %s\n", myUserName, welcomeMessage);
  recv(client_fd, welcomeReply, 512, 0);
  printf("%s - %s\n", otherUserName, welcomeReply);

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
  printf("Username set. Now contacting other server.\n");

  int client_fd = initClientSocket();
  exchangeUserNames(myUserName, otherUserName, client_fd);
  exchangeWelcomeMessage(otherUserName, myUserName, client_fd);

  pthread_t sender, receiver;

  pthread_create(&sender, NULL, send_thread, &client_fd);
  pthread_create(&receiver, NULL, recv_thread, &client_fd);

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
  close(client_fd);
  sem_destroy(&mutex); 
	sem_destroy(&accessList); 
  free(msgs);
  return 0;
}
