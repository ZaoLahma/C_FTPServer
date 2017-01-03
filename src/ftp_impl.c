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
#include <stdint.h>

typedef enum UserRights
{
    READ,
    WRITE
} UserRights;

typedef struct PassivePort
{
    uint16_t portNo;
    uint8_t available;
} PassivePort;

typedef struct ClientConn
{
	int controlFd;
	int dataFd;
	int loggedIn;
	UserRights userRights;
	char transferMode;
	char userName[20];
	char currDir[256];
	char ftpRootDir[256];
	unsigned char ipAddr[4];
	uint16_t numPassivePorts;
	PassivePort* passivePorts;
	struct socket_server* server;
	ThreadStarter* threadStarter;
	pthread_mutex_t* passivePortMutex;
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
	CWD,
	TYPE,
	RETR,
	STOR,
	RMD,
	MKD,
	DELE
} FTP_COMMAND;

typedef struct FtpCommand
{
	FTP_COMMAND command;
	char args[75];
} FtpCommand;

static void ftp_send(int fd, ClientConn* clientConn, char* toSend)
{
	const unsigned int SEND_BUF_SIZE = strlen(toSend) + 3;
	char sendBuf[SEND_BUF_SIZE];
	memset(sendBuf, 0, SEND_BUF_SIZE);
	strncpy(sendBuf, toSend, SEND_BUF_SIZE);
	sendBuf[SEND_BUF_SIZE - 3] = '\r';
	sendBuf[SEND_BUF_SIZE - 2] = '\n';
	sendBuf[SEND_BUF_SIZE - 1] = '\0';

	printf("ftp_send sending: %s on file descriptor: %d\n", sendBuf, fd);

	int noOfBytesSent = clientConn->server->conn.send(fd, sendBuf, SEND_BUF_SIZE - 1);

	printf("ftp_send sent %d no of bytes\n", noOfBytesSent);
}

static FtpCommand get_command(ClientConn* clientConn)
{

	char receiveBuf[REC_BUF_SIZE] = "";

	FtpCommand command = {UNKNOWN, ""};

	int noOfBytesReceived = clientConn->server->conn.receive(clientConn->controlFd,
							receiveBuf,
							REC_BUF_SIZE);

	receiveBuf[noOfBytesReceived] = '\0';

	printf("noOfBytesReceived: %d\n", noOfBytesReceived);

	char commandStr[20];

	if(0 != noOfBytesReceived)
	{
		receiveBuf[noOfBytesReceived - 2] = '\0';

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
		else if(strcmp("PASV", commandStr) == 0)
		{
			command.command = PASV;
		}
		else if(strcmp("CWD", commandStr) == 0)
		{
			command.command = CWD;
		}
		else if(strcmp("TYPE", commandStr) == 0)
		{
			command.command = TYPE;
		}
		else if(strcmp("RETR", commandStr) == 0)
		{
            command.command = RETR;
		}
		else if(strcmp("STOR", commandStr) == 0)
		{
            command.command = STOR;
		}
		else if(strcmp("RMD", commandStr) == 0)
		{
            command.command = RMD;
		}
		else if(strcmp("MKD", commandStr) == 0)
		{
            command.command = MKD;
		}
		else if(strcmp("DELE", commandStr) == 0)
		{
            command.command = DELE;
        }

        printf("command: %s, %s\n", commandStr, command.args);
	}
	else
	{
		command.command = QUIT;
	}

	return command;
}

static void handle_user_command(FtpCommand* command, ClientConn* clientConn)
{
	strncpy(clientConn->userName, command->args, 20);
	ftp_send(clientConn->controlFd, clientConn, "330 OK, send password");
}

