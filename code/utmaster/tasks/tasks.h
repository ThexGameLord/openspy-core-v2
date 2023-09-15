#ifndef _MM_TASKS_H
#define _MM_TASKS_H
#include <string>

#include <OS/Task/TaskScheduler.h>
#include <OS/Task/ScheduledTask.h>

#include <OS/MessageQueue/MQInterface.h>

#define MM_REDIS_EXPIRE_TIME 500
namespace UT {
    class Driver;
    class Peer;
}

namespace MM {

	class PlayerRecord {
		public:
			int id;
			std::string name;
			int ping;
			int score;
			int stats_id;
	};
	class ServerRecord {
		public:
			int id;
			OS::Address m_address;
			int gameid;
			std::string hostname;
			std::string level;
			std::string game_group;
			std::string bot_level;
			int num_players;
			int max_players;

			std::map<std::string, std::string> m_rules;
			std::vector<PlayerRecord> m_players;

			std::vector<std::string> m_mutators;

			bool isStandardMutator(std::string mutator) {
				return mutator.compare("DMMutator") == 0;
			}

			bool isStandardServer() {
				if(m_mutators.size() > 0) {
					std::vector<std::string>::iterator it = m_mutators.begin();
					while(it != m_mutators.end()) {
						std::string s = *it;
						if(!isStandardMutator(s)) return false;
						it++;
					}
				}
				return true;
			}
	};

	enum UTMasterRequestType {
		UTMasterRequestType_ListServers,
		UTMasterRequestType_Heartbeat,
        UTMasterRequestType_DeleteServer,
        UTMasterRequestType_InternalLoadGamename, //used on startup to load game data
	};

	class MMTaskResponse {
		public:
			int server_id;
			UT::Peer *peer;
			std::vector<ServerRecord> server_records;
	};

	typedef void (*MMTaskResponseCallback)(MMTaskResponse response);

	
	enum EQueryType //from MasterServerClient.uc (IpDrv.u)
	{
		QT_Equals,
		QT_NotEquals,
		QT_LessThan,
		QT_LessThanEquals,
		QT_GreaterThan,
		QT_GreaterThanEquals,
		QT_Disabled		// if QT_Disabled, query item will not be added
	};

	class FilterProperties {
		public:
			std::string field;
			std::string property;
			EQueryType type;
	};

	class UTMasterRequest {
		public:
			UTMasterRequest() {
				type = UTMasterRequestType_ListServers;
				peer = NULL;
				driver = NULL;
			}
			~UTMasterRequest() {

			}

			int type;

			UT::Driver *driver;
			UT::Peer *peer;

			ServerRecord record;
			std::vector<FilterProperties> m_filters;

			MMTaskResponseCallback callback;

	};

	TaskScheduler<UTMasterRequest, TaskThreadData> *InitTasks(INetServer *server);

	bool PerformAllocateServerId(UTMasterRequest request, TaskThreadData *thread_data);
	bool PerformHeartbeat(UTMasterRequest request, TaskThreadData *thread_data);
	bool PerformListServers(UTMasterRequest request, TaskThreadData *thread_data);
	bool PerformDeleteServer(UTMasterRequest request, TaskThreadData *thread_data);
	bool PerformInternalLoadGameData(UTMasterRequest request, TaskThreadData *thread_data);

	int GetServerID(TaskThreadData *thread_data);
	bool isServerDeleted(TaskThreadData* thread_data, std::string server_key);
	bool serverRecordExists(TaskThreadData* thread_data, std::string server_key);
	std::string GetServerKey_FromIPMap(UTMasterRequest request, TaskThreadData *thread_data, OS::GameData game_info);

	void selectQRRedisDB(TaskThreadData *thread_data);

	extern const char *mm_channel_exchange;
    extern const char *mp_pk_name;
	extern const char *mm_server_event_routingkey;

}
#endif //_MM_TASKS_H
