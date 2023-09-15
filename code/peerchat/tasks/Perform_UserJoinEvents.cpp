#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {
	bool Perform_UserJoinEvents(PeerchatBackendRequest request, TaskThreadData* thread_data) {
        TaskResponse response;

        response.channel_summary = LookupChannelById(thread_data, request.channel_summary.channel_id);

        Peer::OnTopic_FetchChannelInfo(response, request.peer);
        Peer::OnNames_FetchChannelInfo(response, request.peer);

        int initial_flags = request.channel_modify.set_mode_flags;
        if(request.peer->GetOperFlags() & OPERPRIVS_GLOBALOWNER) {
            initial_flags |= EUserChannelFlag_Owner;
        }
        if(request.peer->GetOperFlags() & OPERPRIVS_GETVOICE) {
            initial_flags |= EUserChannelFlag_Voice;
        }
        int requiredOperFlags = 0;
        if (initial_flags & EUserChannelFlag_Invisible) {
            std::ostringstream mq_message;
            std::string message = "INVISIBLE USER " + request.peer->GetUserDetails().nick + " JOINED CHANNEL";
            const char* base64 = OS::BinToBase64Str((uint8_t*)message.c_str(), message.length());
            std::string b64_string = base64;
            free((void*)base64);

            requiredOperFlags = OPERPRIVS_INVISIBLE;

            mq_message << "\\type\\NOTICE\\toChannelId\\" << request.channel_summary.channel_id << "\\message\\" << b64_string << "\\fromUserId\\-1\\requiredOperFlags\\" << requiredOperFlags << "\\includeSelf\\1";
            thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, mq_message.str().c_str());
        }
        if (initial_flags & EUserChannelFlag_Voice) {
            std::string message = "+v :" + request.peer->GetUserDetails().nick;
            const char* base64 = OS::BinToBase64Str((uint8_t*)message.c_str(), message.length());
            std::string b64_string = base64;
            free((void*)base64);

            std::ostringstream mode_message;
            mode_message << "\\type\\MODE\\toChannelId\\" << request.channel_summary.channel_id << "\\message\\" << b64_string << "\\fromUserId\\-1\\includeSelf\\1\\requiredOperFlags\\" << requiredOperFlags;
            thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, mode_message.str().c_str());
        }
        if (initial_flags & EUserChannelFlag_HalfOp || initial_flags & EUserChannelFlag_Op || initial_flags & EUserChannelFlag_Owner) {
            std::string message = "+o :" + request.peer->GetUserDetails().nick;
            const char* base64 = OS::BinToBase64Str((uint8_t*)message.c_str(), message.length());
            std::string b64_string = base64;
            free((void*)base64);

            std::ostringstream mode_message;
            mode_message << "\\type\\MODE\\toChannelId\\" << request.channel_summary.channel_id << "\\message\\" << b64_string << "\\fromUserId\\-1\\includeSelf\\1\\requiredOperFlags\\" << requiredOperFlags;
            thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, mode_message.str().c_str());
        }

        ChannelSummary channel =  LookupChannelById(thread_data, request.channel_summary.channel_id);
		if(channel.entrymsg.length() > 0) {
            std::ostringstream entry_message;

			const char *base64 = OS::BinToBase64Str((uint8_t *)channel.entrymsg.c_str(), channel.entrymsg.length());
			std::string b64_string = base64;
			free((void *)base64);


			entry_message << "\\type\\NOTICE\\toChannelId\\" << channel.channel_id << "\\fromUserSummary\\" << server_userSummary->ToBase64String(true) << "\\includeSelf\\1\\message\\" << b64_string << "\\onlyVisibleTo\\" << request.peer->GetBackendId();
			thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, entry_message.str().c_str());
		}

        int current_modeflags = request.peer->GetChannelFlags(request.channel_summary.channel_id);
        int old_modeflags = current_modeflags;
        current_modeflags |= initial_flags;

        if(current_modeflags != old_modeflags) {
            void *reply = redisCommand(thread_data->mp_redis_connection, "HSET channel_%d_user_%d modeflags %d", request.channel_summary.channel_id, request.peer->GetBackendId(), current_modeflags);
            freeReplyObject(reply);
            
            request.peer->SetChannelFlags(request.channel_summary.channel_id, current_modeflags);
            SendUpdateUserChanModeflags(thread_data, request.channel_summary.channel_id, request.peer->GetBackendId(), current_modeflags, old_modeflags);
        }


		if (request.callback)
			request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
		return true;
	}
}