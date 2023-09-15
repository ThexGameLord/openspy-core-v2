#include <OS/OpenSpy.h>
#include <OS/gamespy/gsmsalg.h>
#include <sstream>
#include "SBDriver.h"
#include "SBPeer.h"
#include "V1Peer.h"
#include "enctype1_helper.h"

#include <sstream>
#include <vector>
#include <algorithm>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <OS/Net/NetServer.h>

#include <filter/CToken.h>

namespace SB {
		V1Peer::V1Peer(Driver *driver, INetIOSocket *sd) : SB::Peer(driver, sd, 1) {
			memset(&m_challenge,0,sizeof(m_challenge));
			OS::gen_random((char *)&m_challenge,6);

			m_enctype = 0;
			m_waiting_gamedata = 0;
			m_validated = false;

			m_sent_validation = false;
			m_sent_crypt_key = false;

		}
		V1Peer::~V1Peer() {

		}
		void V1Peer::Delete(bool timeout) {
			if(m_enctype == 1) {
				Enctype1_FlushPackets();
			}
			Peer::Delete(timeout);
		}
		void V1Peer::OnConnectionReady() {
			Peer::OnConnectionReady();
			if(!m_sent_validation) {
				send_validation();
			}
		}
		void V1Peer::send_validation() {
			std::ostringstream s;
			s << "\\basic\\\\secure\\" << m_challenge;
			m_sent_validation = true;
			SendPacket((const uint8_t*)s.str().c_str(),s.str().length(), false);
		}
		void V1Peer::send_ping() {
			//check for timeout
			struct timeval current_time;
			gettimeofday(&current_time, NULL);
			if(current_time.tv_sec - m_last_ping.tv_sec > SB_PING_TIME) {
				gettimeofday(&m_last_ping, NULL);
				//SendPacket((uint8_t *)&buff, len, true);
			}
		}
		void V1Peer::think(bool packet_waiting) {
			NetIOCommResp io_resp;
			if (m_delete_flag) return;
			if (m_waiting_gamedata == 2) {
				m_waiting_gamedata = 0;
                flushWaitingPackets();
			}
			if (packet_waiting) {
				OS::Buffer recv_buffer;
				io_resp = this->GetDriver()->getNetIOInterface()->streamRecv(m_sd, recv_buffer);

				int len = io_resp.comm_len;
				if (len <= 0) {
					goto end;
				}
		
				/*
				This scans the incoming packets for \\final\\ and splits based on that,


				as well as handles incomplete packets -- due to TCP preserving data order, this is possible, and cannot be used on UDP protocols
				*/
				std::string recv_buf = m_kv_accumulator;
				m_kv_accumulator.clear();
				recv_buf.append((const char *)recv_buffer.GetHead(), len);
				size_t final_pos = 0, last_pos = 0;
				do {
					final_pos = recv_buf.find("\\final\\", last_pos);
					std::string partial_string;
					if (final_pos == std::string::npos) {
						partial_string = recv_buf.substr(last_pos);
					} else {						
						partial_string = recv_buf.substr(last_pos, final_pos - last_pos);
						last_pos = final_pos + 7; // 7 = strlen of \\final
					}
					partial_string = skip_queryid(partial_string);

					if(partial_string.length() == 0) break;
					handle_packet(partial_string);
				} while (final_pos != std::string::npos);


				//check for extra data that didn't have the final string -- incase of incomplete data
				if (last_pos < (size_t)len) {
					std::string remaining_str = recv_buf.substr(last_pos);

					if (remaining_str.length() > 0) {
						m_kv_accumulator.append(remaining_str);
					}
				}

				if (m_kv_accumulator.length() > MAX_UNPROCESSED_DATA) {
					Delete();
					return;
				}

				
			}

			end:
			send_ping();

			//check for timeout
			struct timeval current_time;
			gettimeofday(&current_time, NULL);
			if(current_time.tv_sec - m_last_recv.tv_sec > SB_PING_TIME*2) {
				Delete(true);
			} else if ((io_resp.disconnect_flag || io_resp.error_flag) && packet_waiting) {
				Delete();
			}
		}

