#include "gtest/gtest.h"
#include "debug.h"

TEST (main, test) {
  TRC_LOG("\n");
}

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/select.h>

#include <unistd.h>

long kPeriod_MS = 1000 * 1000;
const char * kFifoFileName_Server = "fifo_S_to_C";
const char * kFifoFileName_Ack    = "fifo_C_to_S";
int kRetry_MaxCount = 3;
long kRetry_Period = 500 * kPeriod_MS;
int kFlags_Recv = (O_RDONLY | O_NONBLOCK);
mode_t kMode_Create = (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
int kFlags_Send = (O_WRONLY | O_NONBLOCK);

pthread_mutex_t mutex_ = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
pthread_cond_t  cond_  = PTHREAD_COND_INITIALIZER;

unsigned int get_sender_id () {
  static unsigned int sender_id = 0;
  pthread_mutex_lock(&mutex_);
  sender_id++;
  sender_id &= 0xff;
  pthread_mutex_unlock(&mutex_);
  return sender_id;
}
bool flag = false;

void * waitClientRequestThread(int fd) {
  //struct timespec ts = {3, 0};
  while(1) {
    fd_set fsr;
    FD_ZERO(&fsr);
    FD_SET(fd, &fsr);

    int tmp_result = pselect(fd + 1, &fsr, NULL, NULL, NULL, NULL);
    if (tmp_result == -1) {
      ERR_LOG("select : %s\n", strerror(errno));
    }
    //recieve data
    if (tmp_result) {
      char buff[128];
      if (FD_ISSET(fd, &fsr)) {
        tmp_result = read(fd, buff, 128);
        if (tmp_result == -1) {
          ERR_LOG("read error : %s[%d]\n", strerror(errno), errno);
          return NULL;
        } else if (tmp_result) {
          TRC_LOG("read : %s\n", buff);
          pthread_mutex_lock(&mutex_);
          flag = true;
          pthread_cond_broadcast(&cond_);
          pthread_mutex_unlock(&mutex_);
        } else {
          WAR_LOG("EOF\n");
          return NULL;
        }
      } else {
        WAR_LOG("%d\n", tmp_result);
      }
    } else {
      WAR_LOG("time out\n");
    }
  }
}

void * senderTrhead(int fd) {
  pthread_detach(pthread_self());
  while(1) {
    TRC_LOG("start\n");
    pthread_mutex_lock(&mutex_);
    while (!flag) {
      TRC_LOG("wait\n");
      pthread_cond_wait(&cond_, &mutex_);
    }
    char buffer[128];
    sprintf(buffer, "ack:0x%08x", get_sender_id());
    TRC_LOG("%s\n", buffer);
    write(fd, buffer, strlen(buffer) + 1);
    pthread_mutex_unlock(&mutex_);
    flag=false;
  }
  return NULL;
}



TEST(Fifo, Recv) {
  int tmp_result;
  tmp_result = mkfifo(kFifoFileName_Server, kMode_Create);
  if (tmp_result == -1 && errno != EEXIST) {
    ERR_LOG("open error : %s\n", strerror(errno));
    return;
  }

  tmp_result = mkfifo(kFifoFileName_Ack, kMode_Create);
  if (tmp_result == -1 && errno != EEXIST) {
    ERR_LOG("open error : %s\n", strerror(errno));
    return;
  }

  int fd_recv = open(kFifoFileName_Server, kFlags_Recv);
  if (fd_recv == -1) {
    ERR_LOG("open error : %s[%d]\n", strerror(errno), errno);
    return;
  }
  TRC_LOG("open server recv\n");

  int retry = 0;
  int fd_send = -1;
  while(1) {
    fd_send = open(kFifoFileName_Ack, kFlags_Send);
    if (fd_send != -1) {
      TRC_LOG("open server send\n");
      break;
    } else if (errno == ENXIO && retry < kRetry_MaxCount) {
      struct timespec wait_time = {0, kRetry_Period}; 
      nanosleep(&wait_time, NULL);
      retry++;
    } else {
      ERR_LOG("open error : %s[%d]\n", strerror(errno), errno);
      return;
    }
  }
  pthread_t th, th2;
  pthread_create(&th, NULL, (void *(*)(void *))waitClientRequestThread, (void *)fd_recv);
  pthread_create(&th2, NULL, (void *(*)(void *))senderTrhead, (void *)fd_send);
  pthread_join(th, NULL);
}

TEST(Fifo, Send) {
  int retry = 0;
  int fd_send = -1;
  while(1) {
    fd_send = open(kFifoFileName_Server, kFlags_Send);
    if (fd_send != -1) {
      TRC_LOG("open client send\n");
      break;
    } else if (errno == ENXIO && retry < kRetry_MaxCount) {
      struct timespec wait_time = { 0, kRetry_Period }; 
      nanosleep(&wait_time, NULL);
      retry++;
    } else {
      ERR_LOG("open error : %s[%d]\n", strerror(errno), errno);
      return;
    }
  }

  int fd_recv = open(kFifoFileName_Ack, kFlags_Recv);
  if (fd_recv == -1) {
    ERR_LOG("open error : %s[%d]\n", strerror(errno), errno);
    return;
  }

  TRC_LOG("open client recv\n");
  const char * test = "test";
  for (int i = 0; i < 0x102; i++) {
    int ret = write(fd_send, test, strlen(test) + 1);
    TRC_LOG("write %d\n", ret);
    fd_set fsr;
    FD_ZERO(&fsr);
    char buff[128];
    FD_SET(fd_recv, &fsr); 
    struct timespec wait_ack = {2, 0};
    pselect(fd_recv + 1, &fsr, NULL, NULL, &wait_ack, NULL);
    read(fd_recv, buff, 128);

    TRC_LOG("%s\n", buff);
  }
  sleep(1);
}

int main (int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
