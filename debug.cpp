#include "debug.h"
#include "time.h"

const int32_t kDebugLevel = DBG;
const char * kPrefix = "GTEST";

int64_t get_upticks_ns() {
  struct timespec ts = {0,0}; 
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    //失敗時は-1を返す
    //失敗原因は呼び出し元でerrnoを参照することで知ることができる
    return -1;
  }
  return ts.tv_sec * 1000000000 + ts.tv_nsec;
}

