// server.cpp
#include "logger.hpp"
#include <SFML/Network.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <algorithm>
#include <thread>
#include <fstream>
#include <sstream>

// Структура для хранения данных подключенного клиента
struct Client {
    std::shared_ptr<sf::TcpSocket> socket;
    std::string nickname;
};

// Глобальный список клиентов и мьютекс для защиты ресурсов
std::vector<Client> clients;
std::mutex clientsMutex;

// Безопасная функция рассылки сообщений
void broadcastMessage(const std::string& message, const std::shared_ptr<sf::TcpSocket>& senderSocket = nullptr) {
    std::vector<std::shared_ptr<sf::TcpSocket>> targetSockets;

    // Шаг 1: Быстро копируем указатели на сокеты под защитой мьютекса
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (const auto& client : clients) {
            if (client.socket != senderSocket) {
                targetSockets.push_back(client.socket);
            }
        }
    }

    // Шаг 2: Рассылаем данные вне мьютекса. Если в процессе отправки 
    // кто-то отключится, глобальный вектор не пострадает.
    for (const auto& socket : targetSockets) {
        if (socket->send(message.data(), message.size()) == sf::Socket::Status::Error) {
            // Ошибки будут обработаны в индивидуальных потоках handleClient при вызове receive()
            logger::warn("error sending message to {}", socket->getRemoteAddress()->toString());
        }
    }
}

// Функция обслуживания конкретного клиента
void handleClient(std::shared_ptr<sf::TcpSocket> clientSocket) {
    char buffer[1024];
    std::size_t received = 0;
    std::string nickname = "Аноним";

    // 1. Прием никнейма
    if (clientSocket->receive(buffer, sizeof(buffer) - 1, received) == sf::Socket::Status::Done && received > 0) {
        buffer[received] = '\0';
        nickname = std::string(buffer);
    } else {
        // Если клиент отключился сразу после рукопожатия
        std::cout << "[Инфо] Клиент отключился до отправки никнейма." << std::endl;
        logger::info("client @ {} disconnected after handshake but before sending it's nickname", clientSocket->getRemoteAddress()->toString());
        return;
    }

    // Добавляем клиента в общий список
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.push_back({clientSocket, nickname});
    }

    std::string systemMsg = "[Сервер] " + nickname + " вошел в чат!";
    std::cout << systemMsg << std::endl;
    broadcastMessage(systemMsg, clientSocket);

    // 2. Основной цикл приема сообщений
    while (true) {
        sf::Socket::Status status = clientSocket->receive(buffer, sizeof(buffer) - 1, received);

        if (status == sf::Socket::Status::Done && received > 0) {
            buffer[received] = '\0';
            std::string chatMsg = nickname + ": " + std::string(buffer);
            std::cout << chatMsg << std::endl;
            broadcastMessage(chatMsg, clientSocket);
        } 
        else {
            // Любой статус кроме Done (Disconnected, Error) прерывает цикл
            break;
        }
    }

    // 3. Обработка отключения клиента
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.erase(std::remove_if(clients.begin(), clients.end(),
            [&clientSocket](const Client& c) { return c.socket == clientSocket; }), clients.end());
    }

    systemMsg = nickname + " покинул чат.";
    logger::info("{}", systemMsg);
    broadcastMessage(systemMsg);
    
    // Принудительно отключаем сокет перед выходом из потока
    clientSocket->disconnect();
}

// Функция для удобного чтения файла в строку (mbedTLS требует содержимое файла целиком)
std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[Критическая ошибка] Не удалось открыть файл: " << filename << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main() {
#ifdef _WIN32
    std::system("chcp 65001 > nul");
#endif

    unsigned short port = 53000;
    sf::TcpListener listener;

    if (listener.listen(port) != sf::Socket::Status::Done) {
        std::cerr << "[Ошибка] Не удалось запустить сервер на порту " << port << std::endl;
        return -1;
    }

    std::string certData = readFile("server.crt");
    std::string keyData = readFile("server.key");

    if (certData.empty() || keyData.empty()) {
        std::cerr << "[Ошибка] Сертификаты не найдены. Сгенерируйте их через OpenSSL!" << std::endl;
        return -1;
    }

    std::cout << "=== TLS Чат-Сервер запущен на порту " << port << " ===" << std::endl;
    
    while (true) {
        auto clientSocket = std::make_shared<sf::TcpSocket>();

        if (listener.accept(*clientSocket) == sf::Socket::Status::Done) {
            std::cout << "[Новое подключение] Попытка установить TLS Handshake..." << std::endl;

            // Передаем валидные считанные строки в mbedTLS
            if (clientSocket->setupTlsServer(certData, keyData, "") == sf::TcpSocket::TlsStatus::HandshakeComplete) {
                auto remoteAddress = clientSocket->getRemoteAddress();
                std::cout << "[Успех] Шифрование TLS успешно установлено!" << std::endl;

                std::thread clientThread(handleClient, clientSocket);
                clientThread.detach(); 
            } else {
                std::cerr << "[Ошибка] Не удалось выполнить TLS Handshake. Подключение сброшено." << std::endl;
                clientSocket->disconnect();
            }
        }
    }
    return 0;
}
