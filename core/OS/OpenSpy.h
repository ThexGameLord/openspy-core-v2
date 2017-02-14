#ifndef _OPENSPY_H
#define _OPENSPY_H
#ifdef _WIN32
#include <Windows.h>
#include <WinSock.h>
#include "Threads/Win32/WinThread.h"
typedef int socklen_t;
#define snprintf sprintf_s
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define MSG_NOSIGNAL 0
int gettimeofday(struct timeval *tp, struct timezone *tzp);
#else
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/times.h>
#include "Threads/PThreads/PThread.h"
#include "Threads/PThreads/PMutex.h"

#define stricmp strcasecmp
#define sprintf_s snprintf
#define strnicmp strncasecmp
#define vsprintf_s vsnprintf
#define _strnicmp strnicmp

#endif
#include <stdlib.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <stdint.h>
#include <memory.h>
#include <map>
#include <hiredis/hiredis.h>

#include <OS/Logger.h>

#define OPENSPY_WEBSERVICES_URL "http://10.10.10.10"
namespace OS {
	class Logger;
	extern Logger *g_logger;
	void LogText(ELogLevel level, const char *fmt, ...);
	///////////////////////
	/// XXX: put in os/geo/region.h
	typedef struct {
		const char *countrycode;
		const char *countryname;
		int region;
	} countryRegion;
	///////////////////
	//// XXX: put in os/game.h

	//key types for the key type list
	#define KEYTYPE_STRING	0
	#define KEYTYPE_BYTE	1
	#define KEYTYPE_SHORT	2
	#define KEYTYPE_INT		3


	#define OS_MAX_GAMENAME 32
	#define OS_MAX_DESCRIPTION 64
	#define OS_MAX_SECRETKEY 7
	typedef struct {
		int gameid;
		int queryport;
		char gamename[OS_MAX_GAMENAME];
		char description[OS_MAX_DESCRIPTION];
		char secretkey[OS_MAX_SECRETKEY];
		char disabled_services; //0= ok, 1 = temp, 2 = perm
		std::vector<std::string> popular_values;

		std::map<std::string, uint8_t> push_keys; //SB push keys + type(hostname/KEYTYPE_STRING)
	} GameData;
	GameData GetGameByName(const char *from_gamename, redisContext *redis_ctx = NULL);
	GameData GetGameByID(int gameid, redisContext *redis_ctx = NULL);
	enum ERedisDB {
		ERedisDB_QR,
		ERedisDB_SBGroups,
		ERedisDB_Game,
		ERedisDB_NatNeg,
		ERedisDB_Chat
	};


	class Address {
	public:
		Address(struct sockaddr_in);
		Address(const char *str);
		Address();
		Address(uint32_t ip, uint16_t port);
		uint32_t GetIP() { return ip; };
		uint16_t GetPort() { return port; };
		const struct sockaddr_in GetInAddr();
		std::string ToString(bool ip_only = false);
	//private:
		uint32_t ip;
		uint16_t port;
	};

	template<typename Out>
	void split(const std::string &s, char delim, Out result);
	std::vector<std::string> split(const std::string &s, char delim);
	
	void Init(const char *appName);
	void Shutdown();
	std::map<std::string, std::string> KeyStringToMap(std::string input);
	std::vector<std::string> KeyStringToVector(std::string input);
	std::string strip_quotes(std::string s);
	std::string strip_whitespace(std::string s);

	#define MAX_BASE64_STR 768
	void Base64StrToBin(const char *str, uint8_t **out, int &len);
	const char *BinToBase64Str(const uint8_t *in, int in_len);

	const char *MD5String(const char *string);

	//thread
	CThread *CreateThread(ThreadEntry *entry, void *param, bool auto_start);
	CMutex *CreateMutex();

	void Sleep(int time_ms);
}



#endif //_OPENSPY_H
