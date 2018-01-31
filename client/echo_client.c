/* A simple echo client using TCP */
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define SERVER_TCP_PORT 3000	/* well-known port */
#define BUFLEN		512	/* buffer length */
#define DATALEN 100


int downloadFileFromServer(int sd,char *fileName,int strlength);
int uploadFileToServer(int sd, char *fileName, int strlength);
int changeDirectory(int sd, char *path, int strlength);
int listFiles(int sd);
int help();
int sendErrorMsg(int sd, char *errorMsg, int strlength);

struct pduData {
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
}pdu = { 'D','U','R','F','E','P','L','1','Z' };

int main(int argc, char **argv) {
	int n, i, newFile, valid;
	int sd, port;
	struct hostent *hp;
	struct sockaddr_in server;
	char *host, tmp[DATALEN], serverDir[DATALEN], cmd[DATALEN], *pt;

	switch (argc) {
	case 2:
		host = argv[1];
		port = SERVER_TCP_PORT;
		break;
	case 3:
		host = argv[1];
		port = atoi(argv[2]);
		break;
	default:
		fprintf(stderr, "Usage: %s host [port]\n", argv[0]);
		exit(1);
	}

	/* Create a stream socket	*/
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "Can't creat a socket\n");
		exit(1);
	}

	bzero((char *)&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	if (hp = gethostbyname(host))
		bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);
	else if (inet_aton(host, (struct in_addr *) &server.sin_addr)) {
		fprintf(stderr, "Can't get server's address\n");
		exit(1);
	}

	/* Connecting to the server */
	if (connect(sd, (struct sockaddr *) &server, sizeof(server)) == -1) {
		fprintf(stderr, "Can't connect \n");
		exit(1);
	}

	help();
	for (;;)
	{
		/* Client loop : Wait for command */
		read(sd, &receivedData, sizeof(struct pduData));
		printf("recv >>[%c][%d][%s]\n", receivedData.type, receivedData.length, receivedData.data);
		if (receivedData.type == pdu.ready)
		{
			printf("Enter a command : \n");
			n = read(0, cmd, DATALEN);
			for (i = 0; i < DATALEN; i++)
			{
				if (cmd[i] == ' ')
				{
					cmd[i] = '\0';
					i++;
					break;
				}
			}
			cmd[n - 1] = 0;
			pt = &cmd[i];
			strcpy(tmp, pt);
			//printf("<%s> : <%s> : <%s>\n", cmd, tmp, pt);
			
			if (strcmp(cmd, "download") == 0)
			{
				printf("in %s\n", cmd);
				valid = downloadFileFromServer(sd, tmp, n - i);
			}
			else if (strcmp(cmd, "upload") == 0)
			{
				printf("in %s\n", cmd);
				valid = uploadFileToServer(sd, tmp, n - i);
			}
			else if (strcmp(cmd, "directory") == 0)
			{
				printf("in %s\n", cmd);
				valid = changeDirectory(sd, tmp, n - i);
			}
			else if (strcmp(cmd, "listfiles") == 0)
			{
				printf("in %s\n", cmd);
				valid = listFiles(sd);
			}
			else if (strcmp(cmd, "help") == 0)
			{
				printf("in help\n", cmd);
				sendErrorMsg(sd, "H", 1);
				valid = 100;
				help();				
			}
			else if (strcmp(cmd, "exit") == 0)
			{
				printf("in %s\n", cmd);
				transmitedData.type = pdu.EOT;
				transmitedData.data[0] = '\0';
				transmitedData.length = 0;
				write(sd, &transmitedData, sizeof(transmitedData));
				exit(0);
			}
			else
			{
				printf("Invalid command! Type help for a list of valid commands.\n");
			}
		
		}
		else 
		{
			valid = -1;
		}
		
		if (valid < 0)
		{
			printf("in error\n");
			sendErrorMsg(sd, "Error receiving", strlen("Error receiving"));
		}
		printf("finished\n");
	}

	close(sd);
	return (0);
}

