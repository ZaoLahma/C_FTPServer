/*
 * ftp_impl.c
 *
 *  Created on: Nov 10, 2016
 *      Author: janne
 */

#include "../inc/ftp_impl.h"
#include "../inc/socket_wrapper_impl.h"
#include "../inc/thread_starter_impl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>

typedef struct ClientConn
{
	int controlFd;
	int dataFd;
	char userName[20];
	char currDir[100];
	struct socket_server* server;
} ClientConn;

#define REC_BUF_SIZE 1024

typedef enum FTP_COMMAND
{
	UNKNOWN,
	USER,
	QUIT,
	PASS,
	SYST,
	PWD,
	LIST,
	PORT
} FTP_COMMAND;

typedef struct FtpCommand
{
	FTP_COMMAND command;
	char args[200];
} FtpCommand;

static void ftp_send(int fd, ClientConn* clientConn, char* toSend)
{
	const unsigned int SEND_BUF_SIZE = strlen(toSend) + 2;
	char sendBuf[SEND_BUF_SIZE];
	memset(sendBuf, 0, SEND_BUF_SIZE);
	strncpy(sendBuf, toSend, SEND_BUF_SIZE);
	sendBuf[SEND_BUF_SIZE - 2] = '\r';
	sendBuf[SEND_BUF_SIZE - 1] = '\n';

	printf("ftp_send sending: %s on file descriptor: %d\n", sendBuf, fd);

	int noOfBytesSent = clientConn->server->conn.send(fd, sendBuf, SEND_BUF_SIZE);

	printf("ftp_send sent %d no of bytes\n", noOfBytesSent);
}

static FtpCommand get_command(ClientConn* clientConn)
{

	char receiveBuf[REC_BUF_SIZE] = "";

	FtpCommand command = {UNKNOWN, ""};
	char commandStr[10] = "";

	if(0 != clientConn->server->conn.receive(clientConn->controlFd,
			receiveBuf,
			REC_BUF_SIZE))
	{
		printf("receiveBuf: %s (%d)\n", receiveBuf, (int)strlen(receiveBuf));

		sscanf(receiveBuf, "%s %s", commandStr, command.args);

		if(strcmp("USER", commandStr) == 0)
		{
			command.command = USER;
		}
		else if(strcmp("QUIT", commandStr) == 0)
		{
			command.command = QUIT;
		}
		else if(strcmp("PASS", commandStr) == 0)
		{
			command.command = PASS;
		}
		else if(strcmp("SYST", commandStr) == 0)
		{
			command.command = SYST;
		}
		else if(strcmp("PWD", commandStr) == 0)
		{
			command.command = PWD;
		}
		else if(strcmp("LIST", commandStr) == 0)
		{
			command.command = LIST;
		}
		else if(strcmp("PORT", commandStr) == 0)
		{
			command.command = PORT;
		}
	}
	else
	{
		command.command = QUIT;
	}

	printf("command: %s, %s\n", commandStr, command.args);

	return command;
}

static void handle_user_command(FtpCommand* command, ClientConn* clientConn)
{
	strncpy(clientConn->userName, command->args, 20);
	ftp_send(clientConn->controlFd, clientConn, "330 OK, send password");
}

static void handle_pass_command(FtpCommand* command, ClientConn* clientConn)
{
	if((strcmp("hihello", command->args) == 0) &&
	   (strcmp("janne", clientConn->userName) == 0))
	{
		strncpy(clientConn->currDir, "/", 100);
		ftp_send(clientConn->controlFd, clientConn, "230 OK, user logged in");
	}
	else
	{
		ftp_send(clientConn->controlFd, clientConn, "530 PASS not correct");
	}
}

static void handle_syst_command(ClientConn* clientConn)
{
	ftp_send(clientConn->controlFd, clientConn, "215 UNIX Type: L8");
}

static void handle_pwd_command(ClientConn* clientConn)
{
	const unsigned int SEND_BUF_SIZE = 200;
	char sendBuf[SEND_BUF_SIZE];
	strncat(sendBuf, "257 \"", SEND_BUF_SIZE);
	strncat(sendBuf, clientConn->currDir, SEND_BUF_SIZE);
	strncat(sendBuf, "\"\r\n", SEND_BUF_SIZE);

	ftp_send(clientConn->controlFd, clientConn, sendBuf);
}

