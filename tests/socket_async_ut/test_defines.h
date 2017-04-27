// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// This file is made an integral part of socket_async_ut.c with a #include. It
// is broken out for readability. 


// These definitions provide parameters and pass / fail values for unit testing
static int test_socket = (int)0x1;
static uint16_t test_port = 0x5566;
static uint32_t test_ipv4 = 0x11223344;

char test_msg[] = "Send this";

#define BAD_BUFFER_COUNT 10000
#define RECV_FAIL_RETURN -1
#define RECV_ZERO_FLAGS 0
#define EXTENDED_ERROR_FAIL EACCES
#define EXTENDED_ERROR_WAITING EAGAIN
static size_t sizeof_int = sizeof(test_socket);
