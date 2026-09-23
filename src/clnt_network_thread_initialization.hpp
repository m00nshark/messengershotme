#pragma once

#include "clnt_globs.hpp"
#include "logger.hpp"
#include <SFML/Network.hpp>
#include <fstream>

inline std::string read_from_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        logger::fatal("[read_from_file] {} cannot be opened.", filename);
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

/// tls_status should be 'establishing' and status should be connected before entering the function;
/// reacts to glob_is_tls_verified
/// returns:
/// - true with statuses tls:ok
/// - false with statuses (tls:error && connection:tls_failed) || (tls:offline && connection:failed)
inline bool initialize_tls(sf::TcpSocket* socket, sf::IpAddress server_addr) {
    socket->setBlocking(false);
    std::string cert_crt = "";
    if(glob_is_requiring_tls_verified) {
        cert_crt = read_from_file("server2.crt");
    }
    short timeout = 0, max_timeout = 10;
/**/while (tls_status == tls_status_class::establishing && status == connection_status::connected){
        sf::TcpSocket::TlsStatus local_tls_status;
        if(glob_is_requiring_tls_verified) {
            local_tls_status = socket->setupTlsClient(server_addr.toString(), cert_crt);
        } else {
            local_tls_status = socket->setupTlsClient(server_addr.toString(), false);
        }
/*swcs*/switch(local_tls_status) {
            case sf::TcpSocket::TlsStatus::HandshakeComplete:
                tls_status = tls_status_class::ok;
                logger::info("[TI] TLS connection established successfully");
                socket->setBlocking(true);
                break;
            case sf::TcpSocket::TlsStatus::Error:
                socket->disconnect();
                status = connection_status::tls_failed;
                tls_status = tls_status_class::error;
                logger::error("[TI] TLS connection failed to establish, disconnecting for prompting the user.");
                socket->setBlocking(true);
                return false;
            case sf::TcpSocket::TlsStatus::NotConnected:
                socket->disconnect();
                status = connection_status::failed;
                tls_status = tls_status_class::offline;
                logger::error("[TI] unknown error during establishing TLS connection, disconnecting.");
                socket->setBlocking(true);
                return false;
            case sf::TcpSocket::TlsStatus::HandshakeStarted:
                logger::info("[TI] trying to establish TLS handshake");
                timeout++;
                if(timeout > max_timeout) {
                    socket->disconnect();
                    status = connection_status::failed;
                    tls_status = tls_status_class::offline;
                    logger::error("[TI] TLS handshake timeout. bailing out");
                    socket->setBlocking(true);
                    return false;
                }
                sf::sleep(sf::seconds(0.9));
                break;
/*swcs*/}
/**/}
    socket->setBlocking(true);
    return true;
}

