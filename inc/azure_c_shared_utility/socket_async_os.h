// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

//This file pulls in OS-specific header files to allow compilation of socket_async.c under
// most OS's except for Windows. The individual OS sections are activated by a preprocessor
// #define. Please do add new sections for non-Windows OS's, but the code is not compatible
// with Windows, so Windows is not a fit here.

#ifndef AZURE_SOCKET_ASYNC_OS_H
#define AZURE_SOCKET_ASYNC_OS_H

// Tested with:
// ESP32
#ifdef USE_LWIP_SOCKET_FOR_AZURE_IOT
#include "lwip/sockets.h"
#endif

#ifdef USE_LINUX_SOCKET_FOR_AZURE_IOT
#include <sys/types.h> 
#include <sys/socket.h>
#endif

#endif

#endif /* AZURE_SOCKET_ASYNC_OS_H */