		void V1Peer::informDeleteServers(MM::Server *server) {

		}
		void V1Peer::informNewServers(MM::Server *server) {

		}
		void V1Peer::informUpdateServers(MM::Server *server) {

		}
		void V1Peer::send_error(bool disconnect, const char *fmt, ...) {
			char str[512];
			std::ostringstream resp;
			va_list args;
			va_start(args, fmt);
			int len = vsnprintf(str, sizeof(str), fmt, args);
			str[len] = 0;
			va_end(args);
			if(disconnect) {
				resp << "\\fatal\\1";
			}
			resp << "\\error\\" << str;

			OS::LogText(OS::ELogLevel_Info, "[%s] Got Error %s", getAddress().ToString().c_str(), str);

			if (disconnect) {
				Delete();
			}
		}
		void V1Peer::handle_gamename(std::string data) {
			OS::KVReader data_parser = OS::KVReader(data);
			MM::MMQueryRequest req;

			std::string validation;

			int enctype = data_parser.GetValueInt("enctype");
			std::string gamename = data_parser.GetValue("gamename");

		
			validation = data_parser.GetValue("validate");
			m_validation = validation;

			req.type = MM::EMMQueryRequestType_GetGameInfoByGameName;
			m_waiting_gamedata = 1;
			req.gamenames[0] = gamename;
			m_enctype = enctype;
			AddRequest(req);
		}
		void V1Peer::handle_packet(std::string data) {
			if (m_waiting_gamedata == 1) {
				m_waiting_packets.push(data);
				return;
			}
			OS::LogText(OS::ELogLevel_Info, "[%s]: Handle %s", getAddress().ToString().c_str(), data.c_str());

			data = skip_queryid(data);

			if(m_validated == false) {
					//skip invalid \basic\ data for jbnightfire server browser
					if(strncmp(data.c_str(),"\\basic\\", 7) == 0) {
							data = data.substr(6);
					}
			}


			OS::KVReader kv_parser = OS::KVReader(data);

			if (kv_parser.Size() < 1) {
				Delete();
				return;
			}

			std::string command;
			gettimeofday(&m_last_recv, NULL);

			command = kv_parser.GetKeyByIdx(0);
			if(strcmp(command.c_str(),"gamename") == 0 && !m_validated) {
				handle_gamename(data);
			}
			else if (strcmp(command.c_str(), "list") == 0) {
				handle_list(data);
			} else if(strcmp(command.c_str(), "queryid") == 0) {
				OS::LogText(OS::ELogLevel_Info, "[%s] Ignore queryid %s", getAddress().ToString().c_str(), data.c_str());
			}
			else {
				//send_error(true, "Cannot handle request");
				OS::LogText(OS::ELogLevel_Info, "[%s] Got Unknown request %s", getAddress().ToString().c_str(), command.c_str());
			}
		}
		void V1Peer::OnRetrievedServerInfo(const MM::MMQueryRequest request, MM::ServerListQuery results, void *extra) {
			SendServerInfo(results);
            //if(results.last_set) {
            //   Delete();
            //}
		}
		void V1Peer::OnRetrievedServers(const MM::MMQueryRequest request, MM::ServerListQuery results, void *extra) {
			if (request.req.all_keys) {
				SendServerInfo(results);
			}
            else {
                SendServers(results);
            }
            if(results.last_set) {
                Delete();
            }
		}
		void V1Peer::OnRetrievedGroups(const MM::MMQueryRequest request, MM::ServerListQuery results, void *extra) {
			SendGroups(results);
            
            if(results.last_set) {
                Delete();
            }
		}
		void V1Peer::OnRecievedGameInfo(const OS::GameData game_data, void *extra) {
			m_waiting_gamedata = 2;
            char realvalidate[16];
            if (game_data.secretkey[0] == 0) {
                send_error(true, "Invalid source gamename");
                return;
            }
            m_game = game_data;
            gsseckey((unsigned char *)&realvalidate, (const char *)&m_challenge, (const unsigned char *)m_game.secretkey.c_str(), m_enctype);
            if(!m_validated) {
                if(strcmp(realvalidate,m_validation.c_str()) == 0) {
                    m_validated = true;
                } else {
                    send_error(true, "Validation error");
                    return;
                }
            }
			FlushPendingRequests();
            flushWaitingPackets();
		}
		void V1Peer::handle_list(std::string data) {
			std::string mode, gamename;
			OS::KVReader kv_parser(data);

			if (m_game.secretkey[0] == 0) {
				send_error(true, "No source gamename registered");
				return;
			}

			if(!kv_parser.HasKey("list") || !kv_parser.HasKey("gamename")) {
				send_error(true, "Data parsing error");
				return;
			} else {
				mode = kv_parser.GetValue("list");
				gamename = kv_parser.GetValue("gamename");
			}

			MM::MMQueryRequest req;
			MM::ServerListQuery servers;
			req.gamenames[0] = gamename;
			req.req.m_for_gamename = gamename;
			req.req.m_from_game = m_game;
			req.req.all_keys = false;
			req.type = MM::EMMQueryRequestType_GetGameInfoByGameName;
			m_waiting_gamedata = 1;
			req.req.send_groups = false;
			req.req.send_compressed = true;
			if(mode.compare("cmp") == 0) {				
				req.req.all_keys = false;
			} else if(mode.compare("info2") == 0) {
				req.req.all_keys = true;
			} else if(mode.compare("groups") == 0) {
				req.req.send_groups = true;
				req.req.all_keys = true;
			} else {
				req.req.all_keys = false;
				req.req.send_compressed = false;
				//send_error(true, "Unknown list mode");
				//return;
			}

			if(kv_parser.HasKey("where")) {
				req.req.filter = kv_parser.GetValue("where");
			}

			OS::LogText(OS::ELogLevel_Info, "[%s] List Request: gamenames: (%s) - (%s), filter: %s  is_group: %d, all_keys: %d", getAddress().ToString().c_str(), req.req.m_from_game.gamename.c_str(), req.req.m_for_gamename.c_str(), req.req.filter.c_str(), req.req.send_groups, req.req.all_keys);

            req.req.m_for_game.gamename = gamename;
            req.type = req.req.send_groups ? MM::EMMQueryRequestType_GetGroups : MM::EMMQueryRequestType_GetServers;
			
			m_last_list_req = req.req;

			m_last_list_req_token_list = CToken::filterToTokenList(m_last_list_req.filter.c_str());

			AddRequest(req);
			// //server disconnects after this
		}
		void V1Peer::SendServerInfo(MM::ServerListQuery results) {
			std::ostringstream resp;
			size_t field_count;

			std::vector<std::string>::iterator it;
			if(results.first_set) {
				results.captured_basic_fields.insert(results.captured_basic_fields.begin(), "ip"); //insert ip/port field
				field_count = results.captured_basic_fields.size(); // + results.captured_player_fields.size() + results.captured_team_fields.size();
				it = results.captured_basic_fields.begin();
				resp << "\\fieldcount\\" << field_count;
				while(it != results.captured_basic_fields.end()) {
					resp << "\\" << *it;
					it++;
				}
				SendPacket((const uint8_t *)resp.str().c_str(), resp.str().length(), false);
				resp.str("");
			}

			std::vector<MM::Server *>::iterator it2 = results.list.begin();
			while(it2 != results.list.end()) {
				MM::Server *serv = *it2;

				it = results.captured_basic_fields.begin();

				resp << "\\" << serv->wan_address.ToString(true) << ":" << serv->wan_address.port; //add ip/port
				while(it != results.captured_basic_fields.end()) {
					std::string field = *it;
					resp << "\\" << field_cleanup(serv->kvFields[field]);
					it++;
				}
				it2++;
			}
			SendPacket((const uint8_t *)resp.str().c_str(), resp.str().length(), results.last_set);

			//if(results.last_set)
				//m_delete_flag = true;
		}
		void V1Peer::SendGroups(MM::ServerListQuery results) {
			OS::Buffer buffer;

			std::ostringstream resp;
			std::vector<std::string> group_fields;
			size_t fieldcount;

			group_fields.push_back("groupid");
			group_fields.push_back("hostname");
			group_fields.push_back("numplayers");
			group_fields.push_back("maxwaiting");
			group_fields.push_back("numwaiting");
			group_fields.push_back("numservers");
			group_fields.push_back("password");
			group_fields.push_back("other");
						
			fieldcount = group_fields.size();

			if(results.first_set) {
				resp << "\\fieldcount\\" << fieldcount;
				std::vector<std::string>::iterator it = group_fields.begin();
				while(it != group_fields.end()) {
					resp << "\\" << *it;
					it++;
				}

				buffer.WriteBuffer((void *)resp.str().c_str(), resp.str().length());
				SendPacket((const uint8_t *)buffer.GetHead(), buffer.bytesWritten(), false);
				buffer.resetCursors();
			}

			std::vector<MM::Server *>::iterator it2 = results.list.begin();
			while(it2 != results.list.end()) {
				MM::Server *serv = *it2;
				std::vector<std::string>::iterator it = group_fields.begin();
				while(it != group_fields.end()) {
					if((*it).compare("other") == 0) {
						//print everything that is not in group_fields from kvFields
						std::map<std::string, std::string>::iterator it3 = serv->kvFields.begin();
						std::ostringstream field_data_ss;
						std::string field_data;
						buffer.WriteByte('\\');
						while(it3 != serv->kvFields.end()) {
							std::pair<std::string, std::string> kv_pair = *it3;
							if(std::find(group_fields.begin(), group_fields.end(), kv_pair.first) == group_fields.end()) {
								buffer.WriteByte(0x01);
								buffer.WriteBuffer((void *)field_cleanup(kv_pair.first).c_str(), kv_pair.first.length());
								buffer.WriteByte(0x01);
								buffer.WriteBuffer((void*)field_cleanup(kv_pair.second).c_str(), kv_pair.second.length());
							}
							it3++;
						}
					} else {
						buffer.WriteByte('\\');
						buffer.WriteBuffer((void*)field_cleanup(serv->kvFields[*it]).c_str(), serv->kvFields[*it].length());
					}
					it++;
				}
				it2++;
			}
			SendPacket((const uint8_t *)buffer.GetHead(), buffer.bytesWritten(), results.last_set);
		}
		void V1Peer::SendServers(MM::ServerListQuery results) {
			OS::Buffer buffer;



			std::ostringstream ss;
			//std::vector<Server *> list;
			std::vector<MM::Server *>::iterator it = results.list.begin();
			while(it != results.list.end()) {
				MM::Server *serv = *it;
				if(m_last_list_req.send_compressed == true) {
					buffer.WriteInt(serv->wan_address.GetIP());
					buffer.WriteShort(serv->wan_address.GetPort());
				} else {
					ss << "\\ip\\" << serv->wan_address.ToString(true) << ":" << serv->wan_address.port;
					buffer.WriteBuffer(ss.str().c_str(), ss.str().length());
					ss.str("");
				}

				it++;
			}
			SendPacket((const uint8_t *)buffer.GetHead(), buffer.bytesWritten(), results.last_set);
			if (results.last_set) {
				Delete();
			}
		}