inline void initialize_socket(sf::TcpSocket* socket, std::string server_addr_string, std::string socket_port_string) {
// --- begin
    logger::info("[SI] entering socket initialization");
/**/logger::info("[SI] reading ip address from char massive, {}", server_addr_string);
    auto server_addr = sf::IpAddress::fromString(server_addr_string);
    if(!server_addr.has_value()) {
        logger::error("[SI] failed to get ip address from string");
        status = connection_status::ip_translation_failed;
        return;
    } else {
        logger::info("[SI] ip address read successful, {}", server_addr->toString());
    }
/**/logger::info("[SI] reading port from char massive, {}", socket_port_string);
    unsigned short socket_port = 0;
    {
        const char* socket_port_data = socket_port_string.data(); 
        const char* socket_port_data_end = socket_port_data + socket_port_string.size(); 

        auto [ptr, ec] = std::from_chars(socket_port_data, socket_port_data_end , socket_port);
        if (ec != std::errc()) {
            if (ec == std::errc::invalid_argument) {
                logger::error("[SI] port read returned 'invalid argument'");
            } else if (ec == std::errc::result_out_of_range) {
                logger::error("[SI] port read returned 'out of range'");
            }
            status = connection_status::port_translation_failed;
            return;
        }
        logger::info("[SI] port read successful, {}", socket_port);
    }
// --- connection init
    logger::info("[SI] attempting connection to remote server");
    switch(socket->connect(server_addr.value(), socket_port, sf::seconds(10))) {
        case sf::TcpSocket::Status::Done:
            logger::info("[SI] socket connected! trying to set up TLS connection");
            status = connection_status::connected;
            break;
        case sf::TcpSocket::Status::Error:
            logger::error("[SI] socket returned with status: error");
            status = connection_status::failed;
            return;
        case sf::TcpSocket::Status::Disconnected:
            logger::error("[SI] socket instantly disconnected!");
            status = connection_status::failed;
            return;
        default:
            logger::error("[SI] socket returned partial status, which is unexpected. disconnecting, as behaviour for this case is not programmed.");
            socket->disconnect();
            status = connection_status::failed;
            return;
    }
// --- TLS stage
    tls_status = tls_status_class::establishing;
    initialize_tls(socket, server_addr.value());
    if(status == connection_status::failed){
        logger::error("[SI] connection interrupted during TLS initialization stage. quitting.");
        return;
    }
    if(tls_status == tls_status_class::ok) {
        logger::info("[SI] gone through proper TLS initialization");
    // reading TLS chiphersuite name
        {
            std::lock_guard<std::mutex> lock(glob_data_mutex);
            if(socket->getCurrentCiphersuiteName() != std::nullopt) {
                glob_chipher_suite_name = socket->getCurrentCiphersuiteName()->c_str();
            } else {
                glob_chipher_suite_name = "[ERROR] NULLOPT";
            }
        }
        return;
/**/}// down here is comically large code of handling the unsecure TLS connection scenario
    if(!(tls_status == tls_status_class::error && status == connection_status::tls_failed)) {
        socket->disconnect();
        status = connection_status::failed;
        tls_status = tls_status_class::offline;
        glob_is_requiring_tls_verified = true;
        glob_chipher_suite_name = "INIT";
        logger::fatal("[SI] (1) TRIGGERRED A BUG INSIDE SOCKET INITIALIZATION DURING TLS STAGE. BAILING OUT.");
        return;
    }
    while(tls_status == tls_status_class::error && glob_is_requiring_tls_verified && status == connection_status::tls_failed){
        sf::sleep(sf::milliseconds(1000));
    }

    if(status != connection_status::connecting) {
        socket->disconnect();
        logger::warn("[SI] recieved status differing from 'connecting'. gotcha, bailing out.");
        return;
    }

    if(socket->isBlocking() == false) {
        socket->setBlocking(true);
        logger::warn("[SI] socket wasn't blocking. bug in tls initialization, aka [TI] code.");
    }
    logger::info("[SI] is attempting connection to server again.");
    if(socket->connect(server_addr.value(), socket_port, sf::seconds(10)) != sf::Socket::Status::Done) {
        socket->disconnect();
        status = connection_status::failed;
        tls_status = tls_status_class::offline;
        glob_is_requiring_tls_verified = true;
        glob_chipher_suite_name = "INIT";
        logger::error("[SI] error re-connecting to server for proceeding with unsecure connection");
        return;
    } else {
        status = connection_status::connected;
    }
    logger::info("[SI] connected to server again. trying to establish TLS connection.");
    
    tls_status = tls_status_class::establishing;
    initialize_tls(socket, server_addr.value());
    if(status == connection_status::failed){
        logger::error("[SI] connection interrupted during TLS initialization stage. quitting.");
        return;
    }

    if(!(tls_status == tls_status_class::ok && status == connection_status::connected)) {
        socket->disconnect();
        status = connection_status::failed;
        tls_status = tls_status_class::offline;
        glob_is_requiring_tls_verified = true;
        glob_chipher_suite_name = "INIT";
        logger::fatal("[SI] (2) TRIGGERRED A BUG INSIDE SOCKET INITIALIZATION DURING TLS STAGE. BAILING OUT.");
        return;
    }

    logger::info("[SI] gone through unsecure TLS initialization.");
// reading TLS chiphersuite name
    {
        std::lock_guard<std::mutex> lock(glob_data_mutex);
        if(socket->getCurrentCiphersuiteName() != std::nullopt) {
            glob_chipher_suite_name = socket->getCurrentCiphersuiteName()->c_str();
            glob_chipher_suite_name = "[UNSECURE] " + glob_chipher_suite_name; 
        } else {
            glob_chipher_suite_name = "[ERROR] NULLOPT";
            glob_chipher_suite_name = "[UNSECURE] " + glob_chipher_suite_name; 
        }
    }

/**/logger::warn("[SI] socket initialization routine completed without verification.");
    return;
}