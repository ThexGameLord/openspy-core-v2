#ifndef _GPDRIVER_H
#define _GPDRIVER_H
#include <stdint.h>
#include "../main.h"
#include <OS/Net/drivers/TCPDriver.h>

#include "UTPeer.h"

#include <map>
#include <vector>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

namespace UT {
	class Peer;
	class PackageItem {
	public:
		std::string guid;
		std::string hash;
		int version;
	};
	class Config {
		public:
			Config() {
				gameid = 0;
				is_server = 0;
				latest_client_version = 0;
			}
			std::string clientName;
			int gameid;
			OS::GameData game_data;
			std::string motd;
			bool is_server;
			int latest_client_version;
			std::vector<UT::PackageItem> packages;
	};

	class Driver : public OS::TCPDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port, bool proxyHeaders = false);
		void SetConfig(std::vector<UT::Config*> config);
		UT::Config *FindConfigByClientName(std::string clientName);
        std::vector<UT::Config *> GetConfig() { return m_config; }
	protected:
		virtual INetPeer *CreatePeer(INetIOSocket *socket);
		std::vector<UT::Config *> m_config;
	};
}
#endif //_SBDRIVER_H