static void exec_proc(ClientConn* clientConn, char* cmd)
{
	char buffer[4096] = "";

	FILE* file = popen(cmd, "r");

	while (!feof(file)) {
		if (fgets(buffer, 4096, file) != 0) {
			int lineLength = strlen(buffer);
			buffer[lineLength - 1] = '\r';
			buffer[lineLength]     = '\n';
			ftp_send(clientConn->dataFd, clientConn, buffer);
			memset(buffer, 0, 4096);
		}
	}

	pclose(file);
}

static void handle_list_command(FtpCommand* command, ClientConn* clientConn)
{
	exec_proc(clientConn, "ls -l");

	ftp_send(clientConn->controlFd, clientConn, "226 LIST data send finished");
	clientConn->server->conn.disconnect(clientConn->dataFd);
}

static void handle_port_command(FtpCommand* command, ClientConn* clientConn)
{
	int first = 0;
	int second = 0;
	int third = 0;
	int fourth = 0;

	int high = 0;
	int low = 0;

	int portNo = 0;

	sscanf(command->args,
			"%d,%d,%d,%d,%d,%d",
			&first,
			&second,
			&third,
			&fourth,
			&high,
			&low);

	char address[24] = "";
	address[23] = '\0';

	char addrBuf[4] = "";

	sprintf(addrBuf, "%d", first);
	strncat(address, addrBuf, 24);
	strncat(address, ".", 24);

	sprintf(addrBuf, "%d", second);
	strncat(address, addrBuf, 24);
	strncat(address, ".", 24);

	sprintf(addrBuf, "%d", third);
	strncat(address, addrBuf, 24);
	strncat(address, ".", 24);

	sprintf(addrBuf, "%d", fourth);
	strncat(address, addrBuf, 24);

	printf("address: %s\n", address);

	socket_client client;
	init_client_socket(&client);

	high = high * 256;

	portNo = high + low;

	char portBuf[6] = "";
	portBuf[5] = '\0';

	sprintf(portBuf, "%d", portNo);

	printf("portNo: %s\n", portBuf);

	clientConn->dataFd = client.connect(address, portBuf);

	printf("dataFd: %d\n", clientConn->dataFd);

	ftp_send(clientConn->controlFd, clientConn, "200 PORT command successful");
}

static void handle_quit_command(ClientConn* clientConn)
{
	ftp_send(clientConn->controlFd, clientConn, "221 Bye Bye");
}

static void* client_conn_main(void* arg)
{
	ClientConn* clientConn = (ClientConn*)arg;

	printf("New client connected with fd: %d\n", clientConn->controlFd);

	ftp_send(clientConn->controlFd, clientConn, "220 OK");

	FtpCommand command;

	int running = 1;

	while(running)
	{
		command = get_command(clientConn);

		switch(command.command)
		{
		case USER:
			handle_user_command(&command, clientConn);
			break;
		case QUIT:
			handle_quit_command(clientConn);
			running = 0;
			break;
		case PASS:
			handle_pass_command(&command, clientConn);
			break;
		case SYST:
			handle_syst_command(clientConn);
			break;
		case PWD:
			handle_pwd_command(clientConn);
			break;
		case LIST:
			handle_list_command(&command, clientConn);
			break;
		case PORT:
			handle_port_command(&command, clientConn);
			break;
		default:
			ftp_send(clientConn->controlFd, clientConn, "500 - Not implemented");
		}
	}

	clientConn->server->conn.disconnect(clientConn->controlFd);

	printf("Disconnected client with fd: %d\n", clientConn->controlFd);

	free(clientConn);

	return 0;
}

void run_ftp(int* running)
{
	printf("FTP server starting, running: %d\n", *running);
	struct socket_server server;
	init_server_socket(&server);

	int serverSocketFd = server.get_server_socket_fd("3370");
	int clientSocketFd = -1;

	ThreadStarter thread;
	init_thread_starter(&thread, POOL);

	while(*running)
	{
		clientSocketFd = server.wait_for_connection(serverSocketFd);

		if(*running)
		{
			struct ClientConn* client = (struct ClientConn*)malloc(sizeof(struct ClientConn));
			client->controlFd = clientSocketFd;
			client->server = &server;

			thread.execute_function(&client_conn_main, client);
		}
	}

	server.conn.disconnect(serverSocketFd);
}
