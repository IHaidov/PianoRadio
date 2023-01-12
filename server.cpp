#include <iostream>
#include <vector>
#include <map>
#include <thread>
#include <poll.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>

#define HEARTBEAT_INTERVAL 10

// Define the maximum number of users that can be in a room
onst intc MAX_USERS_PER_ROOM = 16;

// Define user roles
enum class UserRole {
    USER,
    MUSICIAN
};

// Define user class
class User {
public:
    User(int socket, UserRole role) : socket_(socket), role_(role) {}

    // Get user's socket descriptor
    int getSocket() { return socket_; }

    // Get user's role
    UserRole getRole() { return role_; }

private:
    int socket_;
    UserRole role_;
};

// Define room class
class Room {
public:
    Room(int id) : id_(id) {}

    // Get room's ID
    int getId() { return id_; }

    // Add a user to the room
    void addUser(User& user) {
        users_.push_back(user);
    }

    // Remove a user from the room
    void removeUser(User& user) {
        users_.erase(std::remove(users_.begin(), users_.end(), user), users_.end());
    }

    // Broadcast a message to all users in the room
    void broadcast(const char* message, int messageSize) {
        for (auto& user : users_) {
            int socket = user.getSocket();
            write(socket, message, messageSize);
        }
    }

    // Check if the room is empty
    bool isEmpty() {
        return users_.empty();
    }

private:
    int id_;
    std::vector<User> users_;
};

// Define the server class
class Server {
public:
    Server() : nextRoomId_(0) {}

    // Start the server
    void start() {
        // Create a socket
        int socketDescriptor = socket(AF_INET, SOCK_STREAM, 0);

        // Bind the socket to an address and port
        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(12345);
        bind(socketDescriptor, (sockaddr*) &address, sizeof(address));

        // Listen for incoming connections
        listen(socketDescriptor, SOMAXCONN);

        // Set up the poll structure
        struct pollfd pollFd;
        pollFd.fd = socketDescriptor;
        pollFd.events = POLLIN;

        while (true) {
            // Wait for activity on the socket
            poll(&pollFd, 1, -1);

            // Check for incoming connections
            if (pollFd.revents & POLLIN) {
                sockaddr_in clientAddress;
                socklen_t clientAddressSize = sizeof(clientAddress);
                int clientSocket = accept(socketDescriptor, (sockaddr*) &clientAddress, &clientAddressSize);
                // Start a new thread to handle the connection
                std::thread([this, clientSocket]() {
                // Send the list of active rooms to the client
                sendRoomList(clientSocket);

                // Wait for the client to send a request to create or join a room
                char requestBuffer[1024];
                int requestSize = recv(clientSocket, requestBuffer, sizeof(requestBuffer), 0);

                // Process the request
                handleRequest(clientSocket, requestBuffer, requestSize);

                // Close the socket
                close(clientSocket);
            }).detach();
        }

        // Send heartbeat to all users to check if they are alive
        sendHeartbeat();

        // Remove empty rooms
        removeEmptyRooms();
    }
}

private:
std::map<int, Room> rooms_;
int nextRoomId_;

// Send the list of active rooms to a client
void sendRoomList(int clientSocket) {
    // Build the list of room IDs
    std::string roomList;
    for (auto& room : rooms_) {
        roomList += std::to_string(room.first) + '\n';
    }

    // Send the list to the client
    send(clientSocket, roomList.c_str(), roomList.size(), 0);
}

// Handle a request to create or join a room
void handleRequest(int clientSocket, char* requestBuffer, int requestSize) {
    std::string request(requestBuffer, requestSize);
    if (request.substr(0, 6) == "create") {
        // Create a new room
        User user(clientSocket, UserRole::MUSICIAN);
        Room room(nextRoomId_++);
        room.addUser(user);
        rooms_[room.getId()] = room;

        // Send the room ID to the client
        send(clientSocket, std::to_string(room.getId()).c_str(), std::to_string(room.getId()).size(), 0);
    } else if (request.substr(0, 4) == "join") {
        // Get the room ID
        int roomId = std::stoi(request.substr(5));

        // Check if the room exists
        if (rooms_.count(roomId) == 0) {
            send(clientSocket, "invalid room", 12, 0);
            return;
        }

        // Check if the room is full
        Room& room = rooms_[roomId];
        if (room.users_.size() >= MAX_USERS_PER_ROOM) {
            send(clientSocket, "room full", 9, 0);
            return;
        }

        // Add the user to the room
        User user(clientSocket, UserRole::USER);
        room.addUser(user);

        // Send the room ID to the client
        send(clientSocket, std::to_string(roomId).c_str(), std::to_string(roomId).size(), 0);
    } else {
        // Invalid request
        send(clientSocket, "invalid request", 14, 0);
        return;
    }
}

// Send a heartbeat to all users to check if they are alive
void sendHeartbeat() {
    for (auto& room : rooms_) {
        Room& current_room = room.second;
        for (auto& user : current_room.users_) {
            int socket = user.getSocket();
            // send the message
            int sent = send(socket, "heartbeat", 9, 0);
            if (sent <= 0)
            {
                cout << "User is disconnected" << endl;
                closesocket(socket);
                current_room.users_.erase(user);
            }
        }

        Sleep(HEARTBEAT_INTERVAL * 1000);
        }
    }
}

// Remove empty rooms from the server
void removeEmptyRooms() {
    for (auto it = rooms_.begin(); it != rooms_.end();) {
        if (it->second.isEmpty()) {
            rooms_.erase(it++);
        } else {
            ++it;
        }
    }
}
};

int main() {
Server server;
server.start();
return 0;
}
