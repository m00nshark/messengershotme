#pragma once

#include <SFML/Network/Packet.hpp>
#include <cstring>
#define globs_uid_t unsigned int
#define globs_msgid_t unsigned int
#define globs_pt_cast unsigned int

namespace globs{

	const unsigned int max_message_size = 1300;
	const unsigned short max_nickname_size = 32;
	const unsigned short min_nickname_size = 5;
	const unsigned short ping_char_array_size = 5;

	enum class packet_type {ping, message, dynamic_typing, client_info, rqhist};

	// inline globs_uid_t admin_uid = 4142434445;

	/// - client_info    {type, uid, uname, act}            
	/// - dynamic_typing {type, uid, msg}
	/// - message        {type, uid, msgid, msg, act}       
	/// - ping           {type, ping}  	only sent by server, to be updated in user gui
	/// - rqhist         {type, msgid}	only sent by user (????)
	struct packet {
		packet_type type;
		char ping[ping_char_array_size];
		globs_uid_t uid;
		char uname[max_nickname_size];
		globs_msgid_t msgid;
		char msg[max_message_size];
		/// - in client_info:
		/// --- from clnt with self: 'c'onnected; TODO: request_previous_messages;
		/// --- from clnt with othr: TODO: kick ban unban;
		/// --- from srvr with self: 'u'id_is_not_allowed; TODO: kicked banned;
		/// --- from srvr with othr: 'c'onnected, 'd'isconnected;
		/// - in message:
		/// --- TODO: 'd'elete
		char act[2];
	};

	inline sf::Packet pack_into_sf_packet(globs::packet& packet) {
		sf::Packet sf_packet;
		switch(packet.type) {
			case globs::packet_type::client_info:
				sf_packet
					<< static_cast<globs_pt_cast>(packet.type)
					<< packet.uid
					<< std::string(packet.uname)
					<< std::string(packet.act);
				break;
			case globs::packet_type::dynamic_typing:
				sf_packet
					<< static_cast<globs_pt_cast>(packet.type)
					<< packet.uid
					<< std::string(packet.msg);
				break;
			case globs::packet_type::message:
				sf_packet
					<< static_cast<globs_pt_cast>(packet.type)
					<< packet.uid
					<< packet.msgid
					<< std::string(packet.msg)
					<< std::string(packet.act);
				break;
			case globs::packet_type::ping:
				sf_packet
					<< static_cast<globs_pt_cast>(packet.type)
					<< std::string(packet.ping);
				break;
			case globs::packet_type::rqhist:
				sf_packet
					<< static_cast<globs_pt_cast>(packet.type)
					<< packet.msgid;
				break;
		}
		return sf_packet;
	}

	/// returns true when successful
	/// returns false when error
	inline bool unpack_from_sf_packet(sf::Packet& tx_packet, packet* writeable_packet_ptr) {
		packet intermediate_packet;
		globs_pt_cast shit;
		if(tx_packet >> shit) {
			intermediate_packet.type = static_cast<packet_type>(shit);
			switch (intermediate_packet.type) {
				case globs::packet_type::client_info:
				case globs::packet_type::dynamic_typing:
				case globs::packet_type::message:
				case globs::packet_type::ping:
				case globs::packet_type::rqhist:
					break;
			}
		} else {
			return false;
		};
		std::memcpy(writeable_packet_ptr, &intermediate_packet, sizeof(intermediate_packet));
		return true;
	}

	/// - client_info:
	///     - from self with self: 'c'onnected;
	///     - from self with othr: 'k'ick, 'b'an 'u'nban, 'r'equest_hist;
	///     - from srvr with othr: 'd'isconnected, 'k'icked, 'b'anned, 'u'nbanned, 'r'equesting_hist;
	///     -
	/// - message (from other): 'd'elete, 
}