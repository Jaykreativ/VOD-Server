#include "Network.h"

#include "Shares/NetworkData.h"

#include <thread>
#include <mutex>
#include <memory>
#include <iostream>
#include <string>
#include <cstring>
#include <stdio.h>
#include <vector>
#include <unordered_map>

#ifdef _WIN32 // windows specific socket include

#include <WinSock2.h>
#include <WS2tcpip.h>

#elif __linux__

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>

typedef in_addr IN_ADDR;
typedef in6_addr IN6_ADDR;

#endif 

namespace sock {
	int close(int socket) {
#ifdef _WIN32
		return closesocket(socket);
#elif __linux__
		return close(socket);
#endif
	}

	int pollState(pollfd fds[], size_t nfds, int timeout) {
#ifdef _WIN32
		return WSAPoll(fds, nfds, timeout);
#elif __linux__
		return poll(fds, nfds, timeout);
#endif
	}

	IN_ADDR presentationToAddrIPv4(std::string presentation) {
		IN_ADDR addr;
		if (inet_pton(AF_INET, presentation.c_str(), &addr) <= 0) {
			fprintf(stderr, "error while decoding ip address\n");
			exit(3);
		}
		return addr;
	}
	IN6_ADDR presentationToAddrIPv6(std::string presentation) {
		IN6_ADDR addr;
		if (inet_pton(AF_INET6, presentation.c_str(), &addr) <= 0) {
			fprintf(stderr, "error while decoding ip address\n");
			exit(3);
		}
		return addr;
	}

	std::string addrToPresentationIPv4(IN_ADDR addr) {
		char ip4[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &addr, ip4, INET_ADDRSTRLEN);
		return std::string(ip4);
	}
	std::string addrToPresentationIPv6(IN6_ADDR addr) {
		char ip6[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &addr, ip6, INET6_ADDRSTRLEN);
		return ip6;
	}
	std::string addrToPresentation(sockaddr* sa) {
		if (sa->sa_family == AF_INET) {
			return addrToPresentationIPv4(reinterpret_cast<sockaddr_in*>(sa)->sin_addr);
		}

		return addrToPresentationIPv6(reinterpret_cast<sockaddr_in6*>(sa)->sin6_addr);
	}

	int lastError() {
#ifdef _WIN32
		return WSAGetLastError();
#elif __linux__
		return errno;
#endif
	}

	void printLastError(const char* msg) {
#ifdef _WIN32
		char* s = NULL;
		FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, WSAGetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR)&s, 0, NULL);
		fprintf(stderr, "%s: %s\n", msg, s);
#elif __linux__
		perror(msg);
#endif
	}
}

void htonMat4(const float mat4[16], void* nData) {
	uint32_t* buf = reinterpret_cast<uint32_t*>(nData);
	for (size_t i = 0; i < 16; i += 1) {
		uint32_t nf = htonl(mat4[i]);
		buf[i] = nf;
	}
}

void ntohMat4(const void* nData, float mat4[16]) {
	const uint32_t* buf = reinterpret_cast<const uint32_t*>(nData);
	for (size_t i = 0; i < 16; i += 1) {
		float hf = ntohl(buf[i]);
		mat4[i] = hf;
	}
}

/* Packets */

enum PacketType {
	eMESSAGE = 1,
	eCONNECT = 2,
	eDISCONNECT = 3,
	eMOVE = 4
};

class Packet {
public:
	void sendTo(int socket, int flags = 0);

	static std::shared_ptr<Packet> receiveFrom(int& type, int socket, int flags = 0);

protected:
	uint32_t fullSize();

	static uint32_t headerSize();

	virtual uint32_t dataSize() = 0;

	// packs just the header
	// takes the buffer which contains the network package
	void packHeader(char* buf, const int type);

	// takes just the header
	// returns the size of the data stored in the packet
	static void unpackHeader(const char* buf, uint32_t& size, int& type);

	// takes the pointer to the data part and fills it with the packed data of the package
	// should use packHeader
	virtual void pack(char* buf) = 0;

	// takes the pointer to the data part and fills the package with data
	virtual void unpackData(const char* buf, uint32_t size) = 0;
};

class MessagePacket : public Packet {
	friend class Packet;
public:
	// data
	std::string id = "";
	std::string msg = "";

protected:
	uint32_t dataSize();

	// packs the data into the given buffer, buffer needs to have the same size as packet.fullSize()
	void pack(char* buf);

	// takes just the data part
	void unpackData(const char* buf, uint32_t size);
};

class ConnectPacket : public Packet {
	friend class Packet;
public:
	// data
	std::string username = "";

protected:
	uint32_t dataSize();

	// packs the data into the given buffer, buffer needs to have the same size as packet.fullSize()
	void pack(char* buf);