static void get_user(FtpCommand* command, ClientConn* clientConn)
{
    FILE* fp;
    const unsigned int BUF_SIZE = 256;
    char buffer[BUF_SIZE];

    fp = fopen("users.cfg", "r");

    char keyword[32] = "";
    char arg[256] = "";

    if(fp)
    {
        int parsingUser = 0;
        while(fgets(buffer, BUF_SIZE, (FILE*) fp)) {
            sscanf(buffer, "%s %s", keyword, arg);
            switch(parsingUser)
            {
            case 1:
                if(strncmp("END_USER", keyword, 32) == 0)
                {
                    parsingUser = 0;
                }
                else if(strncmp("HOME_DIR", keyword, 32) == 0)
                {
                    strncpy(clientConn->ftpRootDir, arg, 256);
                }
                else if(strncmp("PASSWD", keyword, 32) == 0)
                {
                    if(strncmp(arg, command->args, 256) == 0)
                    {
                        clientConn->loggedIn = 1;
                    }
                }
                else if(strncmp("RIGHTS", keyword, 32) == 0)
                {
                    if(strncmp("WRITE", arg, 256) == 0)
                    {
                        clientConn->userRights = WRITE;
                    }
                    else
                    {
                        clientConn->userRights = READ;
                    }
                }
            break;
            default:
                if(strncmp("USER", keyword, 32) == 0)
                {
                    if(strncmp(arg, clientConn->userName, 256) == 0)
                    {
                        printf("Found user: %s\n", arg);
                        parsingUser = 1;
                    }
                }
            break;
            }

        }
    }

    fclose(fp);
}

static void get_config(ClientConn* clientConn)
{
    FILE* fp;
    const unsigned int BUF_SIZE = 256;
    char buffer[BUF_SIZE];

    fp = fopen("config.cfg", "r");

    char keyword[32] = "";
    char arg[256] = "";

    if(fp)
    {
        int parsingPassive = 0;
        while(fgets(buffer, BUF_SIZE, (FILE*) fp)) {
            sscanf(buffer, "%s %s", keyword, arg);
            switch(parsingPassive)
            {
            case 1:
                if(strncmp("END_PASSIVE", keyword, 32) == 0)
                {
                    parsingPassive = 0;
                }
                else if(strncmp("ADDR", keyword, 32) == 0)
                {
                    sscanf(arg,
                           "%hhu.%hhu.%hhu.%hhu",
                           &clientConn->ipAddr[0],
                           &clientConn->ipAddr[1],
                           &clientConn->ipAddr[2],
                           &clientConn->ipAddr[3]);
                }
                else if(strncmp("PORT_RANGE", keyword, 32) == 0)
                {
                    unsigned int low;
                    unsigned int high;
                    sscanf(arg, "%u-%u", &low, &high);
                    clientConn->numPassivePorts = high - low;
                    if(clientConn->numPassivePorts == 0)
                    {
                        clientConn->numPassivePorts += 1;
                    }
                    clientConn->passivePorts = (PassivePort*)malloc(sizeof(PassivePort) * (clientConn->numPassivePorts));

                    unsigned int i = 0;
                    for(i = 0; i < clientConn->numPassivePorts; ++i)
                    {
                        clientConn->passivePorts[i].portNo = low + i;
                        clientConn->passivePorts[i].available = 1;
                    }
                }
                break;

            default:
                if(strncmp("PASSIVE", keyword, 32) == 0)
                {
                    parsingPassive = 1;
                }
                break;
            }
        }
    }

    fclose(fp);
}

