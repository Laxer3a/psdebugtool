/*
	bool initSockets();
	bool connectServer(u8* ipAdr, unsigned short socketNum);
	void releaseServer();
	void releaseSockets();
*/

#include <stdio.h>
#include <winsock2.h>
#pragma comment(lib,"ws2_32.lib") //Winsock Library

WSADATA wsa;

typedef unsigned char u8;

bool initSockets() {
	if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
	{
		printf("Failed. Error Code : %d",WSAGetLastError());
		return false;
	}
	printf("Winsock Init...\n");
	return true;
}

SOCKET s;
struct sockaddr_in server;

bool connectServer(u8* ipAdr, unsigned short socketNum) {
	if ((s = socket(AF_INET , SOCK_STREAM , 0 )) == INVALID_SOCKET)
	{
		printf("Could not create socket : %d" , WSAGetLastError());
	}
	printf("Socket Created...\n");

	char buffer[256];
	sprintf(buffer,"%i.%i.%i.%i",ipAdr[0],ipAdr[1],ipAdr[2],ipAdr[3]);
	
	server.sin_addr.s_addr = inet_addr(buffer);
	server.sin_family = AF_INET;
	server.sin_port = htons( 80 );
	
	//Connect to remote server
	if (connect(s , (struct sockaddr *)&server , sizeof(server)) < 0)
	{
		printf("connect error to server %s\n",buffer);
		return false;
	}
	
	printf("Connected to %s\n",buffer);
	return true;
}

void releaseServer() {
	printf("Socket Freed...\n");
	closesocket(s);
}

void releaseSockets() {
	printf("Winsock Released...\n");
	WSACleanup();
}