	// takes just the data part
	void unpackData(const char* buf, uint32_t size);
};

class DisconnectPacket : public Packet {
	friend class Packet;
public:
	// data
	std::string username = "";

protected:
	uint32_t dataSize();

	// packs the data into the given buffer, buffer needs to have the same size as packet.fullSize()
	void pack(char* buf);

	// takes just the data part
	void unpackData(const char* buf, uint32_t size);
};

class MovePacket : public Packet {
	friend class Packet;
public:
	// data
	std::string username = "";
	float transform[16] = {};

protected:
	uint32_t dataSize();

	// packs the data into the given buffer, buffer needs to have the same size as packet.fullSize()
	void pack(char* buf);

	// takes just the data part
	void unpackData(const char* buf, uint32_t size);
};

// Packet
void Packet::sendTo(int socket, int flags) {
	uint32_t len = fullSize();
	char* buf = new char[len];
	pack(buf);

	uint32_t offset = 0;
	while (offset < len) {
		int bytesSent = send(socket, buf + offset, len - offset, 0);
		if (bytesSent == -1) {
			sock::printLastError("Packet::send");
			delete[] buf;
			return;
		}
		offset += bytesSent;
	}
	delete[] buf;
}

std::shared_ptr<Packet> Packet::receiveFrom(int& type, int socket, int flags) {
	char* buf = new char[headerSize()];
	int bytesRead = recv(socket, buf, headerSize(), 0); // get just header
	if (bytesRead == -1) {
		sock::printLastError("Packet::recv header");
		delete[] buf;
		return nullptr;
	}
	uint32_t dataSize;
	unpackHeader(buf, dataSize, type);
	delete[] buf;

	buf = new char[dataSize];
	bytesRead = recv(socket, buf, dataSize, 0); // get just data
	if (bytesRead == -1) {
		sock::printLastError("Packet::recv data");
		delete[] buf;
		return nullptr;
	}

	std::shared_ptr<Packet> spPacket;
	switch (type)
	{
	case eMESSAGE: {
		spPacket = std::make_shared<MessagePacket>();
		spPacket->unpackData(buf, dataSize);
		break;
	}
	case eCONNECT: {
		spPacket = std::make_shared<ConnectPacket>();
		spPacket->unpackData(buf, dataSize);
		break;
	}
	case eDISCONNECT: {
		spPacket = std::make_shared<DisconnectPacket>();
		spPacket->unpackData(buf, dataSize);
		break;
	}
	case eMOVE: {
		spPacket = std::make_shared<MovePacket>();
		spPacket->unpackData(buf, dataSize);
		break;
	}
	default:
		break;
	}
	delete[] buf;

	return spPacket;
}

uint32_t Packet::fullSize() {
		return headerSize() + dataSize();
	}

uint32_t Packet::headerSize() {
		return 2 * sizeof(uint32_t);
	}

void Packet::packHeader(char* buf, const int type) {
		uint32_t* uintBuf = reinterpret_cast<uint32_t*>(buf);
		uintBuf[0] = htonl(dataSize());
		uintBuf[1] = htonl(type);
	}

void Packet::unpackHeader(const char* buf, uint32_t& size, int& type) {
		const uint32_t* uintBuf = reinterpret_cast<const uint32_t*>(buf);
		size = ntohl(uintBuf[0]);
		type = ntohl(uintBuf[1]);
	}

// MessagePacket
uint32_t MessagePacket::dataSize() {
		return sizeof(uint32_t) + id.size() + sizeof(uint32_t) + msg.size();
	}

void MessagePacket::pack(char* buf) {
		packHeader(buf, eMESSAGE); buf += headerSize();
		/* data */
		uint32_t idSize = htonl(id.size());
		memcpy(buf, &idSize, sizeof(uint32_t));    buf += sizeof(uint32_t);
		memcpy(buf, id.data(), id.size());         buf += id.size();
		uint32_t msgSize = htonl(msg.size());
		memcpy(buf, &msgSize, sizeof(uint32_t));   buf += sizeof(uint32_t);
		memcpy(buf, msg.data(), msg.size());       buf += msg.size();
}

void MessagePacket::unpackData(const char* buf, uint32_t size) {
		uint32_t idSize = ntohl(reinterpret_cast<const uint32_t*>(buf)[0]); buf += sizeof(uint32_t);
		id = std::string(buf, idSize); buf += idSize;
		uint32_t msgSize = ntohl(reinterpret_cast<const uint32_t*>(buf)[0]); buf += sizeof(uint32_t);
		msg = std::string(buf, msgSize); buf += msgSize;
}

// ConnectPacket
uint32_t ConnectPacket::dataSize() {
	return username.size();
}

void ConnectPacket::pack(char* buf) {
	packHeader(buf, eCONNECT); buf += headerSize();
	/* data */
	memcpy(buf, username.data(), username.size());
}