static void handle_pass_command(FtpCommand* command, ClientConn* clientConn)
{
    get_user(command, clientConn);

	if(clientConn->loggedIn == 1)
	{
		strncpy(clientConn->currDir, clientConn->ftpRootDir, 256);
		get_config(clientConn);
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
	memset(sendBuf, 0, SEND_BUF_SIZE);
	strncat(sendBuf, "257 \"", SEND_BUF_SIZE);
	strncat(sendBuf, clientConn->currDir, SEND_BUF_SIZE);
	strncat(sendBuf, "\"", SEND_BUF_SIZE);

	ftp_send(clientConn->controlFd, clientConn, sendBuf);
}

static void exec_proc(ClientConn* clientConn, char* cmd, char* res)
{
    printf("Running command: %s\n", cmd);
    const unsigned int F_BUF_SIZE = 1024;

	FILE* file = popen(cmd, "r");
	char fBuffer[F_BUF_SIZE];
	memset(fBuffer, 0, F_BUF_SIZE);
	unsigned int resOffset = 0;

	while (!feof(file))
	{
		if (fgets(fBuffer, F_BUF_SIZE, file) != 0)
		{
			int lineLength = strlen(fBuffer);
			if(clientConn->transferMode == 'A')
			{
				fBuffer[lineLength - 1] = '\r';
				fBuffer[lineLength]     = '\n';
				lineLength += 1;
			}
			memcpy(&res[resOffset], fBuffer, lineLength);
			resOffset += lineLength;
			memset(fBuffer, 0, F_BUF_SIZE);
		}
	}

	pclose(file);
}

static void handle_list_command(FtpCommand* command, ClientConn* clientConn)
{
	if(-1 != clientConn->dataFd)
	{
		ftp_send(clientConn->controlFd, clientConn, "150 LIST executed ok, data follows");
		char cmd[256] = "";
		sprintf(cmd, "%s %s/%s %s", "ls -l", clientConn->currDir, command->args, "| tail -n+2");
		char res[4096] = "";
		exec_proc(clientConn, cmd, res);
		ftp_send(clientConn->dataFd, clientConn, res);
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

	unsigned int portNo = (ports[0] << 8) + ports[1];
	printf("portNo: %d\n", portNo);

	char portString[6] = "";
	sprintf(portString, "%u", portNo);

	socket_client client;
	init_client_socket(&client);

	clientConn->dataFd = client.connect(addrString, portString);

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

static PassivePort* get_free_passive_port(ClientConn* clientConn)
{
    PassivePort* retVal = 0;

    pthread_mutex_lock(clientConn->passivePortMutex);
    unsigned int i = 0;
    for(i = 0; i < clientConn->numPassivePorts; ++i)
    {
        if(clientConn->passivePorts[i].available == 1)
        {
            clientConn->passivePorts[i].available = 0;
            retVal = &clientConn->passivePorts[i];
            break;
        }
    }
    pthread_mutex_unlock(clientConn->passivePortMutex);
    return retVal;
}

static void return_passive_port(PassivePort* port, ClientConn* clientConn)
{

    pthread_mutex_lock(clientConn->passivePortMutex);
    port->available = 1;
    pthread_mutex_unlock(clientConn->passivePortMutex);
}

static void handle_pasv_command(ClientConn* clientConn)
{
	unsigned char passivePortHigh;
	unsigned char passivePortLow;
    PassivePort* passivePortPtr = get_free_passive_port(clientConn);
    passivePortLow  = passivePortPtr->portNo & 0x00ff;
    passivePortHigh = passivePortPtr->portNo >> 8;

	char portNoStr[6] = "";
	sprintf(portNoStr, "%d", passivePortPtr->portNo);

	int serverFd = clientConn->server->get_server_socket_fd(portNoStr);

	char sendBuf[100] = "";
	memset(sendBuf, 0, 100);
	sprintf(sendBuf,
			"227 PASV (addr:%u,%u,%u,%u,%u,%u)",
			clientConn->ipAddr[0],
			clientConn->ipAddr[1],
			clientConn->ipAddr[2],
			clientConn->ipAddr[3],
			passivePortHigh,
			passivePortLow);

	ftp_send(clientConn->controlFd, clientConn, sendBuf);
	clientConn->dataFd = clientConn->server->wait_for_connection(serverFd);
	clientConn->server->conn.disconnect(serverFd);
	return_passive_port(passivePortPtr, clientConn);
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
        if(strstr(command->args, clientConn->ftpRootDir) == 0)
        {
            strncat(clientConn->currDir, "/", 100);
            strncat(clientConn->currDir, command->args, 100);
		}
		else
		{
            printf("Client sends full path\n");
            strncpy(clientConn->currDir, command->args, 100);
		}
	}

	if(strstr(clientConn->currDir, clientConn->ftpRootDir) != 0)
	{
        printf("clientConn->currDir: %s\n", clientConn->currDir);
        ftp_send(clientConn->controlFd, clientConn, "250 CWD OK");
	}
	else
	{
        strncpy(clientConn->currDir, clientConn->ftpRootDir, 256);
        ftp_send(clientConn->controlFd, clientConn, "550 CWD permission denied. Not allwed to CWD out of ftp root dir");
	}
}

static void handle_type_command(FtpCommand* command, ClientConn* clientConn)
{
	if(strcmp("A", command->args) == 0)
	{
		clientConn->transferMode = 'A';
		ftp_send(clientConn->controlFd, clientConn, "200 TYPE changed to A");
	}
	else if(strcmp("I", command->args) == 0)
	{
		clientConn->transferMode = 'I';
		ftp_send(clientConn->controlFd, clientConn, "200 TYPE change to I");
	}
	else
	{
		ftp_send(clientConn->controlFd, clientConn, "501 not implemented");
	}
}

typedef struct FileTransferData
{
    char filePath[256];
    ClientConn* clientConn;
} FileTransferData;

static void* file_retr_func(void* arg)
{
    FileTransferData* ftData = (FileTransferData*)arg;
    FILE* file = fopen(ftData->filePath, "r");

    const int BUF_SIZE = 1024;
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);

    int readBytes = 0;

    if(file)
    {
        ftp_send(ftData->clientConn->controlFd, ftData->clientConn, "150 RETR OK, data follows");

        while((readBytes = fread(buf, 1, BUF_SIZE, file)) > 0)
        {
            if('A'  == ftData->clientConn->transferMode)
            {
                int index = 0;
                for(index = 0; index < readBytes; ++index)
                {
                    if('\n' == buf[index]) //Append a \r to each new line
                    {
                        char carrRet = '\r';
                        ftData->clientConn->server->conn.send(ftData->clientConn->dataFd, &carrRet, 1);
                    }
                    ftData->clientConn->server->conn.send(ftData->clientConn->dataFd, &buf[index], 1);
                }
            }
            else
            {
                ftData->clientConn->server->conn.send(ftData->clientConn->dataFd, buf, readBytes);
            }
        }

        fclose(file);

        ftData->clientConn->server->conn.disconnect(ftData->clientConn->dataFd);
        ftp_send(ftData->clientConn->controlFd, ftData->clientConn, "226 RETR FINISHED");
    }
    else
    {
        ftData->clientConn->server->conn.disconnect(ftData->clientConn->dataFd);
        ftp_send(ftData->clientConn->controlFd, ftData->clientConn, "451 RETR FAILED");
    }

    free(ftData);

    return 0;
}

static void handle_retr_command(FtpCommand* command, ClientConn* clientConn)
{
    FileTransferData* ftData = (FileTransferData*)malloc(sizeof(FileTransferData));
    snprintf(ftData->filePath, 256, "%s/%s", clientConn->currDir, command->args);
    ftData->clientConn = clientConn;
    clientConn->threadStarter->execute_function(&file_retr_func, ftData);
}

static void* file_stor_func(void* arg)
{
    FileTransferData* ftData = (FileTransferData*)arg;

    ftp_send(ftData->clientConn->controlFd, ftData->clientConn, "150 STOR OK, send data");

    FILE* file = fopen(ftData->filePath, "w");

    const unsigned int BUF_SIZE = 1024;
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);

    int readBytes = 0;

    while((readBytes = ftData->clientConn->server->conn.receive(ftData->clientConn->dataFd, buf, BUF_SIZE)) != 0)
    {
        if('A' != ftData->clientConn->transferMode)
        {
            fwrite(buf, readBytes, 1, file);
        }
        else
        {
            // Strip away all \r from the file
            int index = 0;
            for(index = 0; index < readBytes; index++)
            {
                if('\r' != buf[index])
                {
                    fwrite(&buf[index], 1, 1, file);
                }
            }
        }
        memset(buf, 0, BUF_SIZE);
    }

    fclose(file);


    ftData->clientConn->server->conn.disconnect(ftData->clientConn->dataFd);
    ftp_send(ftData->clientConn->controlFd, ftData->clientConn, "226 STOR OK");

    free(ftData);

    return 0;
}