/*Request to download a specific file from the server*/
int downloadFileFromServer(int sd,char *fileName,int strlength)
{
	char tmp[DATALEN] = "client_new";
	int n, newfile;
	
	strcat(tmp, fileName);
	newfile= open(tmp, O_WRONLY | O_CREAT, S_IRWXU);
	
	if (newfile > 0) 
	{
		transmitedData.type = pdu.download;
		strcpy(transmitedData.data, fileName);
		transmitedData.length = strlength;

		write(sd, &transmitedData, sizeof(transmitedData));//send req

		while ((n = read(sd, &receivedData, sizeof(struct pduData))))
		{			
			if (receivedData.type == pdu.data || receivedData.type == pdu.EOT)
			{
				write(newfile, receivedData.data, receivedData.length);
				if (receivedData.type == pdu.EOT)
				{						
					close(newfile);					
					return 0;
				}
				
			}
			else
			{
				close(newfile);				
				sprintf(receivedData.data, "%s %s", "rm", tmp);
				printf("error:%s\n",receivedData.data);
				system(receivedData.data);
				return -1;
			}
		}
	}
	return -1;
}
/*Request to upload a specific file from the server*/
int uploadFileToServer(int sd, char *fileName, int strlength)
{
	int n, readFile = open(fileName, O_RDONLY);
	if (readFile > 0)
	{
		transmitedData.type = pdu.upload;
		strcpy(transmitedData.data, fileName);
		transmitedData.length = strlength;

		write(sd, &transmitedData, sizeof(transmitedData));//send req

		while ((n = read(readFile, transmitedData.data, DATALEN)) > 0)
		{
			transmitedData.type = n == DATALEN ? pdu.data : pdu.EOT;
			transmitedData.length = n;
			write(sd, &transmitedData, sizeof(transmitedData));
			if (transmitedData.type == pdu.EOT)
				break;
		}
		close(readFile);
	}
	return readFile;

}
int changeDirectory(int sd, char *path, int strlength)
{
	transmitedData.type = pdu.cd;
	strcpy(transmitedData.data, path);
	transmitedData.length = strlength;

	write(sd, &transmitedData, sizeof(transmitedData));
	return 0;

}
int listFiles(int sd)
{
	transmitedData.type = pdu.ls;
	transmitedData.data[0] = 0;
	transmitedData.length = 0;
	write(sd, &transmitedData, sizeof(transmitedData));
	while ( (read(sd, &receivedData, sizeof(struct pduData)))>0)
	{	
		printf("[%c]\n", receivedData.type);
		if (receivedData.type == pdu.lsData || receivedData.type == pdu.EOT)
		{
			write(1, receivedData.data, receivedData.length);			
			if (receivedData.type == pdu.EOT)
			{
				break;
			}
		}
		else
		{			
			return -1;
		}
	}
	printf("Finished Listing files \n\n");
	return 0;
}


int help() {
	printf("List of commands : \n");
	printf("Download a file from server : \"download [filename]\"    \n");
	printf("Upload a file to the server : \"upload [filename]\"      \n");
	printf("Change directory in server  : \"directory [path]\"       \n");
	printf("\tNote: To return to the root directory use [path] = root\n");
	printf("List all files in directory : \"listfiles\"              \n");
	printf("List of avalible commands   : \"help\"                   \n");
	printf("Terminate service           : \"exit\"                   \n");
	return 0;
}

/*Called if any error has occured*/
int sendErrorMsg(int sd, char *errorMsg, int strlength)
{
	printf("%s\n", errorMsg);
	transmitedData.type = pdu.error;
	strcpy(transmitedData.data, errorMsg);
	transmitedData.length = strlen(transmitedData.data);
	printf("[%c][%d][%s]\n", transmitedData.type, transmitedData.length, transmitedData.data);
	write(sd, &transmitedData, sizeof(transmitedData));
	return 0;
}

