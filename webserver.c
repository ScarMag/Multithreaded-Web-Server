#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <unistd.h>
#include "webserver.h"

void *producer(void *arg);
void *consumer(void *arg);
void thread_control();

#define MAX_REQUEST 100

int port, numThread;

int request[MAX_REQUEST]; // shared buffer
int nextProduced = 0;     // counter for index of next request to be produced
int nextConsumed = 0;     // counter for index of next request to be consumed
int numRequest = 0;       // number of active requests in shared buffer

// semaphores and a mutex lock
sem_t sem_full;
sem_t sem_empty;
pthread_mutex_t mutex;


void req_handler() {
  int r;
  struct sockaddr_in sin;
  struct sockaddr_in peer;
  int peer_len = sizeof(peer);
  int sock;
  
  sock = socket(AF_INET, SOCK_STREAM, 0);
  
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(port);
  r = bind(sock, (struct sockaddr *) &sin, sizeof(sin));
  if(r < 0) {
    perror("Error binding socket:");
    exit(0);
  }

  r = listen(sock, 5);
  if(r < 0) {
    perror("Error listening socket:");
    exit(0);
  }

  printf("HTTP server listening on port %d\n", port);

  // Create a thread and pass fd to producer()
  // Array needed to pass fd's, too many produced & s is rewritten
  int s[MAX_REQUEST];
  int j = 0;
  while (1) {
    pthread_t tid;
    
    s[j] = accept(sock, NULL, NULL);
    if (s < 0) break;
  
    pthread_create(&tid, NULL, producer, (void *) &s[j]);
    j = (j + 1) % MAX_REQUEST;
  }
  
  close(sock);
}

void *producer(void *arg) {
  int fd = *((int *) arg);

  sem_wait(&sem_empty);
  pthread_mutex_trylock(&mutex);
  
  request[nextProduced] = fd;
  numRequest++;
  nextProduced = (nextProduced + 1) % MAX_REQUEST;
  
  pthread_mutex_unlock(&mutex);
  sem_post(&sem_full);
    
  pthread_exit(NULL);
}

void *consumer(void *arg) {
  int fd;

  while (1) {
    sem_wait(&sem_full);
    pthread_mutex_trylock(&mutex);
    
    fd = request[nextConsumed];
    numRequest = (numRequest < 0) ? 0 : --numRequest;
    nextConsumed = (nextConsumed + 1) % MAX_REQUEST;
    
    pthread_mutex_unlock(&mutex);
    sem_post(&sem_empty);
    
    process(fd);
  }
}

void thread_control() {
  pthread_t tid[numThread];
  int *retval;
  int i;
  
  for (i = 0; i < numThread; i++) {
    pthread_create(&tid[i], NULL, consumer, NULL);
  }

  for (i = 0; i < numThread; i++) {
    pthread_join(tid[i], (void **) &retval);

    // Code from client.c, lines 69-71
    while ((*retval) <= 0) {
      pthread_create(&tid[i], NULL, consumer, NULL);
      pthread_join(tid[i], (void **) &retval);
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2 || atoi(argv[1]) < 2000 || atoi(argv[1]) > 50000) {
      fprintf(stderr, "./webserver PORT(2001 ~ 49999) (#_of_threads) (crash_rate(%%)\n");
      return 0;
  }

  pthread_t tid;
		
  // port number
  port = atoi(argv[1]);
		
  // # of worker thread
  if(argc > 2) 
    numThread = atoi(argv[2]);
  else numThread = 1;

  // crash rate
  if(argc > 3) 
    CRASH = atoi(argv[3]);
  if(CRASH > 50) CRASH = 50;
		
  sem_init(&sem_empty, 0, MAX_REQUEST);
  sem_init(&sem_full, 0, 0);
  pthread_mutex_init(&mutex, NULL);

  printf("[pid %d] CRASH RATE = %d%%\n", getpid(), CRASH);

  pthread_create(&tid, NULL, (void *) req_handler, NULL);
  thread_control();
  
  sem_destroy(&sem_empty);
  sem_destroy(&sem_full);
  pthread_mutex_destroy(&mutex);
  
  pthread_exit(NULL);
}

