// OverlappedModel.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <stdio.h>

#define ServerPort 6666
#define DATA_BUFSIZE 1024	//���ջ�������С
SOCKET ListenSocket, AcceptSocket[DATA_BUFSIZE] = {0};
WSABUF DataBuf[DATA_BUFSIZE];
WSAOVERLAPPED Overlapped[DATA_BUFSIZE];	//�ص��ṹ
WSAEVENT Events[WSA_MAXIMUM_WAIT_EVENTS];	//֪ͨ�ص�������ɵ��¼��������
DWORD dwRecvBytes = 0,	//���յ����ַ�����
	Flags = 0;	//WSARecv�Ĳ���
DWORD volatile dwEventTotal = 0;	//�������¼�������

//����Event�������ƣ�һ���߳����ֻ��֧��64������
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
	listen(ListenSocket, 100);//100�ȴ����Ӷ��г���
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
					//WSACloseEvent(Events[i]);//�ر��¼�
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
		//����timeout��ֹ������Event�޷�����
		dwIndex = WSAWaitForMultipleEvents(dwEventTotal, Events, FALSE, 1000, FALSE);
		if(dwIndex == WSA_WAIT_TIMEOUT || dwIndex == WSA_WAIT_FAILED)
			continue;
		dwIndex = dwIndex - WSA_WAIT_EVENT_0;
		//printf("dwIndex = %d dwEventTotal = %d\n",dwIndex,dwEventTotal);
		WSAResetEvent(Events[dwIndex]);	//����Ϊδ����
		DWORD dwBytesTransferred;
		WSAGetOverlappedResult(AcceptSocket[dwIndex], &Overlapped[dwIndex], &dwBytesTransferred, FALSE, &Flags);
		if(dwBytesTransferred == 0)
		{
			printf("disconnect: %d\n", dwIndex+1);
			closesocket(AcceptSocket[dwIndex]);
			AcceptSocket[dwIndex] = 0;
			//WSACloseEvent(Events[dwIndex]);    // �ر��¼� 
			DataBuf[dwIndex].buf = NULL;
			DataBuf[dwIndex].len = NULL;
			//continue;
		}
		else
		{
			//ʹ������
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
					//WSACloseEvent(Events[dwIndex]);    // �ر��¼� 
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
			WSAResetEvent(Events[newIndex]);	//����Ϊδ����
			
			BOOL iflag = WSAGetOverlappedResult(AcceptSocket[newIndex], &Overlapped[newIndex], &dwBytesTransferred, FALSE, &Flags);
			if(iflag == FALSE)
			{
				//printf("Overlapped Result Error: %d!\n", WSAGetLastError());
				//printf("newIndex = %d  socket = %d\n", newIndex, AcceptSocket[newIndex]);
			}
			else
			{
				//dwBytesTransferredָ���ͻ���ղ���ʵ�ʴ��͵��ֽ���
				if(dwBytesTransferred == 0)
				{
					printf("disconnect: %d\n", newIndex+1);
					closesocket(AcceptSocket[newIndex]);
					AcceptSocket[newIndex] = 0;
					//WSACloseEvent(Events[newIndex]);    // �ر��¼� 
					DataBuf[newIndex].buf = NULL;
					DataBuf[newIndex].len = NULL;
					continue;
				}
				//ʹ������
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
						//WSACloseEvent(Events[newIndex]);    // �ر��¼� 
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

