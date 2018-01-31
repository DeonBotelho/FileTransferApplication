/* A simple echo server using TCP */

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <sys/signal.h>
#include <sys/wait.h> 

#define SERVER_TCP_PORT 3000	/* well-known port */
#define BUFLEN		512	/* buffer length */
#define DATALEN 100

int echod(int);
void reaper(int);

int uploadFiletoClient(int sd,char *fileName,int strlength);
int downloadFileFromClient(int sd, char *fileName, int strlength);
void sendReady(int sd);
void sendErrorMsg(int sd, char *errorMsg, int strlength);
int changeDirectory(char *path, int strlength);
int listFiles(int sd);

char homepath[BUFLEN], clientpath[BUFLEN],subpath[BUFLEN] ="root";

struct pduData
{
	char type;
	int length;
	char data[DATALEN];
} receivedData, transmitedData;
struct pduType {
	char download;//D:client>>server request to download specified file
	char upload;  //U:client>>server request to upload specified file
	char ready;   //R:server>>client ready to recieved a request from client
	char data;    //F:client<>server response data sent and received 
	char error;   //E:client<>server error has occurred
	char cd;      //P:client>>server request to change directory
	char ls;      //L:client>>server request a list of all files in current directory
	char lsData;  //1:server>>client response to request for list of files
	char EOT;     //Z:client<>server end of transmission
}pdu = { 'D','U','R','F','E','P','L','1','Z'};

int main(int argc, char **argv) {
	int sd, new_sd, client_len, port;
	struct sockaddr_in server, client;

	switch (argc) {
	case 1:
		port = SERVER_TCP_PORT;
		break;
	case 2:
		port = atoi(argv[1]);
		break;
	default:
		fprintf(stderr, "Usage: %d [port]\n", argv[0]);
		exit(1);
	}

	/* Create a stream socket	*/
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "Can't creat a socket\n");
		exit(1);
	}

	/* Bind an address to the socket	*/
	bzero((char *) &server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sd, (struct sockaddr *) &server, sizeof(server)) == -1) {
		fprintf(stderr, "Can't bind name to socket\n");
		exit(1);
	}

	/* queue up to 5 connect requests  */
	listen(sd, 5);

	(void) signal(SIGCHLD, reaper);

	while (1) {
		client_len = sizeof(client);
		new_sd = accept(sd, (struct sockaddr *) &client, &client_len);
		if (new_sd < 0) {
			fprintf(stderr, "Can't accept client \n");
			exit(1);
		}
		switch (fork()) {
		case 0: /* child */
			(void) close(sd);
			exit(echod(new_sd));
		default: /* parent */
			(void) close(new_sd);
			break;
		case -1:
			fprintf(stderr, "fork: error\n");
		}
	}
}

/*	echod program	*/
int echod(int sd) {
	int n, valid;		
	getcwd(homepath, BUFLEN);
	strcpy(clientpath, homepath);	
	printf("\nNew Client %d connected to : \n\t%s\n\n",sd, clientpath);
	for (;;) 
	{		
		printf("Server connected: ");
		sendReady(sd);
		printf("Ready sent\n");
		n = read(sd,&receivedData, sizeof(struct pduData));//receive cmd from server
	
		if (receivedData.type == pdu.download)
		{
			valid = uploadFiletoClient(sd, receivedData.data, receivedData.length);
		}
		else if (receivedData.type == pdu.upload)
		{
			valid = downloadFileFromClient(sd, receivedData.data, receivedData.length);
		}
		else if (receivedData.type == pdu.cd) 
		{
			valid = changeDirectory(receivedData.data, receivedData.length);
		}
		else if (receivedData.type == pdu.ls) 
		{
			valid = listFiles(sd);
		}
		else if (receivedData.type == pdu.EOT)
		{
			break;
		}			
		else if (receivedData.type == pdu.error) 
		{			
			printf("error recived :%s\n",receivedData.data);
		}
		else//error
		{
			valid = -1;
		}
		if (valid<0)//valid < 0
		{
			printf("error\n");
			sendErrorMsg(sd, "Error processing request", strlen("Error processing request"));
		}		
	}

	close(sd);
	return 0;
}
/*	reaper		*/
void reaper(int sig) {
	int status;
	while (wait3(&status, WNOHANG, (struct rusage *) 0) >= 0);
}
/*Called if client wishes to download a file from the server*/
int uploadFiletoClient(int sd, char *fileName, int strlength)
{
	int n, readFile;// = open(fileName, O_RDONLY);
	char filepath[BUFLEN];
	sprintf(filepath, "%s/%s", clientpath, fileName);
	readFile = open(filepath, O_RDONLY);	
	if (readFile > 0)
	{
		printf("opening : %s\n ", filepath);
		while ((n = read(readFile, transmitedData.data, DATALEN)) > 0)
		{
			transmitedData.type = n==DATALEN?pdu.data:pdu.EOT;
			transmitedData.length = n;
			write(sd, &transmitedData, sizeof(transmitedData));
			if (transmitedData.type == pdu.EOT)
				break;
		}
		close(readFile);
	}	
	return readFile;
}

