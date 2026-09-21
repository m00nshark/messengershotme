// clnt_globs.hpp
#pragma once

#include <atomic>
#include <mutex>
#include <string>


enum class connection_status {standby, ip_translation_failed, port_translation_failed, connecting, failed, tls_failed, connected, abort};
enum class tls_status_class {error, ok, offline, establishing};
inline std::atomic<connection_status> status = connection_status::standby; // connection_status
inline std::atomic<tls_status_class> tls_status = tls_status_class::offline; // tls_status_class
inline std::atomic<bool> general_quit(false); // when true - program exits
inline std::atomic<bool> glob_is_requiring_tls_verified(true); // default is true; sets to false by user, indicates user's trust in unverified TLS connection
inline std::atomic<unsigned int> glob_uid = 0; // i don't know why do i need atomic here. 

inline std::string glob_server_addr_string; // transit string between gui and network threads
inline std::string glob_socket_port_string; // transit string between gui and network threads
inline std::string glob_nickname_string; // transit string between gui and network threads
inline std::string glob_chipher_suite_name = "INIT"; // transit string between gui and network threads
inline std::mutex glob_data_mutex; // glutene