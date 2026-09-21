#include "globs.hpp"
#include "logger.hpp"
#include <SFML/Network.hpp>
#include <cstring>
#include <fstream>

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

class core {
private:
	std::string cert_crt, cert_key;
	sf::SocketSelector selector;
	sf::TcpListener listener;
	sf::Time selector_timeout = sf::milliseconds(10);
	sf::Time ping_interval = sf::seconds(2);
	std::vector<std::unique_ptr<usession>> sessions;
	std::vector<uprofile> profiles;
	sf::Clock ping_clock;

	void send_packet (sf::Packet& packet, usession* session) {
		if (session == nullptr || session->socket == nullptr) return;
		auto s = session->socket->send(packet);
		if(s == sf::Socket::Status::Disconnected || s == sf::Socket::Status::Error) {
			logger::warn("failed to send packet to UID {}", session->bound_uid);
		}
	}
	void send_packet (globs::packet& packet, usession* session) {
		sf::Packet sf_packet = globs::pack_into_sf_packet(packet);
		send_packet(sf_packet, session);
	}
	void broadcast_packet (globs::packet& packet, usession* exclude_session) {
		sf::Packet sf_packet = globs::pack_into_sf_packet(packet);
		for (auto& sesh_ptr : sessions) { // sesh_ptr.get() returns raw usession*, that lies there
			if (sesh_ptr.get() == exclude_session) {
				continue; // ignoring the requested session
			}
			if (!sesh_ptr->is_allowed) {
				continue;// denying the session that is not allowed
			}
			send_packet(sf_packet, sesh_ptr.get());
		}
	}

	void delete_socket () {

	}

public:
	core() {}
	bool init(unsigned short port) {
		cert_crt = read_from_file("server.crt");
		cert_key = read_from_file("server.key");
		if(cert_crt.empty() || cert_key.empty()) {
			logger::fatal("[CORE/INIT] certificate and key files are empty or not existing. bailing out.");
			return false;
		} 
		sf::Socket::Status init_listener_status = listener.listen(port);
		if(init_listener_status != sf::Socket::Status::Done){
			logger::fatal("[CORE/INIT] listener failed to start on port {}", port);
			return false;
		}
		logger::info("[CORE/INIT] listener is listening on port {}", port);
		selector.add(listener);
		ping_clock.restart();
		return true;
	}
	void loop() {
		// server ping
		if(!selector.wait(selector_timeout)) {
			if(ping_clock.getElapsedTime() > ping_interval){
				char ping_payload[globs::ping_char_array_size];
				globs::packet packet{globs::packet_type::ping, {}, 0, {}, 0, {}, {}};
				std::memcpy(packet.ping, ping_payload, globs::ping_char_array_size);
				return;
			}
			return;
		}
		// client inbound
		if(selector.isReady(listener)) {

		} else for (auto it = sessions.begin(); it != sessions.end(); ) {
			usession& sesh = **it;

			sf::TcpSocket& sock = *(sesh.socket);

			if (!selector.isReady(sock)) {
				it++;
				continue; 
			}

			if(!sesh.is_allowed) {
				
				continue;
			}

			sf::Packet rx_packet;
			auto rx_status = sock.receive(rx_packet);

			if(rx_status == sf::Socket::Status::Error || rx_status == sf::Socket::Status::Disconnected) {

			}

			if(rx_status == sf::Socket::Status::NotReady || rx_status == sf::Socket::Status::Partial) {
				it++;
				continue;
			}

			globs::packet rx_unpacked_packet;
			if(rx_status == sf::Socket::Status::Done && globs::unpack_from_sf_packet(rx_packet, &rx_unpacked_packet)) {
				switch(rx_unpacked_packet.type) {
					case globs::packet_type::ping:
						logger::warn("received a ping packet from UID {}, behaviour is not programmed.", sesh.bound_uid);
						break;
					case globs::packet_type::client_info:
						logger::warn("received a client_info packet from UID {}, behaviour is not programmed.", sesh.bound_uid);
						break;
					case globs::packet_type::dynamic_typing:
						logger::warn("received a dynamic_typing packet from UID {}, behaviour is not programmed.", sesh.bound_uid);
						break;
					case globs::packet_type::message:
						logger::warn("received a message packet from UID {}, behaviour is not programmed.", sesh.bound_uid);
						break;
					case globs::packet_type::rqhist:
						logger::warn("received a rqhist packet from UID {}, behaviour is not programmed.", sesh.bound_uid);
						break;
				}
			} else {
				logger::warn("received malformed packet from UID {}.", sesh.bound_uid);
			}
		}
	}
};

core server_core;
int main() {
#ifdef _WIN32
std::system("chcp 65001 > nul");
#endif
	server_core.init(8080);
	while(true) {
		server_core.loop();
	}
}