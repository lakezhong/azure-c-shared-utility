// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdbool.h>
#include <stdint.h>

// Enable platform-specific socket.h files using preprocessor defines in the makefile
#ifdef USE_LWIP_SOCKET_FOR_AZURE_IOT
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#endif

#ifdef LINUX
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#endif // LINUX

// WIN32 sockets are incompatible with other OS socket function signatures,
// so this adapter isn't designed to function under Windows. 
// This frees Windows builds to be used mainly for negative unit testing.
// Linux can check positive functionality, and others will just
// verify that they compile.
#ifdef WIN32
#include "win32_header.h"
#endif

#include "azure_c_shared_utility/ssl_socket.h"
#include "azure_c_shared_utility/xlogging.h"

#define AZURE_SSL_SOCKET_SO_KEEPALIVE 1    /* enable keepalive */
#define AZURE_SSL_SOCKET_TCP_KEEPIDLE 30   /* seconds until first keep-alive */
#define AZURE_SSL_SOCKET_TCP_KEEPINTVL 30   /* seconds between keep-alives */
#define AZURE_SSL_SOCKET_TCP_KEEPCNT 3     /* number of keep-alive failures before declaring connection failure */


// EXTRACT_IPV4 pulls the uint32_t IPv4 address out of an addrinfo struct
#ifdef WIN32	
#define EXTRACT_IPV4(ptr) ((struct sockaddr_in *) ptr->ai_addr)->sin_addr.S_un.S_addr
#else
// This default definition handles lwIP. Please add comments for other systems tested.
#define EXTRACT_IPV4(ptr) ((struct sockaddr_in *) ptr->ai_addr)->sin_addr.s_addr
#endif





static int get_socket_errno(int fd)
{
    int sock_errno = 0;
    uint32_t optlen = sizeof(sock_errno);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &sock_errno, &optlen);
    return sock_errno;
}

int SSL_Socket_Create(uint32_t serverIPv4, uint16_t port)
{
    int result = -1;
    int ret;
    int sock = -1;

    struct sockaddr_in sock_addr;

    if (serverIPv4 != 0)
    {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            LogError("create socket failed");
        }
        else
        {
            int keepAlive = 1; //enable keepalive
            int keepIdle = 20; //20s
            int keepInterval = 2; //2s
            int keepCount = 3; //retry # of times

            ret = 0;
            ret = ret || setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepAlive, sizeof(keepAlive));
            ret = ret || setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, (void *)&keepIdle, sizeof(keepIdle));
            ret = ret || setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval));
            ret = ret || setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount));

            // NB: On full-sized (multi-process) systems it would be necessary to use the SO_REUSEADDR option to 
            // grab the socket from any earlier (dying) invocations of the process and then deal with any 
            // residual junk in the connection stream. This doesn't happen with embedded, so it doesn't need
            // to be defended against.

            if (ret != 0)
            {
                LogError("set socket keep-alive failed, ret = %d ", ret);
            }
            else
            {
                // When supplied with either F_GETFL and F_SETFL parameters, the fcntl function
                // does simple bit flips which have no error path, so it is not necessary to
                // check for errors. (Source checked for linux and lwIP).
                int originalFlags = fcntl(sock, F_GETFL, 0);
                (void)fcntl(sock, F_SETFL, originalFlags | O_NONBLOCK);

                memset(&sock_addr, 0, sizeof(sock_addr));
                sock_addr.sin_family = AF_INET;
                sock_addr.sin_addr.s_addr = 0;
                sock_addr.sin_port = 0; // random local port

                ret = bind(sock, (struct sockaddr*)&sock_addr, sizeof(sock_addr));

                if (ret)
                {
                    LogError("bind socket failed");
                }
                else
                {

                    memset(&sock_addr, 0, sizeof(sock_addr));
                    sock_addr.sin_family = AF_INET;
                    sock_addr.sin_addr.s_addr = serverIPv4;
                    sock_addr.sin_port = htons(port);

                    ret = connect(sock, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
                    if (ret == -1)
                    {
                        int sockErr = get_socket_errno(sock);
                        if (sockErr != EINPROGRESS)
                        {
                            LogError("Socket connect failed, not EINPROGRESS: %d", sockErr);
                        }
                        else
                        {
                            // This is the normally expected code path for our non-blocking socket
                            // Wait for the write socket to be ready to perform a write.
                            fd_set writeset;
                            fd_set errset;
                            FD_ZERO(&writeset);
                            FD_ZERO(&errset);
                            FD_SET(sock, &writeset);
                            FD_SET(sock, &errset);


                            ret = select(sock + 1, NULL, &writeset, &errset, timeout);
                            if (ret <= 0)
                            {
                                LogError("Select failed: %d", get_socket_errno(sock));
                            }
                            else
                            {
                                if (FD_ISSET(sock, &errset))
                                {
                                    LogError("Socket select error is set: %d", get_socket_errno(sock));
                                }
                                else if (FD_ISSET(sock, &writeset))
                                {
                                    // Everything worked as expected, so set the result to our good socket
                                    result = sock;
                                }
                                else
                                {
                                    // not possible, so not worth the space for logging
                                }
                            }
                        }
                    }
                    else
                    {
                        // This result would be a big surprise because a non-blocking socket
                        // should always return EINPROGRESS
                        result = sock;
                    }
                }
            }
        }
    }

    if (sock >= 0 && result < 0)
    {
        SSL_Socket_Close(sock);
    }
    return result;
}

void SSL_Socket_Close(int sock)
{
    close(sock);
}

uint32_t SSL_Get_IPv4(const char* hostname)
{
    struct addrinfo *addrInfo = NULL;
    struct addrinfo *ptr = NULL;
    struct addrinfo hints;

    uint32_t result = 0;

    //--------------------------------
    // Setup the hints address info structure
    // which is passed to the getaddrinfo() function
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    //--------------------------------
    // Call getaddrinfo(). If the call succeeds,
    // the result variable will hold a linked list
    // of addrinfo structures containing response
    // information
    int getAddrResult = getaddrinfo(hostname, NULL, &hints, &addrInfo);
    if (getAddrResult == 0)
    {
        // If we find the AF_INET address, use it as the return value
        for (ptr = addrInfo; ptr != NULL; ptr = ptr->ai_next)
        {
            switch (ptr->ai_family)
            {
            case AF_INET:
                result = EXTRACT_IPV4(ptr);
                break;
            }
        }
        freeaddrinfo(addrInfo);
    }

    return result;
}

