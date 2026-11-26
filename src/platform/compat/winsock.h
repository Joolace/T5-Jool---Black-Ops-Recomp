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
static inline int ioctlsocket(SOCKET s, long cmd, u_long *arg) { return ioctl(s, (unsigned long)cmd, arg); }
#define WSAGetLastError() (errno)
#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAECONNRESET ECONNRESET
#define WSAEINTR EINTR
typedef struct sockaddr SOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;
#define WSAENOTSOCK ENOTSOCK
#define WSAEADDRINUSE EADDRINUSE
#define WSAEADDRNOTAVAIL EADDRNOTAVAIL
#define WSAENETDOWN ENETDOWN
#define WSAENETUNREACH ENETUNREACH
#define WSAEMSGSIZE EMSGSIZE
#define WSAECONNREFUSED ECONNREFUSED
#define WSAEAFNOSUPPORT EAFNOSUPPORT
#define WSAEINVAL EINVAL
#define WSAEISCONN EISCONN
#define WSAEINPROGRESS EINPROGRESS
#define WSAEALREADY EALREADY
#define WSAENOBUFS ENOBUFS
#define WSAETIMEDOUT ETIMEDOUT
#define WSAEACCES EACCES
#define WSAEHOSTUNREACH EHOSTUNREACH
#define WSAECONNABORTED ECONNABORTED
#define WSAEFAULT EFAULT
#define WSANOTINITIALISED 10093
#define WSASYSNOTREADY 10091
#define WSAVERNOTSUPPORTED 10092
#define WSAEDISCON 10101
#define WSAHOST_NOT_FOUND 11001
#define WSANO_DATA 11004
#define WSAENETRESET ENETRESET
#define WSAESHUTDOWN ESHUTDOWN
#define WSAEOPNOTSUPP EOPNOTSUPP
#define WSAEPROTONOSUPPORT EPROTONOSUPPORT
#define WSAESOCKTNOSUPPORT ESOCKTNOSUPPORT
#define WSAEPFNOSUPPORT EPFNOSUPPORT
#define WSAEDESTADDRREQ EDESTADDRREQ
#define WSAEPROTOTYPE EPROTOTYPE
#define WSAENOPROTOOPT ENOPROTOOPT
#define WSAEMFILE EMFILE
#define WSAENOTCONN ENOTCONN
typedef struct hostent HOSTENT;
typedef struct WSAData { WORD wVersion, wHighVersion; char szDescription[257]; char szSystemStatus[129]; unsigned short iMaxSockets, iMaxUdpDg; char *lpVendorInfo; } WSADATA, *LPWSADATA;
static inline int WSAStartup(WORD, LPWSADATA d) { if (d) memset(d, 0, sizeof(*d)); return 0; }
static inline int WSACleanup(void) { return 0; }

#ifdef __cplusplus
#include <type_traits>
// winsock usa int* per le lunghezze, POSIX socklen_t*: sovraccarichi di comodo.
template <class I, class = typename std::enable_if<std::is_same<I, int>::value>::type>
static inline ssize_t recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, I *fromlen)
{
    socklen_t l = fromlen ? (socklen_t)*fromlen : 0;
    ssize_t r = ::recvfrom(s, buf, len, flags, from, fromlen ? &l : nullptr);
    if (fromlen) *fromlen = (int)l;
    return r;
}
template <class I, class = typename std::enable_if<std::is_same<I, int>::value>::type>
static inline int accept(int s, struct sockaddr *addr, I *addrlen)
{
    socklen_t l = addrlen ? (socklen_t)*addrlen : 0;
    int r = ::accept(s, addr, addrlen ? &l : nullptr);
    if (addrlen) *addrlen = (int)l;
    return r;
}
template <class I, class = typename std::enable_if<std::is_same<I, int>::value>::type>
static inline int getsockname(int s, struct sockaddr *addr, I *addrlen)
{
    socklen_t l = (socklen_t)*addrlen; int r = ::getsockname(s, addr, &l); *addrlen = (int)l; return r;
}
template <class I, class = typename std::enable_if<std::is_same<I, int>::value>::type>
static inline int getsockopt(int s, int level, int name, char *val, I *len)
{
    socklen_t l = (socklen_t)*len; int r = ::getsockopt(s, level, name, val, &l); *len = (int)l; return r;
}
#endif
