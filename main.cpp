#include "gtest/gtest.h"
#include "debug.h"
#include <pthread.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <memory>

const static char * kFileName = "fifo";
const static size_t kServerBufferSize = 128;
const static char * kTestStrs[] = {
  "test1",
  "test2",
  "test3",
  "test4",
  "testtest6",
  "test5",
};

class FileHelper{
 public:
  FileHelper(const char * file_name) : file_name_(file_name), fd_(-1) {}
  virtual ~FileHelper() {
    if(is_active()) { 
      ::close(fd_);
    }
  }
  bool is_active() { return (fd_ != -1); }
  virtual bool connect() = 0;
 protected:
  FileHelper(const FileHelper &);
  FileHelper & operator=(const FileHelper &);
  const char * file_name_;
  //connectでfdを設定することその後変更はしない
  int fd_;
};

const struct timespec kDefualtOpenSleetime = {0, 500 * 1000000};
const int kDefualtOpenTryCount = 10;

//スレッドセーフではない
class SlaveFileHelper : public FileHelper {
 public:
  SlaveFileHelper(const char * file_name) : FileHelper(file_name)  , every_open_sleeptime_(kDefualtOpenSleetime), max_open_try_count_(kDefualtOpenTryCount) {} 
  virtual ~SlaveFileHelper() {}
  void setProperty(struct timespec every_open_sleeptime, int max_open_try_count) {
    every_open_sleeptime_ = every_open_sleeptime;
    max_open_try_count_ = max_open_try_count;
  }
  int write(const void* data, size_t data_size);

  //失敗時はerrnoの値を調べることで原因がわかる
  bool connect();
 private:
  //server側がfileopenするまでのリトライ間隔
  struct timespec every_open_sleeptime_;
  //server側がfileopenするまでの最大リトライ回数
  int max_open_try_count_;
};

bool SlaveFileHelper::connect() {
  //すでにserverと接続済みの場合
  if(is_active()) {
    return true;
  } 
  for(int i = 0; i < max_open_try_count_; i++) {
    fd_ = open(kFileName, O_WRONLY | O_NONBLOCK);
    if(fd_ != -1) {
      //成功
      LOG_DBG("fd = %d, retry_count = %u\n", fd_, i);
      break;
    } else if (ENXIO ==  errno) {
        //serverがオープンするまで待つ
        nanosleep(&every_open_sleeptime_, NULL);
        continue; 
    } else {
        //失敗
        return false;
    }
  }
  //timeout発生時
  if(fd_ == -1) {
    errno = ETIMEDOUT;
    return false;
  }
  return true;
}

int SlaveFileHelper::write(const void * data, size_t data_size) {
  return ::write(fd_, data, data_size);
}

class MasterFileHelper : public FileHelper {
 public:
  MasterFileHelper(const char * file_name) : FileHelper(file_name) {} 
  virtual ~MasterFileHelper() {}
  int read(void * data, size_t data_size, struct timeval * timeout_time);

  //失敗時はerrnoの値を調べることで原因がわかる
  bool connect();
 private:
};

int MasterFileHelper::read(void * data, size_t data_size, struct timeval * timeout_time) { 
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(fd_ , &rfds); 

  if(::select(fd_ + 1, &rfds, NULL, NULL, timeout_time) == -1) {
    return -1;
  }
  return ::read(fd_, data, kServerBufferSize);
}

bool MasterFileHelper::connect() {
  fd_ = open(kFileName, O_RDONLY | O_NONBLOCK);
  return (fd_ != -1);
}


template <class Master, class Slave>
class Singleton {
 public:
  virtual Master * newMaster(int id) = 0;
  virtual Slave * newSlave(int id) = 0;
 protected:
  Singleton() {}
  virtual ~Singleton() {}
  static pthread_mutex_t mutex_; 

  class MutexLocker {
   public: 
    MutexLocker(pthread_mutex_t * mutex) : mutex_(mutex) { pthread_mutex_lock(mutex_); };
    virtual ~MutexLocker() { pthread_mutex_unlock(mutex_); }
   private:
    pthread_mutex_t * mutex_;
  };
 private:
  Singleton & operator=(const Singleton &);
  Singleton(const Singleton &);
};

template <class Master, class Slave>
pthread_mutex_t Singleton<Master, Slave>::mutex_ = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;



