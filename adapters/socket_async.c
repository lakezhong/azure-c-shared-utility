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
// so there will be a socket_async_win32.c file to handle Windows.
#ifdef WIN32
// This header is just for convenience while writing the code in Windows.
#include "fake_win32_socket.h"
#else
#include "azure_c_shared_utility/socket_async_os.h"
#endif

#include "azure_c_shared_utility/socket_async.h"
#include "azure_c_shared_utility/xlogging.h"


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
    struct sockaddr_in sock_addr;

    if (sock_out == NULL)
    {
        /* Codes_SRS_SOCKET_ASYNC_30_010: [ If the sock parameter is NULL, socket_async_create shall log an error and return FAILURE. ]*/
        LogError("sock_out is NULL");
        result = __FAILURE__;
    }
    else
    {
        /* Codes_SRS_SOCKET_ASYNC_30_013: [ The is_UDP parameter shall be true for a UDP connection, and false for TCP. ]*/
        sock = socket(AF_INET, is_UDP ? SOCK_DGRAM : SOCK_STREAM, 0);
        if (sock < 0)
        {
            /* Codes_SRS_SOCKET_ASYNC_30_023: [ If socket creation fails, the sock value shall be set to SOCKET_ASYNC_INVALID_SOCKET and socket_async_create shall log an error and return FAILURE. ]*/
            // An essentially impossible failure, not worth logging errno()
            LogError("create socket failed");
            *sock_out = SOCKET_ASYNC_INVALID_SOCKET;
            result = __FAILURE__;
        }
        else
        {
            int setopt_ret = 0;
            // None of the currently defined options apply to UDP
            /* Codes_SRS_SOCKET_ASYNC_30_015: [ If is_UDP is true, the optional options parameter shall be ignored. ]*/
            if (!is_UDP)
            {
                if (options != NULL)
                {
                    /* Codes_SRS_SOCKET_ASYNC_30_014: [ If the optional options parameter is non-NULL and is_UDP is false, and options->keep_alive is non-negative, socket_async_create shall set the socket options to the provided options values. ]*/
                    if (options->keep_alive >= 0)
                    {
                        int keepAlive = 1; //enable keepalive
                        setopt_ret = setopt_ret || setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepAlive, sizeof(keepAlive));
                        setopt_ret = setopt_ret || setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, (void *)&(options->keep_idle), sizeof((options->keep_idle)));
                        setopt_ret = setopt_ret || setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, (void *)&(options->keep_interval), sizeof((options->keep_interval)));
                        setopt_ret = setopt_ret || setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, (void *)&(options->keep_count), sizeof((options->keep_count)));
                    }
                    else
                    {
                        /* Codes_SRS_SOCKET_ASYNC_30_015: [ If the optional options parameter is non-NULL and is_UDP is false, and options->keep_alive is negative, socket_async_create not set the socket keep-alive options. ]*/
                        // < 0 means use system defaults, so do nothing
                    }
                }
                else
                {
                    /* Codes_SRS_SOCKET_ASYNC_30_017: [ If the optional options parameter is NULL and is_UDP is false, socket_async_create shall disable TCP keep-alive. ]*/
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
                /* Codes_SRS_SOCKET_ASYNC_30_020: [ If socket option setting fails, the sock value shall be set to SOCKET_ASYNC_INVALID_SOCKET and socket_async_create shall log an error and return FAILURE. ]*/
                LogError("setsockopt failed: %d ", setopt_ret);
                *sock_out = SOCKET_ASYNC_INVALID_SOCKET;
                result = __FAILURE__;
            }
            else
            {
                /* Codes_SRS_SOCKET_ASYNC_30_019: [ The socket returned in sock shall be non-blocking. ]*/
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

                if (bind_ret != 0)
                {
                    /* Codes_SRS_SOCKET_ASYNC_30_021: [ If socket binding fails, the sock value shall be set to SOCKET_ASYNC_INVALID_SOCKET and socket_async_create shall log an error and return FAILURE. ]*/
                    LogError("bind socket failed: %d", get_socket_errno(sock));
                    *sock_out = SOCKET_ASYNC_INVALID_SOCKET;
                    result = __FAILURE__;
                }
                else
                {

                    memset(&sock_addr, 0, sizeof(sock_addr));
                    sock_addr.sin_family = AF_INET;
                    /* Codes_SRS_SOCKET_ASYNC_30_011: [ The host_ipv4 parameter shall be the 32-bit IP V4 of the target server. ]*/
                    sock_addr.sin_addr.s_addr = serverIPv4;
                    /* Codes_SRS_SOCKET_ASYNC_30_012: [ The port parameter shall be the port number for the target server. ]*/
                    sock_addr.sin_port = htons(port);

                    int connect_ret = connect(sock, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
                    if (connect_ret == -1)
                    {
                        int sockErr = get_socket_errno(sock);
                        if (sockErr != EINPROGRESS)
                        {
                            /* Codes_SRS_SOCKET_ASYNC_30_022: [ If socket connection fails, the sock value shall be set to SOCKET_ASYNC_INVALID_SOCKET and socket_async_create shall log an error and return FAILURE. ]*/
                            LogError("Socket connect failed, not EINPROGRESS: %d", sockErr);
                            *sock_out = SOCKET_ASYNC_INVALID_SOCKET;
                            result = __FAILURE__;
                        }
                        else
                        {
                            // This is the normally expected code path for our non-blocking socket
                            /* Codes_SRS_SOCKET_ASYNC_30_018: [ On success, the sock value shall be set to the created and configured SOCKET_ASYNC_HANDLE and socket_async_create shall return 0. ]*/
                            result = 0;
                            *sock_out = sock;
                        }
                    }
                    else
                    {
                        /* Codes_SRS_SOCKET_ASYNC_30_018: [ On success, the sock value shall be set to the created and configured SOCKET_ASYNC_HANDLE and socket_async_create shall return 0. ]*/
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
        /* Codes_SRS_SOCKET_ASYNC_30_026: [ If the is_complete parameter is NULL, socket_async_is_create_complete shall log an error and return FAILURE. ]*/
        LogError("is_complete is NULL");
        result = __FAILURE__;
    }
    else
    {
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
        if (select_ret < 0)
        {
            /* Codes_SRS_SOCKET_ASYNC_30_028: [ On failure, the is_complete value shall be set to false and socket_async_create shall return FAILURE. ]*/
            LogError("Socket select failed: %d", get_socket_errno(sock));
            result = __FAILURE__;
        }
        else
        {
            if (FD_ISSET(sock, &errset))
            {
                /* Codes_SRS_SOCKET_ASYNC_30_028: [ On failure, the is_complete value shall be set to false and socket_async_create shall return FAILURE. ]*/
                LogError("Socket select errset non-empty: %d", get_socket_errno(sock));
                result = __FAILURE__;
            }
            else if (FD_ISSET(sock, &writeset))
            {
                /* Codes_SRS_SOCKET_ASYNC_30_027: [ On success, the is_complete value shall be set to the completion state and socket_async_create shall return 0. ]*/
                // Ready to read
                result = 0;
                *is_complete = true;
            }
            else
            {
                /* Codes_SRS_SOCKET_ASYNC_30_027: [ On success, the is_complete value shall be set to the completion state and socket_async_create shall return 0. ]*/
                // Not ready yet
                result = 0;
                *is_complete = false;
            }
        }
    }
    return result;
}

int socket_async_send(SOCKET_ASYNC_HANDLE sock, const void* buffer, size_t size, size_t* sent_count)
{
    int result;
    if (buffer == NULL)
    {
        /* Codes_SRS_SOCKET_ASYNC_30_033: [ If the buffer parameter is NULL, socket_async_send shall log the error return FAILURE. ]*/
        LogError("buffer is NULL");
        result = __FAILURE__;
    }
    else
    {
        if (sent_count == NULL)
        {
            /* Codes_SRS_SOCKET_ASYNC_30_034: [ If the sent_count parameter is NULL, socket_async_send shall log the error return FAILURE. ]*/
            LogError("sent_count is NULL");
            result = __FAILURE__;
        }
        else
        {
            int send_result = send(sock, buffer, size, 0);
            if (send_result < 0)
            {
                int sock_err = get_socket_errno(sock);
                if (sock_err == EAGAIN || sock_err == EWOULDBLOCK)
                {
                    /* Codes_SRS_SOCKET_ASYNC_30_036: [ If the underlying socket is unable to accept any bytes for transmission because its buffer is full, socket_async_send shall return 0 and the sent_count parameter shall receive the value 0. ]*/
                    // Nothing sent, try again later
                    *sent_count = 0;
                    result = 0;
                }
                else
                {
                    /* Codes_SRS_SOCKET_ASYNC_30_037: [ If socket_async_send fails unexpectedly, socket_async_send shall log the error return FAILURE. ]*/
                    // Something bad happened
                    LogError("Unexpected send error: %d", sock_err);
                    result = __FAILURE__;
                }
            }
            else
            {
                /* Codes_SRS_SOCKET_ASYNC_30_035: [ If the underlying socket accepts one or more bytes for transmission, socket_async_send shall return 0 and the sent_count parameter shall receive the number of bytes accepted for transmission. ]*/
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
        /* Codes_SRS_SOCKET_ASYNC_30_052: [ If the buffer parameter is NULL, socket_async_receive shall log the error and return FAILURE. ]*/
        LogError("buffer is NULL");
        result = __FAILURE__;
    }
    else
    {
        if (received_count == NULL)
        {
            /* Codes_SRS_SOCKET_ASYNC_30_053: [ If the received_count parameter is NULL, socket_async_receive shall log the error and return FAILURE. ]*/
            LogError("received_count is NULL");
            result = __FAILURE__;
        }
        else
        {
            int recv_result = recv(sock, buffer, size, 0);
            if (recv_result < 0)
            {
                int sock_err = get_socket_errno(sock);
                if (sock_err == EAGAIN || sock_err == EWOULDBLOCK)
                {
                    /* Codes_SRS_SOCKET_ASYNC_30_055: [ If the underlying socket has no received bytes available, socket_async_receive shall return 0 and the received_count parameter shall receive the value 0. ]*/
                    // Nothing received, try again later
                    *received_count = 0;
                    result = 0;
                }
                else
                {
                    /* Codes_SRS_SOCKET_ASYNC_30_056: [ If the underlying socket fails unexpectedly, socket_async_receive shall log the error and return FAILURE. ]*/
                    // Something bad happened
                    LogError("Unexpected recv error: %d", sock_err);
                    result = __FAILURE__;
                }
            }
            else
            {
                /* Codes_SRS_SOCKET_ASYNC_30_054: [ On success, the underlying socket shall set one or more received bytes into buffer, socket_async_receive shall return 0, and the received_count parameter shall receive the number of bytes received into buffer. ]*/
                // Received some stuff
                *received_count = (size_t)recv_result;
                result = 0;
            }
        }
    }
    return result;
}

void socket_async_destroy(int sock)
{
    /* Codes_SRS_SOCKET_ASYNC_30_071: [ socket_async_destroy shall call the underlying close method on the supplied socket. ]*/
    close(sock);
}


