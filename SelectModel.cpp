#include <QCoreApplication>
#include <winsock.h>
#include <stdio.h>
#include <windows.h>
#include <math.h>

#define PORT 5150
#define MSGSIZE 1024
#pragma comment(lib, "ws2_32.lib")
int g_iTotalConn = 0;
int maxClienId;

//FD_SETSIZE = 1024  /usr/include/linux/posix_types.h
SOCKET g_CliSockets[FD_SETSIZE];
DWORD WINAPI WorkerThread(LPVOID lpParameter);
int main(int argc, char *argv[])
{
    WSADATA wsaData;
    SOCKET sListen, sClient;
    SOCKADDR_IN local, client;
    int iaddrSize = sizeof(SOCKADDR_IN);
    DWORD dwThreadId;
    WSAStartup(MAKEWORD(2,2), &wsaData);
    
    for(int i=0; i<FD_SETSIZE; i++)
        g_CliSockets[i] = -1;
    maxClienId = -1;

    sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    local.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    local.sin_family = AF_INET;
    local.sin_port = htons(PORT);
    bind(sListen, (struct sockaddr *)&local, sizeof(SOCKADDR_IN));
    
    if(listen(sListen, 3) == -1)
        exit(0);
    CreateThread(NULL, 0, WorkerThread, NULL, 0, &dwThreadId);
    while(true)
    {
        sClient = accept(sListen, (struct sockaddr *)&client, &iaddrSize);
        //maxClienId = max(maxClienId, sClient);
        printf("Accepted client:%s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
        g_CliSockets[g_iTotalConn++] = sClient;
    }
    WSACleanup();
    return 0;
}

DWORD WINAPI WorkerThread(LPVOID lpParam)
{
    int i;
    fd_set fdread;
    int ret;
    int socketId;
    struct timeval tv = {1,0};
    char szMessage[MSGSIZE];
    
    while(true)
    {
        FD_ZERO(&fdread);
        for(i=0; i<g_iTotalConn; i++)
            FD_SET(g_CliSockets[i], &fdread);
        //select阻塞
        ret = select(maxClienId+1, &fdread, NULL, NULL, &tv);
        if(ret == 0)//timeout
            continue;
        for(i=0; i<g_iTotalConn; i++)
        {
            if((socketId = g_CliSockets[i]) < 0)
                continue;
            if(FD_ISSET(socketId, &fdread))//client[i] is ready for read
            {
                ret = recv(socketId, szMessage, MSGSIZE, 0);
                if(ret == 0 || (ret == SOCKET_ERROR && WSAGetLastError() == WSAECONNRESET))
                {
                    //client closed
                    printf("Client socket  %d closed.\n", socketId);
                    closesocket(socketId);
                    if(i < g_iTotalConn - 1)
                        g_CliSockets[i--] = g_CliSockets[--g_iTotalConn];
                    else
                        --g_iTotalConn;
                }
                else
                {
                    szMessage[ret] = '\0';
                    printf("recv message from client %d.\n",socketId);
                    send(socketId, szMessage, strlen(szMessage), 0);
                }
            }
        }
    }
    return 0;
}
