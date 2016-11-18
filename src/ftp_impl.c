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
#include <pthread.h>

typedef struct ClientConn
{
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int controlFd;
	int dataFd;
	char userName[20];
	char currDir[100];
	unsigned char passivePort[2];
	unsigned char ipAddr[4];
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
	PORT,
	PASV,
	CWD
} FTP_COMMAND;

typedef struct FtpCommand
{
	FTP_COMMAND command;
	char args[75];
	char commandStr[20];
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

	FtpCommand command = {UNKNOWN, "", ""};

	int noOfBytesReceived = clientConn->server->conn.receive(clientConn->controlFd,
							receiveBuf,
							REC_BUF_SIZE);

	receiveBuf[noOfBytesReceived] = '\0';

	printf("noOfBytesReceived: %d\n", noOfBytesReceived);

	if(0 != noOfBytesReceived)
	{
		receiveBuf[noOfBytesReceived - 2] = '\0';

		printf("receiveBuf: %s (%d)\n", receiveBuf, (int)strlen(receiveBuf));

		sscanf(receiveBuf, "%s %s", command.commandStr, command.args);

		if(strcmp("USER", command.commandStr) == 0)
		{
			command.command = USER;
		}
		else if(strcmp("QUIT", command.commandStr) == 0)
		{
			command.command = QUIT;
		}
		else if(strcmp("PASS", command.commandStr) == 0)
		{
			command.command = PASS;
		}
		else if(strcmp("SYST", command.commandStr) == 0)
		{
			command.command = SYST;
		}
		else if(strcmp("PWD", command.commandStr) == 0)
		{
			command.command = PWD;
		}
		else if(strcmp("LIST", command.commandStr) == 0)
		{
			command.command = LIST;
		}
		else if(strcmp("PORT", command.commandStr) == 0)
		{
			command.command = PORT;
		}
		else if(strcmp("PASV", command.commandStr) == 0)
		{
			command.command = PASV;
		}
		else if(strcmp("CWD", command.commandStr) == 0)
		{
			command.command = CWD;
		}
	}
	else
	{
		command.command = QUIT;
	}

	printf("command: %s, %s\n", command.commandStr, command.args);

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
		strncpy(clientConn->currDir, "/Users/janne", 100);
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
	if(-1 != clientConn->dataFd)
	{
		ftp_send(clientConn->controlFd, clientConn, "150 LIST executed ok, data follows");
		char cmd[20] = "";
		sprintf(cmd, "%s %s", "ls -l", clientConn->currDir);
		exec_proc(clientConn, cmd);
		ftp_send(clientConn->controlFd, clientConn, "226 LIST data send finished");
		clientConn->server->conn.disconnect(clientConn->dataFd);
		clientConn->dataFd = -1;
	}
	else
	{
		ftp_send(clientConn->controlFd, clientConn, "530 LIST not executed (run PORT or PASV first)");
	}
}

static void handle_port_command(FtpCommand* command, ClientConn* clientConn)
{
	unsigned char address[4];

	unsigned char ports[2];

	sscanf(command->args,
			"%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
			&address[0],
			&address[1],
			&address[2],
			&address[3],
			&ports[0],
			&ports[1]);

	char addrString[24] = "";

	sprintf(addrString,
			"%d.%d.%d.%d",
			address[0],
			address[1],
			address[2],
			address[3]);

	printf("addrString: %s\n", addrString);

	unsigned int portNo = (ports[0] << 8) + ports[1];
	printf("portNo: %d\n", portNo);

	char portString[6] = "";
	sprintf(portString, "%u", portNo);

	socket_client client;
	init_client_socket(&client);

	clientConn->dataFd = client.connect(addrString, portString);

	printf("dataFd: %d\n", clientConn->dataFd);

	if(-1 != clientConn->dataFd)
	{
		ftp_send(clientConn->controlFd, clientConn, "200 PORT command successful");
	}
	else
	{
		ftp_send(clientConn->controlFd, clientConn, "530 PORT command failed");
	}
}

static void handle_quit_command(ClientConn* clientConn)
{
	ftp_send(clientConn->controlFd, clientConn, "221 Bye Bye");
}

static void handle_pasv_command(ClientConn* clientConn)
{
	int portNo = (clientConn->passivePort[0] << 8) + clientConn->passivePort[1];

	char portNoStr[6] = "";
	sprintf(portNoStr, "%d", portNo);

	int serverFd = clientConn->server->get_server_socket_fd(portNoStr);

	char sendBuf[100] = "";
	sprintf(sendBuf,
			"227 PASV (%d,%d,%d,%d,%d,%d)",
			clientConn->ipAddr[0],
			clientConn->ipAddr[1],
			clientConn->ipAddr[2],
			clientConn->ipAddr[3],
			clientConn->passivePort[0],
			clientConn->passivePort[1]);

	ftp_send(clientConn->controlFd, clientConn, sendBuf);
	clientConn->dataFd = clientConn->server->wait_for_connection(serverFd);
	clientConn->server->conn.disconnect(serverFd);
}

static void handle_cwd_command(FtpCommand* command, ClientConn* clientConn)
{
	printf("command->args: %s\n", command->args);

	if(strstr(command->args, "..") != 0)
	{
		char* token;
		char* argStr = (char*)malloc(strlen(clientConn->currDir));
		strncpy(argStr, clientConn->currDir, strlen(clientConn->currDir));
		int noOfLevels = 0;
		while((token = strsep(&argStr, "/")) != 0)
		{
			noOfLevels++;
			printf("token: %s\n", token);
		}
		free(argStr);
		argStr = (char*)malloc(strlen(clientConn->currDir));

		printf("clientConn->currDir: %s\n", clientConn->currDir);
		strncpy(argStr, clientConn->currDir, strlen(clientConn->currDir));
		memset(clientConn->currDir, 0, 100);
		int i = 0;
		for(i = 0; i < noOfLevels; ++i)
		{
			token = strsep(&argStr, "/");
			if(strlen(token) != 0 && i < noOfLevels - 1)
			{
				strncat(clientConn->currDir, "/", 100);
				strncat(clientConn->currDir, token, 100);
			}
		}

		free(argStr);
	}
	else
	{
		strncat(clientConn->currDir, "/", 100);
		strncat(clientConn->currDir, command->args, 100);
	}

	printf("clientConn->currDir: %s\n", clientConn->currDir);
	ftp_send(clientConn->controlFd, clientConn, "250 CWD OK");
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
		case PASV:
			handle_pasv_command(clientConn);
			break;
		case CWD:
			handle_cwd_command(&command, clientConn);
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

		if(*running && -1 != clientSocketFd)
		{
			ClientConn* client = (ClientConn*)malloc(sizeof(struct ClientConn));
			client->controlFd = clientSocketFd;
			unsigned char ip[4] = {192, 168, 1, 189};
			memcpy(client->ipAddr, ip, 4);
			client->passivePort[0] = 10;
			client->passivePort[1] = 10;
			client->server = &server;
			pthread_mutex_init(&client->mutex, 0);
			pthread_cond_init(&client->cond, 0);

			thread.execute_function(&client_conn_main, client);
		}
	}

	server.conn.disconnect(serverSocketFd);
}
