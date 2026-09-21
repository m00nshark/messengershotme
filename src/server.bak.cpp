#include "globs.hpp"
#include "logger.hpp"
#include <SFML/Network.hpp>
#include <cstring>
#include <fstream>

sf::Time selector_timeout = sf::milliseconds(100);
sf::Time handshake_timeout = sf::seconds(2.5);

struct uprofile {
	globs_uid_t uid;
	char uname[globs::max_nickname_size];
	bool is_online = false;
};
struct usession {
	std::unique_ptr<sf::TcpSocket> socket;
	globs_uid_t bound_uid;
	bool is_allowed = false;
	bool is_rejected = false;
	usession(std::unique_ptr<sf::TcpSocket> insocket): socket(std::move(insocket)), bound_uid(0) {};
};

std::string read_from_file(const std::string& filename) {
	std::ifstream file(filename);
	if (!file.is_open()) {
		logger::fatal("file {} cannot be opened.", filename);
		return "";
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

/// -2 for no occurances; +X for one occurance; -1 for server has too much repeating unames;
int where_uname_repeating(char uname[globs::max_nickname_size], std::vector<uprofile> profiles) {
	int bong = 0;
	int where_in_vec = -2;
	for(size_t i = 0; i < profiles.size(); i++) {
		if(std::strcmp(uname, profiles[i].uname) == 0) {
			bong++;
			where_in_vec = i;
		}
	}
	if(bong > 1) {
		logger::error("[I_UNAME_R] found {} repeating unames '{}'. this is a bug inside a server code.", bong, uname);
		where_in_vec = -1;
	}
	return where_in_vec;
}

/// -2 for no occurances; +X for one occurance; -1 for server has too much repeating unames;
int where_uid_repeating(globs_uid_t uid, std::vector<uprofile> profiles) {
	int bong = 0;
	int where_in_vec = -2;
	for(size_t i = 0; i < profiles.size(); i++) {
		if(uid == profiles[i].uid) {
			bong++;
			where_in_vec = i;
		}
	}
	if(bong > 1) {
		logger::error("[I_UID_R] found {} repeating uids '{}'. this is a bug inside a server code.", bong, uid);
		where_in_vec = -1;
	}
	return where_in_vec;
}

int main() {
#ifdef _WIN32
	std::system("chcp 65001 > nul");
#endif
	logger::info("--- starting the server ---");
	unsigned short listener_port = 8080;
	sf::Time timeout = sf::milliseconds(100);

	std::string cert_crt = read_from_file("server.crt"), cert_key = read_from_file("server.key");
	if(cert_crt.empty() || cert_key.empty()) {
		logger::fatal("certificate and key files are empty or not existing. bailing out.");
		return -1;
	} 

	sf::TcpListener listener;
	switch(listener.listen(listener_port)) {
		case sf::Socket::Status::Done:
			logger::info("listening on port {}", listener_port);
			break;
		default:
			logger::fatal("listener failed to start listening on port {}, bailing out", listener_port);
			listener.close();
			return -1;
	}

	std::vector<uprofile> profiles;
	std::vector<std::unique_ptr<usession>> sessions;
	sf::Clock server_clock;
	sf::SocketSelector selector;
	selector.add(listener);

// start main while
	while(true) {
// server ping
		if(!selector.wait(selector_timeout)){
		} 

// client inbound
		if(selector.isReady(listener)) {
			std::unique_ptr<sf::TcpSocket> new_client = std::make_unique<sf::TcpSocket>();
			if(listener.accept(*new_client) == sf::Socket::Status::Done) {
				if(new_client->getRemoteAddress().has_value()){
					logger::info("new client from {}", new_client->getRemoteAddress()->toString());
				} else {
					logger::warn("new client, can't get it's ip address. rejecting it.");
					new_client->disconnect();
					continue;
				}
				{
					bool timeout = false;
					bool is_new_client_rejected = false;
					sf::Clock timeout_clock;
					while(!timeout) {
						auto ssttaattuuss = new_client->setupTlsServer(cert_crt, cert_key);
						if(ssttaattuuss == sf::TcpSocket::TlsStatus::HandshakeComplete) {
							logger::info("new client established TLS handshake successfully");
							timeout = true;
						} else if (ssttaattuuss == sf::TcpSocket::TlsStatus::Error || ssttaattuuss == sf::TcpSocket::TlsStatus::NotConnected) {
							logger::warn("new client failed to establish TLS handshake, rejecting it.");
							new_client->disconnect();
							timeout = true;
							is_new_client_rejected = true;
						} else if(timeout_clock.getElapsedTime() > handshake_timeout) {
							timeout = true; 
						} else {
							sf::sleep(sf::milliseconds(10));
						}
					}
					if(is_new_client_rejected) continue;
				}
				selector.add(*new_client);
				sessions.push_back(std::make_unique<usession>(std::move(new_client)));
				logger::info("added new client to sessions list. waiting for it's id and nickname.");
			}
		}

// client handling
		auto sesh_it = sessions.begin();
		while(sesh_it != sessions.end()) {
			usession& sesh = **sesh_it;
			sf::TcpSocket& sesh_sock = *(sesh.socket);
			if(selector.isReady(sesh_sock)) {
				if(sesh.is_rejected){ // rejected client --------------------------------------------
					sesh_it++;
					continue;
				} else if(!sesh.is_allowed) { // new client -----------------------------------------
					sf::Packet clnt_info_packet;
					auto clnt_info_packet_status = sesh_sock.receive(clnt_info_packet);
					if(clnt_info_packet_status == sf::Socket::Status::Done) {
						globs_pt_cast packet_type;
						globs_uid_t uid;
						char uname[globs::max_nickname_size];
						char act[2];
						if(!(clnt_info_packet >> packet_type)) {
							logger::warn("newbound client sent malformed packet. looking carefully.");
						}
						if(static_cast<globs::packet_type>(packet_type) == globs::packet_type::client_info) {
							logger::info("recieved client info, reading...");
							if(clnt_info_packet >> uid >> uname >> act) {
								if(strcmp(act, "c") != 0) {
									sesh_sock.disconnect();
									selector.remove(sesh_sock);
									sesh_it = sessions.erase(sesh_it);
									logger::info("client info has unexpected act directive. disconnecting it.");
									continue;
								} else {
									int wuir = where_uid_repeating(uid, profiles);
									int wunr = where_uname_repeating(uname, profiles);
									if (wuir == wunr) { // recognized the client!
										if(profiles[wuir].is_online == false) {
											profiles[wuir].is_online = true;
											sesh.bound_uid = uid;
											sesh.is_allowed = true;
											sesh.is_rejected = false;
											logger::info("welcome back, {}&{}!", uname, uid);
										} else {
											sesh.is_rejected = true;
											sesh.is_allowed = false;
											logger::warn("newbound client used same credentials as other client that is online and allowed.");
										}
									} else if (wuir != wunr) { // uid/uname mismatch
										sesh.is_rejected = true;
										sesh.is_allowed = false;
										logger::warn("newbound client mismatched known uid and uname.");
									} else if (wuir == -1 || wunr == -1) { // internal server error: too much registered clients with same uname/uid

									} else if (wuir == -2 && wunr == -2) { // registering new client 
										if(uid == 0) {
											sesh_sock.disconnect();
											selector.remove(sesh_sock);
											sesh_it = sessions.erase(sesh_it);
											logger::info("can't accept new client without UID");
										} else {
											profiles.push_back(uprofile());
											sesh.is_allowed = true;
											sesh.is_rejected = false;
											logger::info("new client is welcome here! hello, {}&{}!", uname, uid);
										}
									} else { // unknown error, bug
										sesh_sock.disconnect();
										selector.remove(sesh_sock);
										sesh_it = sessions.erase(sesh_it);
										logger::fatal("unknown error during acception stage of new client\nit's uid: {}\n it's uname:{}\n\nknowns:\nwuir&uid:{}&{}\nwunr&uname:{}&{}",
											uid,uname, wuir,profiles[wuir].uid, wunr,profiles[wunr].uname);
									}
								}
							} else {
								sesh_sock.disconnect();
								selector.remove(sesh_sock);
								sesh_it = sessions.erase(sesh_it);
								logger::info("client info has unexpected act directive. disconnecting it.");
								continue;
							}
						} else {
							
						}
					} else if(clnt_info_packet_status == sf::Socket::Status::Disconnected) {
						
					} else if(clnt_info_packet_status == sf::Socket::Status::Error) {

					}

					sesh_it++;
				} /* else if(!sesh.is_allowed) */ else { // active client -------------------------------------------

				}
			} else { sesh_it++; }
		} // while(sesh_it != sesions.end())
	}
// end main while

	logger::info("purpose of this program is now complete.");
	logger::info("---this should be the last log. goodbye.---");
}