/*Called if the client wishes to upload to the server*/
int downloadFileFromClient(int sd, char *fileName, int strlength)
{
	char filepath[BUFLEN];
	int n, writeFile;

	sprintf(filepath, "%s/%s%s", clientpath,"server_new", fileName);
	writeFile = open(filepath, O_WRONLY | O_CREAT, S_IRWXU);
	if (writeFile > 0)
	{
		printf("opening : %s\n ", filepath);
		while ((n = read(sd, &receivedData, sizeof(struct pduData))) > 0)
		{
			if (receivedData.type == pdu.data || receivedData.type == pdu.EOT)
			{
				write(writeFile, receivedData.data, receivedData.length);
				if (receivedData.type == pdu.EOT)
					break;
			}
			else
			{
				close(writeFile);
				sprintf(receivedData.data, "%s %s", "rm", filepath);
				system(receivedData.data);
				return -1;
			}
		}
		close(writeFile);
	}
	return writeFile;		
}

/*Called to tell client,server is ready for a cmd*/
void sendReady(int sd)
{
	transmitedData.type = pdu.ready;
	strcpy(transmitedData.data, "ready");
	transmitedData.length = strlen(transmitedData.data);

	write(sd, &transmitedData, sizeof(transmitedData));
}

/*Called if any error has occured*/
void sendErrorMsg(int sd, char *errorMsg, int strlength)
{
	transmitedData.type = pdu.error;
	strcpy(transmitedData.data, errorMsg);
	transmitedData.length = strlen(transmitedData.data);

	write(sd, &transmitedData, sizeof(transmitedData));	
}
/*Called if the client wishes to change the current directory*/
int changeDirectory(char *path, int strlength)
{
	char cmd[BUFLEN] = "cd ";
	if (strcmp(path, "root") == 0)
	{
		strcpy(clientpath, homepath);
		strcpy(subpath, path);
		printf("New path : %s\n", clientpath);
		return 0;
	}
	else
	{
		strcat(cmd, path);
		if (system(cmd) == 0)//is the path valid?
		{
			sprintf(clientpath, "%s/%s", clientpath, path);
			strcpy(subpath, path);
			printf("New path : %s\n", clientpath);
			return 0;
		}
	}
	return -1;
}
/*Called if the client wishes to know what is in the current directory*/
int listFiles(int sd)
{
	char cmd[BUFLEN],tmp[BUFLEN];
	int tmpFile, valid, n;
	printf("in listfiles: %s\n%s\n",subpath,clientpath);
	if (strcmp(subpath, "root") != 0)
	{
		sprintf(cmd, "%s %s %s", "cd", subpath, "&& ls -Q>> tmp_ls_file.txt");
	}
	else
	{
		strcpy(cmd, "ls -Q>> tmp_ls_file.txt");
	}

	printf("%s\n", cmd);
	valid = system(cmd);	
	sprintf(tmp, "%s/%s", clientpath, "tmp_ls_file.txt");
	tmpFile = open(tmp, O_RDONLY);

	if (valid >= 0 && tmpFile > 0)
	{
		printf("tmp file open\n");
		while ((n = read(tmpFile, transmitedData.data, DATALEN))>=0)
		{
			transmitedData.type = n == DATALEN ? pdu.lsData : pdu.EOT;
			transmitedData.length = n;			
			write(sd, &transmitedData, sizeof(transmitedData));			
			printf("[%c]", transmitedData.type);
			write(1, transmitedData.data, transmitedData.length);
			if (transmitedData.type == pdu.EOT)
			{
				printf("done");
				close(tmpFile);
				break;
			}
		}

	}
	if (strcmp(subpath, "root") != 0)
	{
		sprintf(cmd, "%s %s %s", "cd", subpath, "&& rm tmp_ls_file.txt");
	}
	else
	{
		strcpy(cmd, "rm tmp_ls_file.txt");
	}
	system(cmd);	
	return valid;
}
