#include "clnt_network_thread.hpp"
#include "clnt_globs.hpp"
#include "logger.hpp"
#include "globs.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Network.hpp>
#include <imgui-SFML.h>
#include <imgui.h>
#include <atomic>
#include <random>
#include <thread>

const ImGuiWindowFlags imgui_begin_flags = 
    ImGuiWindowFlags_NoTitleBar         | // Отключает верхнюю панель с названием окна
    ImGuiWindowFlags_NoResize           | // Запрещает пользователю менять размер мышкой
    ImGuiWindowFlags_NoMove             | // Запрещает перетаскивать окно по экрану
    ImGuiWindowFlags_NoCollapse         | // Запрещает сворачивать окно двойным кликом
    ImGuiWindowFlags_AlwaysAutoResize   ; // Окно само подгоняется под контент (если не задан жесткий размер)

void glitched_imgui_text(std::string input) {
        static std::mt19937 rng{std::random_device{}()};
    for (char& c : input) {
        if (c != ' ' && c != ':' && rng() % 50 == 0 && c != '\n') 
            c = "01234567890ABCжшцдл±×÷≈√"[rng() % 17]; // Подмена байта на лету
    }
    ImGui::Text("%s", input.c_str());
}

void tls_query() {
    ImGui::TextWrapped("could not verify TLS connection.\n i strongly advice you to abort connection,\n or keep in mind that you could be\n a victim of MITM attack.\n could be, not automatically are.");
    if(ImGui::Button("proceed anyway")) {
        glob_is_requiring_tls_verified = false;
        tls_status = tls_status_class::establishing;
        status = connection_status::connecting;
    }
}

void messenger_gui() {
    if(tls_status == tls_status_class::establishing) {
        glitched_imgui_text("establishing TLS handshake");
    } else {
        if(ImGui::TreeNode("TLS INFO")) {
            ImGui::TextWrapped("%s", glob_chipher_suite_name.c_str());
            ImGui::TreePop();
        }
    }
    if(!glob_is_requiring_tls_verified) {
        ImGui::TextColored({1,0.5,0.5,1}, "connection not secure");
    }
    if(ImGui::Button("disconnect")) status = connection_status::abort;
}

int main() {
#ifdef _WIN32
std::system("chcp 65001 > nul");
#endif

    logger::info("---starting the client.---");
    
    char socket_port_char_buf[6] = "8080";
    char server_addr_char_buf[20] = "127.0.0.1";
    char nickname_char_buf[globs::max_nickname_size] = "\0";
    bool is_nickname_accepted = false;


    logger::info("variables initialized");
    std::thread network_thread(network_loop);

	sf::RenderWindow window( sf::VideoMode( { 800, 600 } ), "the messenger shot me", sf::Style::Close );
    window.setMinimumSize( sf::Vector2u{ 800, 600 } );
    window.setFramerateLimit(60);

    if (!ImGui::SFML::Init(window))
        return -1;
	ImGui::GetIO().IniFilename = nullptr;
    
    sf::Clock clock;
    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            ImGui::SFML::ProcessEvent(window, *event);
            if (event->is<sf::Event::Closed>()) {
                window.close();
                general_quit = true;
            }
        }
        ImGui::SFML::Update(window, clock.restart());

        if(strlen(nickname_char_buf) < globs::min_nickname_size) {
            is_nickname_accepted = false;
        } else {
            is_nickname_accepted = true;
        }

        ImGui::SetNextWindowPos({0,0});
        ImGui::SetNextWindowSize({800,600});
        ImGui::Begin("messengershotme", nullptr, imgui_begin_flags);

        switch(status) {
            case connection_status::standby:
                glitched_imgui_text("the messenger shot me");
                ImGui::Separator();
                ImGui::Text("enter nickname, address and port:");
                ImGui::InputText("nick", nickname_char_buf, sizeof(nickname_char_buf));
                if(!is_nickname_accepted) {
                    ImGui::Text("nickname too short,\n enter more symbols.");
                }
                ImGui::InputText("ip", server_addr_char_buf, sizeof(server_addr_char_buf));
                if((ImGui::InputText("port", socket_port_char_buf, sizeof(socket_port_char_buf), ImGuiInputTextFlags_EnterReturnsTrue)
                    ||
                    ImGui::Button("connect to server"))
                    &&
                    is_nickname_accepted) {
                        {
                            std::lock_guard<std::mutex> lock(glob_data_mutex);
                            glob_server_addr_string = server_addr_char_buf;
                            glob_socket_port_string = socket_port_char_buf;
                            glob_nickname_string = nickname_char_buf;
                        }
                        status = connection_status::connecting; 
                }
                break;
            case connection_status::ip_translation_failed:
                ImGui::Text("check ip address.\n you should enter IP address,\n not domain.\n also check if you left accidental spaces.");
                if(ImGui::Button("back"))
                    { status = connection_status::standby; }
                break;
            case connection_status::port_translation_failed:
                ImGui::Text("check port.\n it should be\n - in range from 0 to 65536\n       and\n - entered with numbers.\n though, how'd you connect to port\n that is less than 1.");
                if(ImGui::Button("back"))
                    { status = connection_status::standby; }
                break;
            case connection_status::connecting:
                glitched_imgui_text("connecting. please wait.");
                if(ImGui::Button("no"))
                    { status = connection_status::abort; }
                break;
            case connection_status::failed:
                ImGui::Text("connection failed.\n check terminal logs if it's open.");
                if(ImGui::Button("ok"))
                    { status = connection_status::standby; }
                break;
            case connection_status::tls_failed:
                tls_query();
                break;
            case connection_status::connected:
                messenger_gui();
                break;
            case connection_status::abort:
                glitched_imgui_text("aborting connection.\n please wait\n or close this app.");
                break;
        }
        ImGui::End();

		window.clear();
        ImGui::SFML::Render(window);
		window.display();
    }


    logger::info("---starting quit routine.---");
    status = connection_status::abort;
    general_quit = true;
    logger::info("waiting for network thread to join...");
    if (network_thread.joinable()) {
        network_thread.join();
    }

    logger::info("purpose of this program is now complete.");
    logger::info("---this should be the last log. goodbye.---");
}