void ConnectPacket::unpackData(const char* buf, uint32_t size) {
	username = std::string(buf, size);
}

// DisconnectPacket
uint32_t DisconnectPacket::dataSize() {
	return username.size();
}

void DisconnectPacket::pack(char* buf) {
	packHeader(buf, eDISCONNECT); buf += headerSize();
	/* data */
	memcpy(buf, username.data(), username.size());
}

void DisconnectPacket::unpackData(const char* buf, uint32_t size) {
	username = std::string(buf, size);
}

// MovePacket
uint32_t MovePacket::dataSize() {
	return sizeof(uint32_t) + username.size() + sizeof(float)*16;
}

void MovePacket::pack(char* buf) {
	packHeader(buf, eMOVE); buf += headerSize();
	/* data */
	uint32_t usernameSize = htonl(username.size());
	memcpy(buf, &usernameSize, sizeof(uint32_t)); buf += sizeof(uint32_t);
	memcpy(buf, username.data(), username.size()); buf += username.size();
	htonMat4(transform, buf);
}

void MovePacket::unpackData(const char* buf, uint32_t size) {
	uint32_t usernameSize = ntohl(reinterpret_cast<const uint32_t*>(buf)[0]); buf += sizeof(uint32_t);
	username = std::string(buf, usernameSize); buf += usernameSize;
	ntohMat4(buf, transform);
}

namespace server {
	std::mutex mTerminate; // controls access to variables for terminating the server
	volatile bool shouldStop = false;
	std::thread thread;

	struct ClientStorage {
		std::vector<int> sockets;
		std::unordered_map<std::string, int> nameSocketMap;
		std::unordered_map<int, std::string> socketNameMap;

		bool isPresent(int socket, std::string name) {
			return nameSocketMap.count(name) || socketNameMap.count(socket);
		}

		void addClient(int socket, std::string name) {
			sockets.push_back(socket);
			nameSocketMap[name] = socket;
			socketNameMap[socket] = name;
		}

		void deleteClient(int socket, std::string name) {
			nameSocketMap.erase(name);
			socketNameMap.erase(socket);
			
			bool found = false;
			for (size_t i = 0; !found && i < sockets.size(); i++) {
				if (sockets[i] == socket) {
					sockets.erase(sockets.begin() + i);
					found = true;
				}
			}
		}

		void clear() {
			sockets.clear();
			nameSocketMap.clear();
			socketNameMap.clear();
		}

	} clientStorage;

	// the file descriptors used in the poll command
	// (#0:server)
	std::vector<pollfd> pollfds;

	enum PollResult {
		eSUCCESS
	};

	void acceptClient(int serverSocket) {
		sockaddr_storage clientAddr;
		socklen_t addrSize = sizeof clientAddr;
		int clientSocket;
		if ((clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &addrSize)) < 0) {
			sock::printLastError("accept");
			exit(sock::lastError());
		}

		pollfd clientPollfd;
		clientPollfd.fd = clientSocket;
		clientPollfd.events = POLLIN;
		clientPollfd.revents = 0;
		pollfds.push_back(clientPollfd); // add pollfd, client will be inserted into the map when receiving the connect packet

