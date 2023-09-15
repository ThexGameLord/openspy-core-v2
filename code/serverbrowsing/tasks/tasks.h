#ifndef _MM_TASKS_H
#define _MM_TASKS_H
#include <string>

#include <OS/Task/TaskScheduler.h>
#include <OS/Task/ScheduledTask.h>

#include <OS/MessageQueue/MQInterface.h>

#define NN_REDIS_EXPIRE_TIME 500

class CToken;
namespace SB {
    class Peer;
    class Driver;
    class Server;
}
namespace MM {

	class Server {
		public:
			Server() {
				id = 0;
				allow_unsolicited_udp = false;
				deleted = false;
			}
			~Server() {

			}
			OS::Address wan_address;
			OS::Address lan_address;
			OS::Address icmp_address;
			bool allow_unsolicited_udp;

			OS::GameData game;
			OS::countryRegion region;
			std::map<std::string, std::string> kvFields;

			std::map<int, std::map<std::string, std::string> > kvPlayers;
			std::map<int, std::map<std::string, std::string> > kvTeams;

			std::string key;


			int id;		
			bool deleted;
	};

	class ServerListQuery {
		public:
			ServerListQuery() {
				first_set = false;
				last_set = false;
			}
			~ServerListQuery() {

			}
			std::vector<std::string> requested_fields;

			std::vector<std::string> captured_basic_fields;
			std::vector<std::string> captured_player_fields;
			std::vector<std::string> captured_team_fields;
			std::vector<Server *> list;
			bool first_set;
			bool last_set;
	};

	class sServerListReq {
		public:
			sServerListReq() {
				protocol_version = 0;
				encoding_version = 0;
				game_version = 0;
				send_groups = false;
				push_updates = false;
				no_server_list = false;
				no_list_cache = false;
				send_fields_for_all = false;
				all_keys = false;
				all_player_keys = false;
				all_team_keys = false;
				send_compressed = true;
			}
			~sServerListReq() {

			}
			uint8_t protocol_version;
			uint8_t encoding_version;
			uint32_t game_version;
		
			std::string filter;
			//const char *field_list;
			std::vector<std::string> field_list;
		
			uint32_t source_ip; //not entire sure what this is for atm
			uint32_t max_results;
		
			bool send_groups;
			//bool send_wan_ip;
			bool push_updates;
			bool no_server_list;
			bool no_list_cache;
			bool send_fields_for_all;
			bool send_compressed;
		

			//used after lookup
			OS::GameData m_for_game;
			OS::GameData m_from_game;


			//used before lookup
			std::string m_for_gamename;
			std::string m_from_gamename;

			bool all_keys;
			bool all_player_keys;
			bool all_team_keys;
			bool include_deleted;
		
	};

	enum EMMQueryRequestType {
		EMMQueryRequestType_GetServers,
		EMMQueryRequestType_GetGroups,
		EMMQueryRequestType_GetServerByKey,
		EMMQueryRequestType_GetServerByIP,
		EMMQueryRequestType_SubmitData,
		EMMQueryRequestType_GetGameInfoByGameName,
		EMMQueryRequestType_GetGameInfoPairByGameName, //get 2 game names in same thread
	};

	class MMQueryRequest {
		public:
			MMQueryRequest() {
				type = EMMQueryRequestType_GetServers;
				peer = NULL;
				driver = NULL;
				extra = NULL;
			}
			~MMQueryRequest() {

			}

			int type;
			//union {
				sServerListReq req;
				//submit data
					OS::Buffer buffer;
					OS::Address from;
					OS::Address to;
//
				OS::Address address; //used for GetServerByIP
				std::string key; //used for GetServerByKey
			//} sQueryData;
			SB::Driver *driver;
			SB::Peer *peer;

			std::string gamenames[2];
			void *extra;
	};

	TaskScheduler<MMQueryRequest, TaskThreadData> *InitTasks(INetServer *server);

    bool PerformGetServers(MMQueryRequest request, TaskThreadData *thread_data);
    bool PerformGetGroups(MMQueryRequest request, TaskThreadData *thread_data);
    bool PerformGetServerByKey(MMQueryRequest request, TaskThreadData *thread_data);
    bool PerformGetServerByIP(MMQueryRequest request, TaskThreadData *thread_data);
    bool PerformSubmitData(MMQueryRequest request, TaskThreadData *thread_data);
    bool PerformGetGameInfoByGameName(MMQueryRequest request, TaskThreadData *thread_data);
    bool PerformGetGameInfoPairByGameName(MMQueryRequest request, TaskThreadData *thread_data);

    //server update functions
    bool Handle_ServerEventMsg(TaskThreadData *thread_data, std::string message);

	//shared functions
	void AppendServerEntry(TaskThreadData *thread_data, std::string entry_name, ServerListQuery *ret, bool all_keys, bool include_deleted, const sServerListReq *req);
	void AppendGroupEntry(TaskThreadData *thread_data, const char *entry_name, ServerListQuery *ret, bool all_keys, const MMQueryRequest *request, std::vector<CToken> token_list);

	void BuildServerListRequestData(const sServerListReq* req, std::vector<std::string>& basic_lookup_keys, std::vector<std::string>& lookup_keys, std::ostringstream& basic_lookup_str, std::ostringstream& lookup_str, std::vector<CToken>& out_token_list, int peer_version);
	void AppendServerEntries(TaskThreadData* thread_data, std::vector<std::string> server_keys, ServerListQuery* query_response, const sServerListReq* req, int peer_version);
	void AppendServerEntries_AllKeys(TaskThreadData* thread_data, std::vector<std::string> server_keys, ServerListQuery* query_response, const sServerListReq* req);
	bool LoadBasicServerInfo(MM::Server* server, redisReply* basic_keys_response);
	void LoadCustomServerInfo(MM::Server* server, std::vector<std::string>& basic_lookup_keys, redisReply* basic_keys_response);
	void LoadCustomServerInfo_AllKeys(MM::Server* server, redisReply* custom_keys_response);
	void LoadCustomInfo_AllPlayerKeys(TaskThreadData* thread_data, MM::Server* server, std::string server_key);
	void LoadCustomInfo_AllTeamKeys(TaskThreadData* thread_data, MM::Server* server, std::string server_key);

	Server *GetServerByKey(TaskThreadData *thread_data, std::string key, bool include_deleted = false, bool include_player_and_team_keys = false);
	Server *GetServerByIP(TaskThreadData *thread_data, OS::Address address, OS::GameData game, bool include_deleted = false, bool include_player_and_team_keys = false);

	void GetServers(TaskThreadData *thread_data, const MMQueryRequest *request);
	void GetGroups(TaskThreadData *thread_data, const MMQueryRequest *request);

	void FreeServerListQuery(MM::ServerListQuery *query);

	extern const char *mm_channel_exchange;

	extern const char *mm_client_message_routingkey;
	extern const char *mm_server_event_routingkey;

    extern const char *mp_pk_name;

}
#endif //_MM_TASKS_H