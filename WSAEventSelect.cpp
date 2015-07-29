#include <Winsock2.h>
#include <stdio.h>
#include <windows.h>
#include <math.h>

#define PORT 5250
#define MSGSIZE 1024
#pragma comment(lib, "ws2_32.lib")


int main()
{
    //Init
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);


    WSAEVENT events[WSA_MAXIMUM_WAIT_EVENTS];
    SOCKET socks[WSA_MAXIMUM_WAIT_EVENTS];

    int nEventTotal = 0;

    //Init Socket
    SOCKET sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);
    sin.sin_addr.S_un.S_addr = INADDR_ANY;
    if(bind(sListen, (sockaddr*)&sin, sizeof(sin)) == SOCKET_ERROR)
    {
        printf("bind error!");
        return 0;
    }
    listen(sListen, 5);

    //Create Event and bind to socket
    WSAEVENT event = WSACreateEvent();
    int flag = WSAEventSelect(sListen, event, FD_ACCEPT|FD_CLOSE);//listen for accept and close event
    if(flag == 0)
        printf("EventSelect OK\n");
    else
        return 0;
    //Add to Array
    events[nEventTotal] = event;
    socks[nEventTotal] = sListen;
    nEventTotal++;

    while(true)
    {
        //Wait for all events
        int nIndex = WSAWaitForMultipleEvents(nEventTotal, events, false, WSA_INFINITE, false);
        nIndex -= WSA_WAIT_EVENT_0;

        /******* 不循环处理 *******/
        /*
        if(nIndex == WSA_WAIT_FAILED)
        {
            printf("Failed %d\n",WSAGetLastError());
            continue;
        }
        else if(nIndex == WSA_WAIT_TIMEOUT)
        {
            printf("Timeout!\n");
            continue;
        }
        else
        {
            WSANETWORKEVENTS netEvent;
            WSAEnumNetworkEvents(socks[nIndex], events[nIndex], &netEvent);
            //Deal with accept event
            if(netEvent.lNetworkEvents & FD_ACCEPT)
            {
                //Check Accept_bit error
                if(netEvent.iErrorCode[FD_ACCEPT_BIT] == 0)
                {
                    if(nEventTotal > WSA_MAXIMUM_WAIT_EVENTS)
                    {
                        printf("Too many connections!");
                        continue;
                    }
                    SOCKET sNew = accept(socks[nIndex], NULL, NULL);
                    WSAEVENT sEvent = WSACreateEvent();
                    WSAEventSelect(sNew, sEvent, FD_READ|FD_CLOSE|FD_WRITE);
                    //Add to array
                    events[nEventTotal] = sEvent;
                    socks[nEventTotal] = sNew;
                    nEventTotal++;
                    printf("Socket %d connect\n",sNew);
                }
                else
                    printf("Accept bit error!\n");
            }
            //Deal with read event
            else if(netEvent.lNetworkEvents & FD_READ)
            {
                //Check read bit error
                if(netEvent.iErrorCode[FD_READ_BIT] == 0)
                {
                    char szText[MSGSIZE];
                    int nRecv = recv(socks[nIndex], szText, MSGSIZE, 0);
                    if(nRecv > 0)
                    {
                        szText[nRecv] = '\0';
                        printf("Recv data: %s", szText);
                    }
                }
                else
                    printf("Read bit error!\n");
            }
            //Deal with close event
            else if(netEvent.lNetworkEvents & FD_CLOSE)
            {
                //Check read bit error
                if(netEvent.iErrorCode[FD_CLOSE_BIT] == 0)
                {
                    closesocket(socks[nIndex]);
                    for(int j=nIndex; j<nEventTotal-1; j++)
                    {
                        socks[j] = socks[j+1];
                        events[j] = events[j+1];
                    }
                    nEventTotal--;
                    printf("socket %d closed!\n",nIndex);
                }
                else
                    printf("Close bit error!\n");
            }
        }
        */

        /******* 循环处理 *******/
        for(int i=nIndex; i<nEventTotal; i++)
        {
            //timeout(ms)
            nIndex = WSAWaitForMultipleEvents(1, &events[i], true, 10, false);
            if(nIndex == WSA_WAIT_FAILED)
            {
                //printf("Failed %d\n",WSAGetLastError());
                continue;
            }
            else if(nIndex == WSA_WAIT_TIMEOUT)
            {
                //printf("Timeout!\n");
                continue;
            }
            else
            {
                WSANETWORKEVENTS netEvent;
                WSAEnumNetworkEvents(socks[i], events[i], &netEvent);
                //Deal with accept event
                if(netEvent.lNetworkEvents & FD_ACCEPT)
                {
                    //Check Accept_bit error
                    if(netEvent.iErrorCode[FD_ACCEPT_BIT] == 0)
                    {
                        if(nEventTotal > WSA_MAXIMUM_WAIT_EVENTS)
                        {
                            printf("Too many connections!");
                            continue;
                        }
                        SOCKET sNew = accept(socks[i], NULL, NULL);
                        WSAEVENT sEvent = WSACreateEvent();
                        WSAEventSelect(sNew, sEvent, FD_READ|FD_CLOSE|FD_WRITE);
                        //Add to array
                        events[nEventTotal] = sEvent;
                        socks[nEventTotal] = sNew;
                        nEventTotal++;
                        printf("Socket %d connect\n",sNew);
                    }
                    else
                        printf("Accept bit error!\n");
                }
                //Deal with read event
                else if(netEvent.lNetworkEvents & FD_READ)
                {
                    //Check read bit error
                    if(netEvent.iErrorCode[FD_READ_BIT] == 0)
                    {
                        char szText[MSGSIZE];
                        int nRecv = recv(socks[i], szText, MSGSIZE, 0);
                        if(nRecv > 0)
                        {
                            szText[nRecv] = '\0';
                            printf("Recv data: %s", szText);
                        }
                    }
                    else
                        printf("Read bit error!\n");
                }
                //Deal with close event
                else if(netEvent.lNetworkEvents & FD_CLOSE)
                {
                    //Check read bit error
                    if(netEvent.iErrorCode[FD_CLOSE_BIT] == 0)
                    {
                        closesocket(socks[i]);
                        for(int j=i; j<nEventTotal-1; j++)
                        {
                            socks[j] = socks[j+1];
                            events[j] = events[j+1];
                        }
                        nEventTotal--;
                        printf("socket %d closed!\n",i);
                    }
                    else
                        printf("Close bit error!\n");
                }
            }
        }
    }

    return 0;
}