		printf("client connected: %s\n", sock::addrToPresentation(reinterpret_cast<sockaddr*>(&clientAddr)).c_str());
	}

	void recvClient(int socket) {
		int type;
		auto spPacket = Packet::receiveFrom(type, socket);
		switch (type)
		{
		case eMESSAGE: {
			MessagePacket& packet = *reinterpret_cast<MessagePacket*>(spPacket.get());
			printf("server msg: %s (%s)\n", packet.msg.c_str(), packet.id.c_str());
			for (int clientSocket : clientStorage.sockets)
				packet.sendTo(clientSocket);
			break;
		}
		case eCONNECT: {
			ConnectPacket& packet = *reinterpret_cast<ConnectPacket*>(spPacket.get());
			if (clientStorage.isPresent(socket, packet.username)) { // prevent multiple usernames
				printf("%s already present, wont be accepted\n", packet.username.c_str());
				break;
			}
			clientStorage.addClient(socket, packet.username);
			printf("%s joined the server\n", packet.username.c_str());

			for (int clientSocket : clientStorage.sockets) {
				packet.sendTo(clientSocket); // tell all clients(including the new one) that a new player joined
				if (clientSocket != socket) {
					ConnectPacket connectPacket;
					connectPacket.username = clientStorage.socketNameMap.at(clientSocket);
					connectPacket.sendTo(socket); // send the new client all clients that where already present
				}
			}

			break;
		}
		case eDISCONNECT: {
			DisconnectPacket& packet = *reinterpret_cast<DisconnectPacket*>(spPacket.get());
			if (!clientStorage.isPresent(socket, packet.username)) {
				printf("%s was not connected, already left\n", packet.username.c_str());
				break;
			}
			printf("%s left the server\n", packet.username.c_str());
			clientStorage.deleteClient(socket, packet.username);
			for (int clientSocket : clientStorage.sockets) {
				if(clientSocket != socket)
					packet.sendTo(clientSocket);
			}
			break;
		}
		case eMOVE: {
			MovePacket& packet = *reinterpret_cast<MovePacket*>(spPacket.get());
			for (int clientSocket : clientStorage.sockets) {
				if(clientSocket != socket)
					packet.sendTo(clientSocket);
			}
			break;
		}
		default: {
			break;
		}
		}
	}

	void disconnectClient(int socket, int index) {
		sockaddr clientAddr; // find address to print
		socklen_t addrSize = sizeof clientAddr;
		getpeername(socket, &clientAddr, &addrSize);
		printf("client disconnected: %s\n", sock::addrToPresentation(&clientAddr).c_str());

		if (sock::close(socket) < 0) {
			sock::printLastError("close");
			exit(sock::lastError());
		}
		pollfds.erase(pollfds.begin()+index+1); // delete the clients pollfd, +1 for the server pollfd
	}

	PollResult handlePoll(int pollCount) {
		if (pollCount == 0) // return if there are no polls
			return eSUCCESS;

		int checkedPollCount = 0;

		pollfd serverPollfd = pollfds[0];
		if (serverPollfd.revents & POLLIN) { // accept client
			acceptClient(serverPollfd.fd);
			checkedPollCount++;
		}

		// go through all clients
		// i is the index of the client in clientSockets
		int eraseOffset = 0; // offset the index by the times erase was used as erase shifts all remaining indices by -1
		for (int i = 0; i < pollfds.size()-1; i++) {
			pollfd poll = pollfds[i+1];
			if (poll.revents & POLLIN) {
				recvClient(poll.fd);
			}
			if (poll.revents & POLLHUP) {
				disconnectClient(poll.fd, i-eraseOffset);
				eraseOffset++;
			}

			if (poll.revents & (POLLIN | POLLHUP)) // add checked if poll had events
				checkedPollCount++;
			if (checkedPollCount >= pollCount) // return when all polls are checked
				return eSUCCESS;
		}
	}

	int getServerSocket(NetworkData& network) {
		addrinfo hints;
		addrinfo* serverInfo;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;

		int status;
		if ((status = getaddrinfo(NULL, network.port.c_str(), &hints, &serverInfo)) != 0) { // turn the port into a full address, ip is null because its a server
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
			exit(sock::lastError());
		}

		// creating the socket
		int serverSocket;
		if ((serverSocket = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol)) < 0) {
			sock::printLastError("socket");
			exit(sock::lastError());
		}

		// enable port reuse
		//const char yes = 1;
		//if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
		//	sock::printLastError("setsockopt");
		//	exit(sock::lastError());
		//}

		// bind to port
		if (bind(serverSocket, serverInfo->ai_addr, serverInfo->ai_addrlen) < 0) {
			sock::printLastError("bind");
			exit(sock::lastError());
		}

		if (listen(serverSocket, network.backlog) < 0) {
			sock::printLastError("listen");
			exit(sock::lastError());
		}

		freeaddrinfo(serverInfo);

		return serverSocket;
	}

	void loop(NetworkData network) {
		int serverSocket = getServerSocket(network);
		pollfd serverPollfd; // make a poll fd for the server socket, gets an event when a new client connects
		serverPollfd.fd = serverSocket;
		serverPollfd.events = POLLIN;
		serverPollfd.revents = 0;
		pollfds.push_back(serverPollfd);
		printf("server running\n");

		while (true) {
			{
				std::lock_guard<std::mutex> lk(mTerminate);
				if (shouldStop) {
					for (size_t i = 1; i < pollfds.size(); i++)
						sock::close(pollfds[i].fd);
					pollfds.clear();
					clientStorage.clear();
					break;
				}
			}

			int pollCount = sock::pollState(pollfds.data(), pollfds.size(), 100);// fetch events of the given pollfds
			if (pollCount == 0)
				continue;

			if (pollCount == -1) {
				sock::printLastError("poll");
				exit(sock::lastError());
			}

			if (handlePoll(pollCount) != eSUCCESS)
				printf("handlePoll failed\n");
		}

		if (sock::close(serverSocket) < 0) {
			sock::printLastError("close");
			exit(sock::lastError());
		}
		printf("server done\n");
	}
}

void runServer(NetworkData network) {
	server::thread = std::thread(server::loop, network);
}

void terminateServer() {
	{
		std::lock_guard<std::mutex> lk(server::mTerminate);
		server::shouldStop = true;
	}
	server::thread.join();
	server::shouldStop = false;
}