		void V1Peer::Enctype1_FlushPackets() {
			OS::Buffer encrypted_buffer;

			create_enctype1_buffer((const char *)&m_challenge, m_enctype1_accumulator, encrypted_buffer);

			this->GetDriver()->getNetIOInterface()->streamSend(m_sd, encrypted_buffer);
		}
		void V1Peer::SendPacket_Enctype1(OS::Buffer buffer) {
			m_enctype1_accumulator.WriteBuffer(buffer.GetHead(), buffer.bytesWritten());
		}
		void V1Peer::SendPacket(const uint8_t *buff, size_t len, bool attach_final, bool skip_encryption) {

			OS::Buffer buffer;
			OS::Buffer output_buffer;
			int skip_len = 0;
			if(m_validated && m_enctype == 2)
				send_crypt_header(m_enctype, buffer);

			skip_len += buffer.bytesWritten();
			buffer.WriteBuffer((void *)buff, len);
			if(attach_final) {
				buffer.WriteBuffer((void*)"\\final\\", 7);
			}
			if(m_game.secretkey.length() == 0)
				skip_encryption = true;
			if (!skip_encryption) {
				switch (m_enctype) {
				case 1:
					SendPacket_Enctype1(buffer);
					return;
				case 2:
					crypt_docrypt(&m_crypt_key_enctype2, (unsigned char *)buffer.GetHead() + skip_len, (int)buffer.bytesWritten() - skip_len);
					break;
				}
			}

			NetIOCommResp io_resp;
			io_resp = this->GetDriver()->getNetIOInterface()->streamSend(m_sd, buffer);
			if(io_resp.disconnect_flag || io_resp.error_flag) {
				Delete();
			}
		}
		void V1Peer::send_crypt_header(int enctype, OS::Buffer &buffer) {
			if(m_sent_crypt_key) return;
			m_sent_crypt_key = true;
			char cryptkey[13];
			size_t secretkeylen = m_game.secretkey.length();
			if(enctype == 2) {
				for(unsigned int x=0;x<sizeof(cryptkey);x++) {
					cryptkey[x] = (uint8_t)rand();
				}
				init_crypt_key((unsigned char *)&cryptkey, sizeof(cryptkey),&m_crypt_key_enctype2);
				for(size_t i=0;i< secretkeylen;i++) {
					cryptkey[i] ^= m_game.secretkey[i];
				}
				buffer.WriteByte(sizeof(cryptkey) ^ 0xEC);
				buffer.WriteBuffer((uint8_t *)&cryptkey, sizeof(cryptkey));
			}
		}
		std::string V1Peer::field_cleanup(std::string s) {
			std::string r;
			char ch;
			for(unsigned int i=0;i<s.length();i++) {
				ch = s[i];
				if(ch == '\\') ch = '.';
				r += ch;
			}
			return r;
		}

		void V1Peer::OnRecievedGameInfoPair(const OS::GameData game_data_first, const OS::GameData game_data_second, void *extra) {
		}
		std::string V1Peer::skip_queryid(std::string s) {
			if (s.substr(0, 9).compare("\\queryid\\") == 0) {
				s = s.substr(9);
				size_t queryid_offset = s.find("\\");
				if (queryid_offset != std::string::npos) {
					if (s.length() > queryid_offset) {
						queryid_offset++;
					}
					s = s.substr(queryid_offset);
				}
			}

			if (s.substr(0, 2).compare("\r\n") == 0) { //skip new lines (for unreal)... probably should rename this function
				s = s.substr(2);
			}
			return s;
		}
        void V1Peer::flushWaitingPackets() {
            while (!m_waiting_packets.empty()) {
                std::string cmd = m_waiting_packets.front();
                m_waiting_packets.pop();
                handle_packet(cmd);
            }
        }
}
