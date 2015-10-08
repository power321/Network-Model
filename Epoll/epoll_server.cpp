#include <iostream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

using namespace std;

#define MAXLINE 5
#define OPEN_MAX 100
#define LISTENQ 20
#define SERV_PORT 5000
#define INFTIM 1000

char str[] = "hello from epoll : this is a long string which may be cut by the net\n";

void setnonblocking(int sock){
	int opts;
	opts = fcntl(sock, F_GETFL);
	if (opts < 0){
		perror("fcntl(sock,GETFL)");
		return ;
	}
	opts = opts | O_NONBLOCK;
	if (fcntl(sock, F_SETFL, opts) < 0){
		perror("fcntl(sock,SETFL,opts)");
		return ;
	}
}

void CloseAndDisable(int sockid, epoll_event ev){
	close(sockid);
	ev.data.fd = -1;
}

int main(){
	int i, maxi, listenfd, connfd, sockfd, epfd, nfds, portnumber;
	char line[MAXLINE];
	socklen_t clilen;
	portnumber = SERV_PORT;
	//声明epoll_event结构体 ev用于注册,数组用于回传
	struct epoll_event ev, events[20];
	
	epfd = epoll_create(256);
	struct sockaddr_in clientaddr;
	struct sockaddr_in serveraddr;
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(portnumber);

	//bind and listen
	bind(listenfd, (sockaddr *)&serveraddr, sizeof(serveraddr));
	listen(listenfd, LISTENQ);
	
	//set data and events
	ev.data.fd = listenfd;
	ev.events = EPOLLIN | EPOLLET;
	
	epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

	maxi = 0;
	int bOut = 0;
	while (true){
		if (bOut == 1)
			break;
		nfds = epoll_wait(epfd, events, 20, -1);//Infinite
                int tmp = nfds;
                cout << "epoll_wait returns " << tmp << endl;

                for (i = 0; i < tmp; ++i){
                    cout << "event number : "<< i << "nfds : " << tmp << endl;
			if (events[i].data.fd == listenfd){
				connfd = accept(listenfd, (sockaddr *)&clientaddr, &clilen);
				if (connfd < 0){
					perror("connfd<0");
					return 1;
				}
				
                                char *adr = inet_ntoa(clientaddr.sin_addr);
                                cout << "accept a connection from" << adr << endl;

				setnonblocking(connfd);
				ev.data.fd = connfd;
				ev.events = EPOLLIN | EPOLLET;
				//register ev
				epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
			}
			else if (events[i].events & EPOLLIN){
				cout << "EPOLLIN" <<endl;
				if ((sockfd = events[i].data.fd) < 0)
					continue;
                                cout << "sockfd = " << sockfd << endl;
				char * head = line;
				int recvNum = 0;
				int count = 0;
				bool bReadOk = false;
				while(1){
					recvNum = recv(sockfd, head + count, MAXLINE, 0);
					if (recvNum < 0){
						if (errno == EAGAIN){
							//there is no buffer to read
							bReadOk = true;
							break;
						}
						else if (errno == ECONNRESET){
							CloseAndDisable(sockfd, events[i]);
							cout << "counterpart send out RST" << endl;
							break;
						}
						else if (errno == EINTR){
							continue;
						}
						else{
							CloseAndDisable(sockfd, events[i]);
							cout << "unrecovable error" << endl;
							break;
						}
					}
					else if (recvNum == 0){
						CloseAndDisable(sockfd, events[i]);
						cout << "counterpart has shut off" << endl;
						break;
					}

					count += recvNum;
					if (recvNum == MAXLINE){
						continue;//maybe has not readed all data
					}
					else{
						//read all data
						bReadOk = true;
						break;
					}
				}

                                if (bReadOk == true && count != 0){
					line[count] = '\0';
					cout << "we have read from the client : " << line;
					ev.data.fd = sockfd;
					ev.events = EPOLLOUT | EPOLLET;
					epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
				}
			}
			else if (events[i].events & EPOLLOUT){
                            cout << "before out ndfs = " << tmp << endl;

                                //memcpy(line, str, sizeof(str));
                                cout << "after mem ndfs = " << tmp << endl;
                                cout << "Write " << str ;
				sockfd = events[i].data.fd;
                                cout << "sockfd = " << sockfd << endl;

				bool bWritten = false;
				int writenLen = 0;
				int count = 0;
                                char * head = str;
                                cout << "str Size = " << sizeof(str) << endl;
                                head[sizeof(str)] = '\0';
                                cout << "before while ndfs = " << tmp << endl;
				while(1){
					writenLen = send(sockfd, head + count, MAXLINE, 0);
                                        cout << "writenLen = " << writenLen << endl;
					if (writenLen == -1){
						if (errno == EAGAIN){
							bWritten = true;
							break;
						}
						else if (errno == ECONNRESET){
							CloseAndDisable(sockfd, events[i]);
							cout << "counterpart send out RST" << endl;
							break;
						}
						else if (errno == EINTR){
							continue;
						}
						else{
							//other errors
						}
					}

					if (writenLen == 0){
						CloseAndDisable(sockfd, events[i]);
						cout << "counterpart has shut off" << endl;
						break;
					}

					count += writenLen;
					if (writenLen == MAXLINE){
						//maybe haven't done
						continue;
					}
					else{
						//all right
						bWritten = true;
						break;
					}
				}
                                cout << "count = " << count << endl;
                                if (bWritten == true && count != 0){
                                        cout << "write ok!" << endl;
					ev.data.fd = sockfd;
					ev.events = EPOLLIN | EPOLLET;
                                        cout << "before ctl ndfs = " << tmp << endl;
					epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
                                        cout << "after ctl ndfs = " << tmp << endl;
				}
			}
		}
	}
	return 0;
}
