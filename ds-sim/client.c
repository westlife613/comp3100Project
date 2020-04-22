#include <stdio.h>
#include <ctype.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h> //For the AF_INET (Address Family)
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h> 	// For fork
#include <pwd.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <libxml/parser.h>

#define SERVER_PORT 50000
#define MAX_BUF 100
#define DEBUG 0

int sock;
char sendbuf[100], recvbuf[100];
char largestType[20];
void SendMsg(char *msg)
{
    strcpy(sendbuf, msg);
    //sendbuf[strlen(sendbuf)] = 0;
    if (DEBUG)
        printf("send: %s\n", sendbuf);
	if ((send(sock, msg, strlen(msg), 0)) < 0) {
        perror("send");
        exit(1);
	}
}

void RecvMsg(const char *msg)
{
	int recvCnt = read(sock, recvbuf, MAX_BUF - 1);
    recvbuf[recvCnt] = 0;
    if (DEBUG)
        printf("recv: %s\n", recvbuf);
    if (strncmp(recvbuf, msg, strlen(msg)))
    {
    printf("recv: %s %s\n", msg, recvbuf);
        perror("recv");
        exit(1);
    }
}

void ReadSimConfig(char *filename)
{
	xmlDoc *doc = NULL;
    xmlNode *rootNode, *curNode;
    xmlChar* type;
    int best = 0, mem;

	doc = xmlReadFile(filename, NULL, 0);
	assert(doc);
	rootNode = xmlDocGetRootElement(doc);
    curNode = rootNode->xmlChildrenNode;
    while(curNode != NULL)
    {
       if ((!xmlStrcmp(curNode->name, (const xmlChar *)"servers")))
       {
            curNode = curNode->xmlChildrenNode;
            continue;
        }
       if ((!xmlStrcmp(curNode->name, (const xmlChar *)"server")))
       {
           xmlChar* memstr = xmlGetProp(curNode,BAD_CAST "memory");
           mem = atoi(memstr);
           if (mem > best) {
                best = mem;
                type = xmlGetProp(curNode,BAD_CAST "type");
            }
           xmlFree(memstr);
       }
       curNode = curNode->next;
    }
    strcpy(largestType, type);
    xmlFree(type);

	xmlFreeDoc(doc);
}

void resc(int index)
{
    SendMsg(sendbuf);
    RecvMsg("DATA");
    SendMsg("OK");
    while(1) {
        int recvCnt = read(sock, recvbuf, MAX_BUF - 1);
        recvbuf[recvCnt] = 0;
        if (DEBUG)
            printf("recv: %s\n", recvbuf);
        if (!strncmp(recvbuf, "ERR", 3))
            exit(1);
        if (!strcmp(recvbuf, ".")) {
            sprintf(sendbuf, "SCHD %d %s 0", index, largestType);
            SendMsg(sendbuf);
            RecvMsg("OK");
            return;
        }
        SendMsg("OK");
    }
}

void sendJob(char *filename)
{
	xmlDoc *doc = NULL;
    xmlNode *rootNode, *curNode;
    xmlChar *corestr, *memstr, *diskstr;
    int best = 0, mem;
    int index = 0;

	doc = xmlReadFile(filename, NULL, 0);
	assert(doc);
	rootNode = xmlDocGetRootElement(doc);
    curNode = rootNode->xmlChildrenNode;
    while(curNode != NULL)
    {
       if ((!xmlStrcmp(curNode->name, (const xmlChar *)"jobs")))
       {
        printf("jobs\n");
            curNode = curNode->xmlChildrenNode;
            continue;
        }
       if ((!xmlStrcmp(curNode->name, (const xmlChar *)"job")))
       {
            SendMsg("REDY");
            RecvMsg("JOBN");
           corestr = xmlGetProp(curNode,BAD_CAST "cores");
           memstr = xmlGetProp(curNode,BAD_CAST "memory");
           diskstr = xmlGetProp(curNode,BAD_CAST "disk");
            sprintf(sendbuf, "RESC All %s %s %s", corestr, memstr, diskstr);
            resc(index++);
       }
       curNode = curNode->next;
    }
       xmlFree(corestr);
       xmlFree(memstr);
       xmlFree(diskstr);

	xmlFreeDoc(doc);
}
int main(int argc, char **argv)
{
    struct sockaddr_in serverAddr;
    int size;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return 1;
    }
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("connet");
        return 1;
    }
    //HELO
    SendMsg("HELO");
    RecvMsg("OK");
    //AUTH
    struct passwd *pwd;
    char uname[20];
    pwd = getpwuid(getuid());
    sprintf(sendbuf, "AUTH %s", pwd->pw_name);
    SendMsg(sendbuf);
    RecvMsg("OK");

    ReadSimConfig("system.xml");
    
    sendJob("ds-jobs.xml");

    SendMsg("REDY");
    RecvMsg("NONE");
    SendMsg("QUIT");
    RecvMsg("QUIT");

    return 0;
}