static void handle_stor_command(FtpCommand* command, ClientConn* clientConn)
{
    if(clientConn->userRights == WRITE)
    {
        FileTransferData* ftData = (FileTransferData*)malloc(sizeof(FileTransferData));
        snprintf(ftData->filePath, 256, "%s/%s", clientConn->currDir, command->args);
        ftData->clientConn = clientConn;
        clientConn->threadStarter->execute_function(&file_stor_func, ftData);
    }
    else
    {
        ftp_send(clientConn->controlFd, clientConn, "553 STOR permission denied due to user rights set to READ");
        clientConn->server->conn.disconnect(clientConn->dataFd);
    }
}

static void handle_rmd_command(FtpCommand* command, ClientConn* clientConn)
{
    if(clientConn->userRights == WRITE)
    {
        char res[1024];
        char cmd[300] = "";

        sprintf(cmd, "rmdir %s/%s", clientConn->currDir, command->args);

        exec_proc(clientConn, cmd, res);

        ftp_send(clientConn->controlFd, clientConn, "250 RMD OK");
    }
    else
    {
        ftp_send(clientConn->controlFd, clientConn, "550 RMD permission denied due to user rights set to READ");
    }
}

static void handle_mkd_command(FtpCommand* command, ClientConn* clientConn)
{
    if(clientConn->userRights == WRITE)
    {
        char res[1024];
        char cmd[300] = "";

        sprintf(cmd, "mkdir %s/%s", clientConn->currDir, command->args);

        exec_proc(clientConn, cmd, res);

        ftp_send(clientConn->controlFd, clientConn, "250 MKD OK");
    }
    else
    {
        ftp_send(clientConn->controlFd, clientConn, "550 MKD permission denied due to user rights set to READ");
    }
}

