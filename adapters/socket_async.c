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

// WIN32 sockets are incompatible with other OS socket function signatures
#ifdef WIN32
// Just use this header for convenience while writing the code in Windows. Later it will
// only run for Linux variants.
#include "fake_win32_socket.h"
#endif

#include "azure_c_shared_utility/socket_async.h"
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

int socket_async_create(SOCKET_ASYNC_HANDLE* sock_out, uint32_t serverIPv4, uint16_t port, 
    bool is_UDP, SOCKET_ASYNC_OPTIONS_HANDLE options)
{
    int result;
    int sock;
    *sock_out = SOCKET_ASYNC_NULL_SOCKET;
    struct sockaddr_in sock_addr;

    if (sock_out == NULL)
    {
        LogError("sock_out is NULL");
        result = __FAILURE__;
    }
    else
    {
        sock = socket(AF_INET, is_UDP ? SOCK_DGRAM : SOCK_STREAM, 0);
        if (sock < 0)
        {
            LogError("create socket failed");
            result = __FAILURE__;
        }
        else
        {
            int setopt_ret = 0;
            // None of the currently defined options apply to UDP
            if (!is_UDP)
            {
                bool disable_keepalive;  // disable by default
                if (options != NULL)
                {
                    if (options->keep_alive > 0)
                    {
                        int keepAlive = 1; //enable keepalive
                        setopt_ret = setopt_ret || setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepAlive, sizeof(keepAlive));
                        setopt_ret = setopt_ret || setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, (void *)&(options->keep_idle), sizeof((options->keep_idle)));
                        setopt_ret = setopt_ret || setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, (void *)&(options->keep_interval), sizeof((options->keep_interval)));
                        setopt_ret = setopt_ret || setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, (void *)&(options->keep_count), sizeof((options->keep_count)));
                        disable_keepalive = false;
                    }
                    else if (options->keep_alive == 0)
                    {
                        disable_keepalive = true;
                    }
                    else
                    {
                        // < 0 means use system defaults, so do nothing
                        disable_keepalive = false;
                    }
                }
                else
                {
                    disable_keepalive = true;
                }

                if (disable_keepalive)
                {
                    int keepAlive = 0; //disable keepalive
                    setopt_ret = setopt_ret || setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepAlive, sizeof(keepAlive));
                }
            }

            // NB: On full-sized (multi-process) systems it would be necessary to use the SO_REUSEADDR option to 
            // grab the socket from any earlier (dying) invocations of the process and then deal with any 
            // residual junk in the connection stream. This doesn't happen with embedded, so it doesn't need
            // to be defended against.

            if (setopt_ret != 0)
            {
                LogError("setsockopt failed: %d ", setopt_ret);
                result = __FAILURE__;
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

                int bind_ret = bind(sock, (struct sockaddr*)&sock_addr, sizeof(sock_addr));

                if (bind_ret)
                {
                    LogError("bind socket failed");
                    result = __FAILURE__;
                }
                else
                {

                    memset(&sock_addr, 0, sizeof(sock_addr));
                    sock_addr.sin_family = AF_INET;
                    sock_addr.sin_addr.s_addr = serverIPv4;
                    sock_addr.sin_port = htons(port);

                    int connect_ret = connect(sock, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
                    if (connect_ret == -1)
                    {
                        int sockErr = get_socket_errno(sock);
                        if (sockErr != EINPROGRESS)
                        {
                            LogError("Socket connect failed, not EINPROGRESS: %d", sockErr);
                            result = __FAILURE__;
                        }
                        else
                        {
                            // This is the normally expected code path for our non-blocking socket
                            result = 0;
                            *sock_out = sock;
                        }
                    }
                    else
                    {
                        // This result would be a surprise because a non-blocking socket
                        // returns EINPROGRESS. But it could happen if this thread got
                        // blocked for a while by the system while the handshake proceeded,
                        // or for a UDP socket.
                        result = 0;
                        *sock_out = sock;
                    }
                }
            }
        }
    }

    return result;
}

int socket_async_is_create_complete(SOCKET_ASYNC_HANDLE sock, bool* is_complete)
{
    int result;
    if (is_complete == NULL)
    {
        LogError("is_complete is NULL");
        result = __FAILURE__;
    }
    else
    {
        is_complete = false;

        // Check if the socket is ready to perform a write.
        fd_set writeset;
        fd_set errset;
        FD_ZERO(&writeset);
        FD_ZERO(&errset);
        FD_SET(sock, &writeset);
        FD_SET(sock, &errset);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_sec = 0;
        int select_ret = select(sock + 1, NULL, &writeset, &errset, &tv);
        if (select_ret <= 0)
        {
            LogError("Socket select failed: %d", get_socket_errno(sock));
            result = __FAILURE__;
        }
        else
        {
            if (FD_ISSET(sock, &errset))
            {
                LogError("Socket select errset non-empty: %d", get_socket_errno(sock));
                result = __FAILURE__;
            }
            else if (FD_ISSET(sock, &writeset))
            {
                // Everything worked as expected, so set the result to our good socket
                result = 0;
                *is_complete = true;
            }
            else
            {
                // not possible, so not worth the space for logging; just quiet the compiler
                result = __FAILURE__;
            }
        }
    }
    return result;
}

int socket_async_send(SOCKET_ASYNC_HANDLE sock, void* buffer, size_t size, size_t* sent_count)
{
    int result;
    if (buffer == NULL)
    {
        LogError("buffer is NULL");
        result = __FAILURE__;
    }
    else
    {
        if (sent_count == NULL)
        {
            LogError("sent_count is NULL");
            result = __FAILURE__;
        }
        else
        {
            int send_result = send(sock, buffer, size, 0);
            if (send_result < 0)
            {
                *sent_count = 0;
                int sock_err = get_socket_errno(sock);
                if (sock_err == EAGAIN || sock_err == EWOULDBLOCK)
                {
                    // Nothing sent, try again later
                    result = 0;
                }
                else
                {
                    // Something bad happened
                    LogError("Unexpected send error: %d", sock_err);
                    result = __FAILURE__;
                }
            }
            else
            {
                // Sent at least part of the message
                result = 0;
                *sent_count = (size_t)send_result;
            }
        }
    }
    return result;
}

int socket_async_receive(SOCKET_ASYNC_HANDLE sock, void* buffer, size_t size, size_t* received_count)
{
    int result;
    if (buffer == NULL)
    {
        LogError("buffer is NULL");
        result = __FAILURE__;
    }
    else
    {
        if (received_count == NULL)
        {
            LogError("received_count is NULL");
            result = __FAILURE__;
        }
        else
        {
            int recv_result = recv(sock, buffer, size, 0);
            if (recv_result < 0)
            {
                *received_count = 0;
                int sock_err = get_socket_errno(sock);
                if (sock_err == EAGAIN || sock_err == EWOULDBLOCK)
                {
                    // Nothing received, try again later
                    result = 0;
                }
                else
                {
                    // Something bad happened
                    LogError("Unexpected recv error: %d", sock_err);
                    result = __FAILURE__;
                }
            }
            else
            {
                // Received some stuff
                result = 0;
                *received_count = (size_t)recv_result;
            }
        }
    }
    return result;
}

void socket_async_destroy(int sock)
{
    close(sock);
}

#if(0)
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
#endif

