// OverlappedModel.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <stdio.h>

#define ServerPort 6666
#define DATA_BUFSIZE 1024	//接收缓冲区大小
SOCKET ListenSocket, AcceptSocket[DATA_BUFSIZE] = {0};
WSABUF DataBuf[DATA_BUFSIZE];
WSAOVERLAPPED Overlapped[DATA_BUFSIZE];	//重叠结构
WSAEVENT Events[WSA_MAXIMUM_WAIT_EVENTS];	//通知重叠操作完成的事件句柄数组
DWORD dwRecvBytes = 0,	//接收到的字符长度
	Flags = 0;	//WSARecv的参数
DWORD volatile dwEventTotal = 0;	//程序中事件的总数

//由于Event数量限制，一个线程最多只能支持64个连接
DWORD WINAPI AcceptThread(LPVOID lpParameter)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2,2), &wsaData);
	ListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN ServerAddr;
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	ServerAddr.sin_port = htons(ServerPort);
	bind(ListenSocket, (LPSOCKADDR)&ServerAddr, sizeof(ServerAddr));
	listen(ListenSocket, 100);//100等待连接队列长度
	printf("listenning ...\n");
	int i = 0;
	SOCKADDR_IN ClientAddr;
	int addr_length = sizeof(ClientAddr);
	while(TRUE)
	{
		while((AcceptSocket[i] = accept(ListenSocket, (LPSOCKADDR)&ClientAddr, &addr_length)
			) != INVALID_SOCKET)
		{
			printf("accept %d ip:%s port:%d socket:%d\n", i+1, inet_ntoa(ClientAddr.sin_addr), ClientAddr.sin_port, AcceptSocket[i]);
			Events[i] = WSACreateEvent();
			dwEventTotal ++;
			memset(&Overlapped[i], 0, sizeof(WSAOVERLAPPED));
			Overlapped[i].hEvent = Events[i];
			char *buf = new char[DATA_BUFSIZE];
			memset(buf, 0, DATA_BUFSIZE);
			DataBuf[i].buf = buf;
			DataBuf[i].len = DATA_BUFSIZE;
			if(WSARecv(AcceptSocket[i], &DataBuf[i], dwEventTotal, &dwRecvBytes, &Flags, &Overlapped[i], NULL) 
				== SOCKET_ERROR)
			{
				int err = WSAGetLastError();
				printf("In Accept -- WSARecv error: %d\n", err);
				if(err != WSA_IO_PENDING)
				{
					printf("disconnect: %d\n", i+1);
					closesocket(AcceptSocket[i]);
					AcceptSocket[i] = 0;
					//WSACloseEvent(Events[i]);//关闭事件
					DataBuf[i].buf = NULL;
					DataBuf[i].len = NULL;
					continue;
				}
			}
			i = (i+1)%WSA_MAXIMUM_WAIT_EVENTS;
		}
	}
	return FALSE;
}

