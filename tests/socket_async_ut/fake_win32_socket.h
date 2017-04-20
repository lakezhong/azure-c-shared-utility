
#ifndef TEST_SOCKET_H
#define TEST_SOCKET_H

// This file enables testing of these Linux-oriented unit tests under Windows. It is not 
// strictly necessary, but is convenient to have.

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define	SOL_SOCKET	0xffff		/* options for socket level */
#define	SO_ERROR	0x1007		/* get error status and clear */
#define	AF_INET		2		/* internetwork: UDP, TCP, etc. */
#define	SOCK_STREAM	1		/* stream socket */
#define	SOCK_DGRAM	2		/* datagram socket */
#define	SO_KEEPALIVE	0x0008		/* keep connections alive */
#define IPPROTO_TCP     6
#define TCP_KEEPIDLE   0x03    /* set pcb->keep_idle  - Same as TCP_KEEPALIVE, but use seconds for get/setsockopt */
#define TCP_KEEPINTVL  0x04    /* set pcb->keep_intvl - Use seconds for get/setsockopt */
#define TCP_KEEPCNT    0x05    /* set pcb->keep_cnt   - Use number of probes sent for get/setsockopt */
#define F_GETFL 3
#define F_SETFL 4
#define O_NONBLOCK  1 /* nonblocking I/O */
#define	EACCES		13		/* Permission denied */

#ifndef FD_SET
#undef  FD_SETSIZE
    /* Make FD_SETSIZE match NUM_SOCKETS in socket.c */
#define FD_SETSIZE    8 
#define FD_SET(n, p)
#define FD_CLR(n, p)  ((p)->fd_bits[(n)/8] &= ~(1 << ((n) & 7)))
#define FD_ZERO(p)    memset((void*)(p),0,sizeof(*(p)))

    typedef struct fd_set {
        unsigned char fd_bits[(FD_SETSIZE + 7) / 8];
    } fd_set;

#endif /* FD_SET */

#define htons(x) x


    struct in_addr {
        uint32_t       s_addr;     /* address in network byte order */
    };

    struct sockaddr_in {
        uint8_t         sin_family; /* address family: AF_INET */
        uint16_t        sin_port;   /* port in network byte order */
        struct in_addr  sin_addr;   /* internet address */
    };

    struct sockaddr {
        uint8_t	sa_len;			/* total length */
        uint8_t	sa_family;		/* address family */
        char	sa_data[14];		/* actually longer; address value */
    };

    struct timeval {
        long    tv_sec;         /* seconds */
        long    tv_usec;        /* and microseconds */
    };



    int socket(int socket_family, int socket_type, int protocol);

    int fcntl(int fd, int cmd, int arg);

    int bind(int sockfd, const struct sockaddr *addr, size_t addrlen);

    int getsockopt(int sockfd, int level, int optname, void *optval, size_t *optlen); 
    
    int setsockopt(int sockfd, int level, int optname, const void *optval, size_t optlen);

    int connect(int sockfd, const struct sockaddr *addr, size_t addrlen);

    int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);

    int send(int sockfd, const void *buf, size_t len, int flags);

    int recv(int sockfd, void *buf, size_t len, int flags);

    int close(int fd);

    int FD_ISSET(int sock, void* dummy);

#ifdef __cplusplus
}
#endif

#endif /* TEST_SOCKET_H */