static void handle_dele_command(FtpCommand* command, ClientConn* clientConn)
{
    if(clientConn->userRights == WRITE)
    {
        char res[1024];
        char cmd[300] = "";

        sprintf(cmd, "rm %s/%s", clientConn->currDir, command->args);

        exec_proc(clientConn, cmd, res);

        ftp_send(clientConn->controlFd, clientConn, "250 DELE OK");
    }
    else
    {
        ftp_send(clientConn->controlFd, clientConn, "550 DELE permission denied due to user rights set to READ");
    }
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

        switch(clientConn->loggedIn)
        {
        case 1:
            switch(command.command)
            {
            case QUIT:
                handle_quit_command(clientConn);
                running = 0;
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
            case TYPE:
                handle_type_command(&command, clientConn);
                break;
            case RETR:
                handle_retr_command(&command, clientConn);
                break;
            case STOR:
                handle_stor_command(&command, clientConn);
                break;
            case RMD:
                handle_rmd_command(&command, clientConn);
                break;
            case MKD:
                handle_mkd_command(&command, clientConn);
                break;
            case DELE:
                handle_dele_command(&command, clientConn);
                break;
            default:
                ftp_send(clientConn->controlFd, clientConn, "500 - Not implemented");
                break;
            }
            break;
        default:
            switch(command.command)
            {
            case USER:
                handle_user_command(&command, clientConn);
                break;
            case PASS:
                handle_pass_command(&command, clientConn);
                break;
            case SYST:
                handle_syst_command(clientConn);
                break;
            case QUIT:
                handle_quit_command(clientConn);
                running = 0;
                break;
            default:
                ftp_send(clientConn->controlFd, clientConn, "530 - Not logged in");
                break;
            }
            break;
        }
	}

	clientConn->server->conn.disconnect(clientConn->controlFd);

	printf("Disconnected client with fd: %d\n", clientConn->controlFd);

    free(clientConn->passivePorts);
	free(clientConn);

	return 0;
}

void run_ftp(int* running, char* port)
{
	printf("FTP server starting, running: %d\n", *running);
	struct socket_server server;
	init_server_socket(&server);

	int serverSocketFd = server.get_server_socket_fd(port);
	int clientSocketFd = -1;

	ThreadStarter thread;
	init_thread_starter(&thread, POOL);

	pthread_mutex_t passiveMutex;
	pthread_mutex_init(&passiveMutex, 0);

	while(*running)
	{
		clientSocketFd = server.wait_for_connection(serverSocketFd);

		if(*running && -1 != clientSocketFd)
		{
			ClientConn* client = (ClientConn*)malloc(sizeof(struct ClientConn));
			client->controlFd = clientSocketFd;
			client->loggedIn = 0;
			client->userRights = READ;
			client->transferMode = 'A';
			client->server = &server;
			client->threadStarter = &thread;
			client->passivePorts = 0;
			client->passivePortMutex = &passiveMutex;

			thread.execute_function(&client_conn_main, client);
		}
	}

	server.conn.disconnect(serverSocketFd);
}