DWORD WINAPI ReceiveThread(LPVOID lpParameter)
{
	DWORD dwIndex = 0;
	while(TRUE)
	{
		//设置timeout防止新连接Event无法处理
		dwIndex = WSAWaitForMultipleEvents(dwEventTotal, Events, FALSE, 1000, FALSE);
		if(dwIndex == WSA_WAIT_TIMEOUT || dwIndex == WSA_WAIT_FAILED)
			continue;
		dwIndex = dwIndex - WSA_WAIT_EVENT_0;
		//printf("dwIndex = %d dwEventTotal = %d\n",dwIndex,dwEventTotal);
		WSAResetEvent(Events[dwIndex]);	//重置为未受信
		DWORD dwBytesTransferred;
		WSAGetOverlappedResult(AcceptSocket[dwIndex], &Overlapped[dwIndex], &dwBytesTransferred, FALSE, &Flags);
		if(dwBytesTransferred == 0)
		{
			printf("disconnect: %d\n", dwIndex+1);
			closesocket(AcceptSocket[dwIndex]);
			AcceptSocket[dwIndex] = 0;
			//WSACloseEvent(Events[dwIndex]);    // 关闭事件 
			DataBuf[dwIndex].buf = NULL;
			DataBuf[dwIndex].len = NULL;
			//continue;
		}
		else
		{
			//使用数据
			printf("%s\n",DataBuf[dwIndex].buf);
			memset(DataBuf[dwIndex].buf, 0, DATA_BUFSIZE);
			if(WSARecv(AcceptSocket[dwIndex], &DataBuf[dwIndex], dwEventTotal, &dwRecvBytes, &Flags, 
				&Overlapped[dwIndex], NULL) == SOCKET_ERROR)
			{
				int err = WSAGetLastError();
				printf("WSARecv error: %d\n", err);
				if(err != WSA_IO_PENDING) 
				{ 

					printf("disconnect: %d\n",dwIndex+1); 
					closesocket(AcceptSocket[dwIndex]); 
					AcceptSocket[dwIndex] = 0; 
					//WSACloseEvent(Events[dwIndex]);    // 关闭事件 
					DataBuf[dwIndex].buf = NULL; 
					DataBuf[dwIndex].len = NULL; 
					//continue; 
				} 
			}
		}
		for (int i=dwIndex+1; i<dwEventTotal; i++)
		{
			DWORD newIndex = WSAWaitForMultipleEvents(1, &Events[i], TRUE, 0, FALSE);
			if(newIndex == WSA_WAIT_FAILED || newIndex == WSA_WAIT_TIMEOUT)
				continue;
			newIndex = newIndex - WSA_WAIT_EVENT_0;
			WSAResetEvent(Events[newIndex]);	//重置为未受信
			
			BOOL iflag = WSAGetOverlappedResult(AcceptSocket[newIndex], &Overlapped[newIndex], &dwBytesTransferred, FALSE, &Flags);
			if(iflag == FALSE)
			{
				//printf("Overlapped Result Error: %d!\n", WSAGetLastError());
				//printf("newIndex = %d  socket = %d\n", newIndex, AcceptSocket[newIndex]);
			}
			else
			{
				//dwBytesTransferred指发送或接收操作实际传送的字节数
				if(dwBytesTransferred == 0)
				{
					printf("disconnect: %d\n", newIndex+1);
					closesocket(AcceptSocket[newIndex]);
					AcceptSocket[newIndex] = 0;
					//WSACloseEvent(Events[newIndex]);    // 关闭事件 
					DataBuf[newIndex].buf = NULL;
					DataBuf[newIndex].len = NULL;
					continue;
				}
				//使用数据
				printf("%s\n",DataBuf[newIndex].buf);
				memset(DataBuf[newIndex].buf, 0, DATA_BUFSIZE);
				if(WSARecv(AcceptSocket[newIndex], &DataBuf[newIndex], dwEventTotal, &dwRecvBytes, &Flags, 
					&Overlapped[newIndex], NULL) == SOCKET_ERROR)
				{
					int err = WSAGetLastError();
					printf("WSARecv error: %d\n", err);
					if(err != WSA_IO_PENDING) 
					{ 
				
						printf("disconnect: %d\n",newIndex+1); 
						closesocket(AcceptSocket[newIndex]); 
						AcceptSocket[newIndex] = 0; 
						//WSACloseEvent(Events[newIndex]);    // 关闭事件 
						DataBuf[newIndex].buf = NULL; 
						DataBuf[newIndex].len = NULL; 
						continue; 
					} 
				}
			}
		}
	}
	return FALSE;
}
int _tmain(int argc, _TCHAR* argv[])
{
	HANDLE hThread[2];
	hThread[0] = CreateThread(NULL, 0, AcceptThread, NULL, NULL, NULL);
	hThread[1] = CreateThread(NULL, 0, ReceiveThread, NULL, NULL, NULL);
	WaitForMultipleObjects(2, hThread, TRUE, INFINITE);
	printf("exit\n");
	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);
	return 0;
}

