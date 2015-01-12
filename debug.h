#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include <stdint.h>

enum DebugLevelType {
  ERR = 3,
  WAR,
  INF,
  TRC,
  DBG,
};

extern const int32_t kDebugLevel;
extern const char * kPrefix;
extern int64_t get_upticks_ns();

#define P_LVL(lvl) (\
	((lvl) == ERR ) ? "\x1b[31mERR\x1b[0m" : \
	((lvl) == WAR ) ? "\x1b[33mWAR\x1b[0m" : \
	((lvl) == INF ) ? "\x1b[34mINF\x1b[0m" : \
	((lvl) == TRC ) ? "TRC" : \
	((lvl) == DBG ) ? "DBG" : \
	"")

#define LOG(lvl, fmt, ...) \
	do { \
		if (lvl <= kDebugLevel) {	\
			printf ("** \x1b[1;37m%s\x1b[0m ** [%s:%d] <<%s>> [%s] <%04ld>: " fmt , kPrefix, __FILE__, __LINE__, __FUNCTION__, P_LVL(lvl), get_upticks_ns()/1000000, ##__VA_ARGS__); \
		} \
	} while(0)


#define LOG_ERR(fmt, ...) LOG(ERR, fmt, ## __VA_ARGS__)
#define LOG_WAR(fmt, ...) LOG(WAR, fmt, ## __VA_ARGS__)
#define LOG_INF(fmt, ...) LOG(INF, fmt, ## __VA_ARGS__)
#define LOG_TRC(fmt, ...) LOG(TRC, fmt, ## __VA_ARGS__)
#define LOG_DBG(fmt, ...) LOG(DBG, fmt, ## __VA_ARGS__)



#endif // DEBUG_H
