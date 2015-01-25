#include "gtest/gtest.h"
#include "debug.h"

TEST (main, test) {
  TRC_LOG("\n");
}

class Context {
};
 
//メンバ変数を持ってはいけない
//エラーハンドリングしたければcontextにもたせる
class State {
 friend class StateMachine;
 protected:
  State() {}
  virtual ~State() {}
  State & operator=(State &);
  State(const State &); 

  //StateList
  //ex: InitialClass this is State child class
  //    static InitialClass state;
  //    return &state;
  static State * initial();
  static State * pausing();
  //static State * Playing();
  //static State * finish();
  
  //EventList
  //EventDataを渡せるようにする?
  //contextに渡してもよい?
  virtual State * init(Context * ctx);
  virtual State * start(Context * ctx);
  virtual State * stop(Context * ctx);
  virtual State * deinit(Context * ctx);
  virtual const char * get_StateName() = 0; 
};

State * State::init(Context * ctx) {
  (void)ctx;
  WAR_LOG("not difine this event [%s]\n", get_StateName()); 
  return this; 
}

State * State::start(Context * ctx) {
  (void)ctx;
  WAR_LOG("not difine this event [%s]\n", get_StateName()); 
  return this; 
}

State * State::stop(Context * ctx) {
  (void)ctx;
  WAR_LOG("not difine this event [%s]\n", get_StateName()); 
  return this; 
}

State * State::deinit(Context * ctx) {
  (void)ctx;
  WAR_LOG("not difine this event [%s]\n", get_StateName()); 
  return this; 
}

class InitialState : protected State {
 private:
 const char * get_StateName() { return "initial"; }
 State * init(Context * ctx);
};

State * InitialState::init(Context * ctx) {
  (void)ctx;
  return pausing();
}

class PausingState : protected State {
 private:
 const char * get_StateName() { return "Pausing"; }
};

State * State::initial() {
  static InitialState state;
  return (State*)&state;
}
State * State::pausing(){
  static PausingState state;
  return (State*)&state;
}


class StateMachine {
 private:
  State * state_;
  Context * ctx_;
 public:
  StateMachine(Context * ctx) : ctx_(ctx) {
    state_ = State::initial();
  }
  //これが実際のイベント state_のI/Fを呼ぶ
  int init() {
    state_ = state_->init(ctx_);
    return 0;
  }
};

TEST(state, basic) {
  Context ctx;
  StateMachine sm(&ctx);
  sm.init();
  sm.init();
}


int main (int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
