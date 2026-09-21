#pragma once

#include "clnt_globs.hpp"
#include "clnt_network_thread_initialization.hpp"
#include "logger.hpp"
#include <SFML/Network.hpp>

inline void handle_socket(sf::TcpSocket* socket) {
    logger::info("[SH] entering handle_socket");
    socket->setBlocking(false);
    sf::Packet packet;

    sf::Clock ping_clock;

    while(status == connection_status::connected) {
        switch(socket->receive(packet)) {
            case sf::TcpSocket::Status::Disconnected:
                status = connection_status::failed;
                logger::warn("[SH] socket disconnected, status set to failed.");
                break;
            case sf::TcpSocket::Status::Done:
                // *** //
                packet.clear();
                break;
            case sf::TcpSocket::Status::Error:
                status = connection_status::failed;
                logger::error("[SH] socket experienced fatal error, status set to failed.");
                break;
            case sf::TcpSocket::Status::NotReady:
                break;
            case sf::TcpSocket::Status::Partial:
                break;
        }

        sf::sleep(sf::milliseconds(5));
    }
    socket->disconnect();
    socket->setBlocking(true);
    logger::info("[SH] quitting handle_socket");
}

inline void network_loop() {
    logger::info("---[NL] entering network loop---");
    sf::TcpSocket socket;
    std::string ip;
    std::string port;

    while(!general_quit){
        switch(status) {
            case connection_status::standby:
                tls_status = tls_status_class::ok;
                socket.setBlocking(true);
                sf::sleep(sf::milliseconds(20));
                break;
            case connection_status::connecting:
                logger::info("[NL] starting the connection. this log entry should be seen once for connection attempt.");
                sf::sleep(sf::seconds(0.3));
                {
                    std::lock_guard<std::mutex> lock(glob_data_mutex);
                    ip = glob_server_addr_string;
                    port = glob_socket_port_string;
                }
                initialize_socket(&socket, ip, port);
                ip = port = "0";
                if(status != connection_status::connecting) logger::info("[NL] confirming the completion of socket initialization routine.");
                else { status = connection_status::abort; logger::info("[NL] something went wrong during socket initialization routine and global status haven't changed from 'connecting'. aborting connection.");}
                break;
            case connection_status::abort:
                tls_status = tls_status_class::offline;
                glob_is_requiring_tls_verified = true;
                {
                    std::lock_guard<std::mutex> lock(glob_data_mutex);
                    glob_chipher_suite_name = "INIT";
                }
                socket.disconnect();
                status = connection_status::standby;
                break;
            case connection_status::failed:
                tls_status = tls_status_class::offline;
                socket.disconnect();
                sf::sleep(sf::milliseconds(20));
                break;
            case connection_status::ip_translation_failed:
            case connection_status::port_translation_failed:
                tls_status = tls_status_class::offline;
                socket.disconnect();
                break;
            case connection_status::connected:
                handle_socket(&socket);
                break;
            case connection_status::tls_failed:
                logger::fatal("[NL] recieved status tls_failed, it means that socket initialization went severely south. aborting connection.");
                socket.disconnect();
                status = connection_status::failed;
                tls_status = tls_status_class::offline;
                glob_is_requiring_tls_verified = true;
                glob_chipher_suite_name = "INIT";
                break;
        }
    }
    logger::info("---[NL] quitting network loop---");
}