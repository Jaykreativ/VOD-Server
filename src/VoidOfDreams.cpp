#include "Layers/Network.h"

#include <iostream>

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>

void startWSA() {
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		fprintf(stderr, "WSAStartup failed\n");
		exit(1);
	}

	if (LOBYTE(wsaData.wVersion) != 2 ||
		HIBYTE(wsaData.wVersion) != 2)
	{
		fprintf(stderr, "Version 2.2 of Winsock not available\n");
		WSACleanup();
		exit(1);
	}
}

void closeWSA() {
	WSACleanup();
}
#endif // _WIN32


int main() {
#ifdef _WIN32
	startWSA();
#endif // _WIN32

	NetworkData network = {};

	runServer(network);

	while (true) {
		std::string input;
		getline(std::cin, input);

		if (input == "stop") {
			terminateServer();
			break;
		}
	}

#ifdef _WIN32
	closeWSA();
#endif // _WIN32

	return 0;
}
