CXX = g++
TARGET = a.out
SRCS := main.cpp debug.cpp
OBJS := $(SRCS:%.cpp=%.o)
DEPS := $(SRCS:%.cpp=%.d)
GTEST_DIR := gtest-1.7.0
CXXFLAGS = -g -Wall -isystem $(GTEST_DIR)/include -pthread
LDFLAGS = libgtest.a -lrt

all: $(TARGET)
-include $(DEPS)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $<

.PHONY: clean
clean:
	$(RM) $(TARGET) $(OBJS) $(DEPS)

.PHONY: gtest
gtest:
	$(CXX) -isystem $(GTEST_DIR)/include -I$(GTEST_DIR) \
	-pthread -c $(GTEST_DIR)/src/gtest-all.cc
	$(AR) -rv libgtest.a gtest-all.o
	$(RM) gtest-all.o

# vim: set ts=8 sts=2 sw=2 noet:
