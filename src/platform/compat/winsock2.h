#pragma once
// winsock.h -> socket BSD. Le differenze (closesocket, ioctlsocket, WSA*) sono in win_compat.h/net.
#include "win_compat.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#define ioctlsocket ioctl
#define WSAGetLastError() (errno)
#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAECONNRESET ECONNRESET
#define WSAEINTR EINTR
typedef struct sockaddr SOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct WSAData { WORD wVersion, wHighVersion; char szDescription[257]; char szSystemStatus[129]; unsigned short iMaxSockets, iMaxUdpDg; char *lpVendorInfo; } WSADATA, *LPWSADATA;
static inline int WSAStartup(WORD, LPWSADATA d) { if (d) memset(d, 0, sizeof(*d)); return 0; }
static inline int WSACleanup(void) { return 0; }