class FifoFactroy : public Singleton<MasterFileHelper, SlaveFileHelper> {
 public:
  static FifoFactroy * getInstance();
  FifoFactroy() : Singleton<MasterFileHelper, SlaveFileHelper>() {}
  ~FifoFactroy() {}
  SlaveFileHelper * newSlave(int id) {
    if(id < max_file_count_) {
      LOG_TRC("%s\n", file_names_[id]);
      return new SlaveFileHelper(file_names_[id]);
    } else {
      return NULL;
    }
  }
  MasterFileHelper * newMaster(int id) {
    if(id < max_file_count_) {
      LOG_TRC("%s\n", file_names_[id]);
      return new MasterFileHelper(file_names_[id]);
    } else {
      return NULL;
    }
  }

 private:
  FifoFactroy * create() { return new FifoFactroy(); }
  static FifoFactroy * my_;
  static const char ** file_names_;
  static const int max_file_count_;
  int * is_slaves_active;
  int * is_;
};

const char * file_list[] = {
  "fifo"
};

const char ** FifoFactroy::file_names_ = file_list;
const int FifoFactroy::max_file_count_ = (int)(sizeof(file_list)/sizeof(file_list[0]));
FifoFactroy * FifoFactroy::my_ = NULL;

FifoFactroy * FifoFactroy::getInstance() {
  if(my_ == NULL) {
    MutexLocker ml(&mutex_);
    if(my_ == NULL) {
      my_ = new FifoFactroy();
    }
  }
  return my_;
}

static void client_test_body(void * param);
void * client_test_thread(void * param) {
  client_test_body(param);
  return NULL;
}
//gtestのASERT_XX呼ぶためのclient_test_threadのヘルパー関数
void client_test_body(void * param) {
  //serverがpipeをオープンするまでリトライを続ける
  LOG_TRC("start \n");
  FifoFactroy * factroy = FifoFactroy::getInstance();
  std::auto_ptr<SlaveFileHelper> sh(factroy->newSlave(0));
  struct timespec ts = {1,0};
  sh->setProperty(ts, 5);
  ASSERT_TRUE(sh->connect()) << strerror(errno) << "[" << errno << "]";
  for (size_t i = 0; i < sizeof(kTestStrs) / sizeof(kTestStrs[0]); i++) {
    ASSERT_NE(-1, sh->write(kTestStrs[i], strlen(kTestStrs[i]) + 1)) << strerror(errno) << " [" << errno << "]";
    struct timespec ts = {0, 10 * 1000 * 1000};
    nanosleep(&ts, NULL);
    LOG_TRC("%s\n", kTestStrs[i]);
  }
}

static void server_test_body(void * param);
void * server_test_thread(void * param) {
  server_test_body(param);
  return NULL;
}
//gtestのASERT_XX呼ぶためのclient_test_threadのヘルパー関数
void server_test_body(void * param) {
  LOG_TRC("start\n");
  FifoFactroy * factroy = FifoFactroy::getInstance();
  std::auto_ptr<MasterFileHelper> mh(factroy->newMaster(0));
  int read_count = 0;
  ASSERT_TRUE(mh->connect()) << strerror(errno) << "[" << errno << "]";
  while(1) {
    char * buffer[kServerBufferSize];
    int ret = mh->read(buffer, kServerBufferSize, NULL);
    ASSERT_NE(-1, ret) << strerror(errno) << "[" << errno << "]";
    if (ret == 0) {
      LOG_TRC("ALL Clients closed\n");
      break;
    }
    ASSERT_EQ(ret, strlen(kTestStrs[read_count])+1);
    ASSERT_STREQ(kTestStrs[read_count], (const char*)buffer) << "read_count = " << read_count;
    read_count++;
  }
}

TEST(Fifo, basic) {
  if (mkfifo(kFileName, 0666) != 0) {
    ASSERT_EQ(EEXIST, errno);
  }
  pthread_t client_th, server_th;
  pthread_create(&client_th, NULL, client_test_thread, NULL);
  pthread_create(&server_th, NULL, server_test_thread, NULL);
  pthread_join(client_th, NULL);
  pthread_join(server_th, NULL);
}

TEST(Fifo, client_connect_time_out) {
  SlaveFileHelper sh(kFileName);
  ASSERT_FALSE(sh.connect());
  ASSERT_EQ(ETIMEDOUT, errno);
}

int main (int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
