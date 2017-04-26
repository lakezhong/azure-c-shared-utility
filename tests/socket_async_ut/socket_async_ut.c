// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#else
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#endif

/**
 * Include the C standards here.
 */
#ifdef __cplusplus
#include <cstddef>
#include <ctime>
#else
#include <stddef.h>
#include <time.h>

#endif

#include "azure_c_shared_utility/socket_async.h"

// This file is OS-specific, and is identified by setting include directories
// in the project
#include "socket_async_os.h"


/**
 * Include the test tools.
 */
#include "testrunnerswitcher.h"
#include "umock_c.h"
#include "umocktypes_charptr.h"
#include "umocktypes_bool.h"
#include "umock_c_negative_tests.h"
#include "azure_c_shared_utility/macro_utils.h"

#define ENABLE_MOCKS
#include "azure_c_shared_utility/gballoc.h"
MOCKABLE_FUNCTION(, int, socket, int, af, int, type, int, protocol);
MOCKABLE_FUNCTION(, int, bind, int, sockfd, const struct sockaddr*, addr, socklen_t, addrlen);
MOCKABLE_FUNCTION(, int, setsockopt, int, sockfd, int, level, int, optname, const void*, optval, socklen_t, optlen);
MOCKABLE_FUNCTION(, int, connect, int, sockfd, const struct sockaddr*, addr, socklen_t, addrlen);
MOCKABLE_FUNCTION(, int, select, int, nfds, fd_set*, readfds, fd_set*, writefds, fd_set*, exceptfds, struct timeval*, timeout);
MOCKABLE_FUNCTION(, ssize_t, send, int, sockfd, const void*, buf, size_t, len, int, flags);
MOCKABLE_FUNCTION(, ssize_t, recv, int, sockfd, void*, buf, size_t, len, int, flags);
MOCKABLE_FUNCTION(, int, close, int, sockfd);
#undef ENABLE_MOCKS

static int bool_Compare(bool left, bool right)
{
    return left != right;
}

static void bool_ToString(char* string, size_t bufferSize, bool val)
{
    (void)bufferSize;
    (void)strcpy(string, val ? "true" : "false");
}

#ifndef __cplusplus
static int _Bool_Compare(_Bool left, _Bool right)
{
    return left != right;
}

static void _Bool_ToString(char* string, size_t bufferSize, _Bool val)
{
    (void)bufferSize;
    (void)strcpy(string, val ? "true" : "false");
}
#endif

static int test_socket = (int)0x1;
static uint16_t test_port = 0x5566;
static uint32_t test_ipv4 = 0x11223344;
int fcntl(int fd, int cmd, ... /* arg */) { (void)fd; (void)cmd; return 0; }
#define BAD_BUFFER_COUNT 10000
char test_msg[] = "Send this";

#include "test_points.h"
#include "keep_alive.h"

static TEST_PATH test_path;

// getsockopt is only used to retrieve extended errors, so this is simpler than
// it might be.
int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
    (void)sockfd;
    (void)level;
    (void)optname;
    (void)optlen;
    int result = EAGAIN;
    switch (test_path)
    {
    case TP_UDP_SOCKET_FAIL: result = EACCES; break;
    case TP_UDP_CONNECT_IN_PROGRESS: result = EINPROGRESS; break;
    case TP_SEND_FAIL: result = EACCES; break;
    case TP_SEND_WAITING_OK: result = EAGAIN; break;
    case TP_RECEIVE_FAIL: result = EACCES; break;
    case TP_RECEIVE_WAITING_OK: result = EAGAIN; break;
    }
    // This ugly cast is safe for this UT and this socket_async.c file because of the 
    // limited usage of getsockopt
    *((int*)optval) = result;
    return 0;
}

int my_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    (void)timeout;
    (void)nfds;
    (void)readfds;
    // TP_TCP_IS_COMPLETE_ERRSET_FAIL,     // a non-empty error set
    // TP_TCP_IS_COMPLETE_READY_OK,        // 
    // TTP_TCP_IS_COMPLETE_NOT_READY_OK,    // 

    // This arguably odd sequence of FD_SET, etc. was necessary
    // to make the linux_c-ubuntu-clang build succeed. FD_CLR
    // did not work as expected on that system, but this does the job.
    switch (test_path)
    {
    case TP_TCP_IS_COMPLETE_ERRSET_FAIL: 
        FD_SET(nfds, exceptfds);
        break;
    case TP_TCP_IS_COMPLETE_READY_OK:
        FD_ZERO(exceptfds);
        FD_SET(nfds, writefds);
        break;
    case TP_TCP_IS_COMPLETE_NOT_READY_OK:
    default:
        FD_ZERO(exceptfds);
        FD_ZERO(writefds);
        break;
    }
    return 0;
}

 /**
  * Umock error will helps you to identify errors in the test suite or in the way that you are 
  *    using it, just keep it as is.
  */
DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    char temp_str[256];
    (void)snprintf(temp_str, sizeof(temp_str), "umock_c reported error :%s", ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
    ASSERT_FAIL(temp_str);
}

/**
 * This is necessary for the test suite, just keep as is.
 */
static TEST_MUTEX_HANDLE g_testByTest;
static TEST_MUTEX_HANDLE g_dllByDll;

BEGIN_TEST_SUITE(socket_async_ut)

    /**
     * This is the place where we initialize the test system. Replace the test name to associate the test 
     *   suite with your test cases.
     * It is called once, before start the tests.
     */
    TEST_SUITE_INITIALIZE(a)
    {
        int result;
        TEST_INITIALIZE_MEMORY_DEBUG(g_dllByDll);
        g_testByTest = TEST_MUTEX_CREATE();
        ASSERT_IS_NOT_NULL(g_testByTest);

        (void)umock_c_init(on_umock_c_error);

        result = umocktypes_charptr_register_types();
		ASSERT_ARE_EQUAL(int, 0, result);
        result = umocktypes_bool_register_types();
        ASSERT_ARE_EQUAL(int, 0, result);

        REGISTER_UMOCK_ALIAS_TYPE(ssize_t, int);
        REGISTER_UMOCK_ALIAS_TYPE(uint32_t, unsigned int);
        REGISTER_UMOCK_ALIAS_TYPE(socklen_t, uint32_t);

        REGISTER_GLOBAL_MOCK_RETURNS(socket, test_socket, -1);
        REGISTER_GLOBAL_MOCK_RETURNS(bind, 0, -1);
        REGISTER_GLOBAL_MOCK_RETURNS(connect, 0, -1);
        REGISTER_GLOBAL_MOCK_RETURNS(setsockopt, 0, -1);
        REGISTER_GLOBAL_MOCK_RETURNS(select, 0, -1);
        REGISTER_GLOBAL_MOCK_RETURNS(send, sizeof(test_msg), -1);
        REGISTER_GLOBAL_MOCK_RETURNS(recv, sizeof(test_msg), -1);

        REGISTER_GLOBAL_MOCK_HOOK(setsockopt, my_setsockopt);
        REGISTER_GLOBAL_MOCK_HOOK(select, my_select);

    }

    /**
     * The test suite will call this function to cleanup your machine.
     * It is called only once, after all tests is done.
     */
    TEST_SUITE_CLEANUP(TestClassCleanup)
    {
        //free(g_GenericPointer);

        umock_c_deinit();

        TEST_MUTEX_DESTROY(g_testByTest);
        TEST_DEINITIALIZE_MEMORY_DEBUG(g_dllByDll);
    }

    /**
     * The test suite will call this function to prepare the machine for the new test.
     * It is called before execute each test.
     */
    TEST_FUNCTION_INITIALIZE(initialize)
    {
        if (TEST_MUTEX_ACQUIRE(g_testByTest))
        {
            ASSERT_FAIL("Could not acquire test serialization mutex.");
        }

        umock_c_reset_all_calls();
    }

    /**
     * The test suite will call this function to cleanup your machine for the next test.
     * It is called after execute each test.
     */
    TEST_FUNCTION_CLEANUP(cleans)
    {
        TEST_MUTEX_RELEASE(g_testByTest);
    }

    TEST_FUNCTION(socket_async_destroy_test)
    {
        for (test_path = TP_DESTROY_OK; test_path <= TP_DESTROY_OK; test_path++)
        {
            begin_arrange(test_path);   ////// Begin the Arrange phase     

            /* Tests_SRS_SOCKET_ASYNC_30_071: [ socket_async_destroy shall call the underlying close method on the supplied socket. ]*/
            TEST_PATH_NO_FAIL(TP_DESTROY_OK, close(test_socket));

            begin_act(test_path);       ////// Begin the Act phase 

            /* Tests_SRS_SOCKET_ASYNC_30_071: [ socket_async_destroy shall call the underlying close method on the supplied socket. ]*/
            socket_async_destroy(test_socket);

            // Nothing to assert for socket_async_destroy except the call sequence

            end_assertions();   ////// End the Assertion phase and verify call sequence 
        }
    }

    TEST_FUNCTION(socket_async_recv_test)
    {
        for (test_path = TP_RECEIVE_NULL_BUFFER_FAIL; test_path <= TP_RECEIVE_OK; test_path++)
        {
            begin_arrange(test_path);   ////// Begin the Arrange phase     

            // Receive test paths
            //
            // TP_RECEIVE_NULL_BUFFER_FAIL,           // receive with null buffer
            // TP_RECEIVE_NULL_RECEIVED_COUNT_FAIL,   // receive with null receive count
            // TP_RECEIVE_FAIL,                       // receive failed
            // TP_RECEIVE_WAITING_OK,                 // receive not ready
            // TP_RECEIVE_OK,     
            // 
            switch (test_path)
            {
            case TP_RECEIVE_FAIL:
                TEST_PATH(TP_RECEIVE_FAIL, recv(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
                break;
            case TP_RECEIVE_WAITING_OK:
                TEST_PATH(TP_RECEIVE_WAITING_OK, recv(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
                break;
            case TP_RECEIVE_OK:
                TEST_PATH_NO_FAIL(TP_RECEIVE_OK, recv(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
                break;
            }

            begin_act(test_path);       ////// Begin the Act phase 

            /////////////////////////////////////////////////////////////////////////////////////////////////////
            // Receive test paths
            //
            // TP_RECEIVE_NULL_BUFFER_FAIL,           // receive with null buffer
            // TP_RECEIVE_NULL_RECEIVED_COUNT_FAIL,   // receive with null receive count
            // TP_RECEIVE_FAIL,                       // receive failed
            // TP_RECEIVE_WAITING_OK,                 // receive not ready
            // TP_RECEIVE_OK,     
            // 

            /////////////////////////////////////////////////////////////////////////////////////////////////////
            // Set up input parameters
            char *buffer = test_path == TP_RECEIVE_NULL_BUFFER_FAIL ? NULL : test_msg;
            size_t received_count = BAD_BUFFER_COUNT;
            size_t *received_count_param = test_path == TP_RECEIVE_NULL_RECEIVED_COUNT_FAIL ? NULL : &received_count;

            /////////////////////////////////////////////////////////////////////////////////////////////////////
            // Call the function under test
            int receive_result = socket_async_receive(test_socket, buffer, sizeof(test_msg), received_count_param);

            /////////////////////////////////////////////////////////////////////////////////////////////////////
            // Begin assertion phase

            // Does receive_result match expectations>?
            switch (test_path)
            {
            case TP_RECEIVE_NULL_BUFFER_FAIL:           /* Tests_SRS_SOCKET_ASYNC_30_052: [ If the buffer parameter is NULL, socket_async_receive shall log the error and return FAILURE. ]*/
            case TP_RECEIVE_NULL_RECEIVED_COUNT_FAIL:   /* Tests_SRS_SOCKET_ASYNC_30_053: [ If the received_count parameter is NULL, socket_async_receive shall log the error and return FAILURE. ]*/
            case TP_RECEIVE_FAIL:                       /* Codes_SRS_SOCKET_ASYNC_30_056: [ If the underlying socket fails unexpectedly, socket_async_receive shall log the error and return FAILURE. ]*/
                ASSERT_ARE_NOT_EQUAL_WITH_MSG(int, receive_result, 0, "Unexpected receive_result success");
                break;
            case TP_RECEIVE_WAITING_OK:    /* Tests_SRS_SOCKET_ASYNC_30_055: [ If the underlying socket has no received bytes available, socket_async_receive shall return 0 and the received_count parameter shall receive the value 0. ]*/
            case TP_RECEIVE_OK:            /* Tests_SRS_SOCKET_ASYNC_30_054: [ On success, the underlying socket shall set one or more received bytes into buffer, socket_async_receive shall return 0, and the received_count parameter shall receive the number of bytes received into buffer. ]*/
                ASSERT_ARE_EQUAL_WITH_MSG(int, receive_result, 0, "Unexpected receive_result failure");
                break;
            default: ASSERT_FAIL("Unhandled test path")
            }

            // Does received_count match expectations>?
            switch (test_path)
            {
            case TP_RECEIVE_NULL_BUFFER_FAIL:
            case TP_RECEIVE_NULL_RECEIVED_COUNT_FAIL:
            case TP_RECEIVE_FAIL: 
                // The received_count should not have been touched
                ASSERT_ARE_EQUAL_WITH_MSG(int, received_count, BAD_BUFFER_COUNT, "Unexpected returned received_count has been set");
                break;
            case TP_RECEIVE_WAITING_OK: 
                /* Tests_SRS_SOCKET_ASYNC_30_055: [ If the underlying socket has no received bytes available, socket_async_receive shall return 0 and the received_count parameter shall receive the value 0. ]*/
                ASSERT_ARE_EQUAL_WITH_MSG(int, received_count, 0, "Unexpected returned received_count of non-zero");
                break;
            case TP_RECEIVE_OK:         
                /* Tests_SRS_SOCKET_ASYNC_30_054: [ On success, the underlying socket shall set one or more received bytes into buffer, socket_async_receive shall return 0, and the received_count parameter shall receive the number of bytes received into buffer. ]*/
                ASSERT_ARE_EQUAL_WITH_MSG(int, received_count, sizeof(test_msg), "Unexpected returned received_count");
                break;
            default: ASSERT_FAIL("Unhandled test path")
            }

            end_assertions();   ////// End the Assertion phase and verify call sequence 
        }
    }

    TEST_FUNCTION(socket_async_send_test)
    {
        for (test_path = TP_SEND_NULL_BUFFER_FAIL; test_path <= TP_SEND_OK; test_path++)
        {
            begin_arrange(test_path);   ////// Begin the Arrange phase     
            init_keep_alive_values();

            // Send test paths
            //
            // TP_SEND_NULL_BUFFER_FAIL,           // send with null buffer
            // TP_SEND_NULL_SENT_COUNT_FAIL,       // send with null sent count
            // TP_SEND_FAIL,                       // send failed
            // TP_SEND_WAITING_OK,                 // send not ready
            // TP_SEND_OK,     
            // 
            switch (test_path)
            {
            case TP_SEND_NULL_BUFFER_FAIL:
            case TP_SEND_NULL_SENT_COUNT_FAIL:
                // No expected calls here
                break;  
            case TP_SEND_FAIL:
                TEST_PATH(TP_SEND_FAIL, send(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
                break;
            case TP_SEND_WAITING_OK:
                // The "Fail" of send is not a failure, but rather returns a positive value for bytes sent 
                TEST_PATH(TP_SEND_WAITING_OK, send(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
                break;
            case TP_SEND_OK:
                // The "No Fail" of send returns 0, which is a successful "no data available"
                //TEST_PATH_NO_FAIL(TP_SEND_OK, send(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG)).IgnoreArgument(1);
                TEST_PATH_NO_FAIL(TP_SEND_OK, send(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
                break;
            default: ASSERT_FAIL("Unhandled test path")
            }

            begin_act(test_path);       ////// Begin the Act phase 

            /////////////////////////////////////////////////////////////////////////////////////////////////////
            // Send test paths
            //
            // TP_SEND_NULL_BUFFER_FAIL,           // send with null buffer
            // TP_SEND_NULL_SENT_COUNT_FAIL,       // send with null sent count
            // TP_SEND_FAIL,                       // send failed
            // TP_SEND_WAITING_OK,                 // send not ready
            // TP_SEND_OK,     
            // 

            /////////////////////////////////////////////////////////////////////////////////////////////////////
            // Set up input parameters
            const char *buffer = test_path == TP_SEND_NULL_BUFFER_FAIL ? NULL : test_msg;
            size_t sent_count = BAD_BUFFER_COUNT;
            size_t *sent_count_param = test_path == TP_SEND_NULL_SENT_COUNT_FAIL ? NULL : &sent_count;

            /////////////////////////////////////////////////////////////////////////////////////////////////////
            // Call the function under test
            int send_result = socket_async_send(test_socket, buffer, sizeof(test_msg), sent_count_param);

            /////////////////////////////////////////////////////////////////////////////////////////////////////
            // Begin assertion phase

            // Does send_result match expectations>?
            switch (test_path)
            {
            case TP_SEND_NULL_BUFFER_FAIL:      /* Tests_SRS_SOCKET_ASYNC_30_033: [ If the buffer parameter is NULL, socket_async_send shall log the error return FAILURE. ]*/
            case TP_SEND_NULL_SENT_COUNT_FAIL:  /* Tests_SRS_SOCKET_ASYNC_30_034: [ If the sent_count parameter is NULL, socket_async_send shall log the error return FAILURE. ]*/
            case TP_SEND_FAIL:                  /* Tests_SRS_SOCKET_ASYNC_30_037: [ If socket_async_send fails unexpectedly, socket_async_send shall log the error return FAILURE. ]*/
                ASSERT_ARE_NOT_EQUAL_WITH_MSG(int, send_result, 0, "Unexpected send_result success");
                break;
            case TP_SEND_WAITING_OK:    /* Tests_SRS_SOCKET_ASYNC_30_036: [ If the underlying socket is unable to accept any bytes for transmission because its buffer is full, socket_async_send shall return 0 and the sent_count parameter shall receive the value 0. ]*/
            case TP_SEND_OK:            /* Tests_SRS_SOCKET_ASYNC_30_035: [ If the underlying socket accepts one or more bytes for transmission, socket_async_send shall return 0 and the sent_count parameter shall receive the number of bytes accepted for transmission. ]*/
                ASSERT_ARE_EQUAL_WITH_MSG(int, send_result, 0, "Unexpected send_result failure");
                break;
            default: ASSERT_FAIL("Unhandled test path")
            }

            // Does sent_count match expectations>?
            switch (test_path)
            {
            case TP_SEND_NULL_BUFFER_FAIL:
            case TP_SEND_NULL_SENT_COUNT_FAIL:
            case TP_SEND_FAIL: 
                ASSERT_ARE_EQUAL_WITH_MSG(int, sent_count, BAD_BUFFER_COUNT, "Unexpected returned sent_count has been set");
                break;
            case TP_SEND_WAITING_OK:    /* Tests_SRS_SOCKET_ASYNC_30_036: [ If the underlying socket is unable to accept any bytes for transmission because its buffer is full, socket_async_send shall return 0 and the sent_count parameter shall receive the value 0. ]*/
                ASSERT_ARE_EQUAL_WITH_MSG(int, sent_count, 0, "Unexpected returned sent_count value is non-zero");
                break;
            case TP_SEND_OK:            /* Tests_SRS_SOCKET_ASYNC_30_035: [ If the underlying socket accepts one or more bytes for transmission, socket_async_send shall return 0 and the sent_count parameter shall receive the number of bytes accepted for transmission. ]*/
                ASSERT_ARE_EQUAL_WITH_MSG(int, sent_count, sizeof(test_msg), "Unexpected returned sent_count value is not message size");
                break;
            default: ASSERT_FAIL("Unhandled test path")
            }

            end_assertions();   ////// End the Assertion phase and verify call sequence 
        }
    }

    TEST_FUNCTION(socket_async_is_create_complete_test)
    {
        for (test_path = TP_TCP_IS_COMPLETE_NULL_PARAM_FAIL; test_path <= TP_TCP_IS_COMPLETE_NOT_READY_OK; test_path++)
        {
            begin_arrange(test_path);   ////// Begin the Arrange phase     

            // The socket_async_is_create_complete test paths
            //
            // TP_TCP_IS_COMPLETE_NULL_PARAM_FAIL, // supplying a null is_complete
            // TP_TCP_IS_COMPLETE_SELECT_FAIL,     // the select call fails
            // TP_TCP_IS_COMPLETE_ERRSET_FAIL,     // a non-empty error set
            // TP_TCP_IS_COMPLETE_READY_OK,        // 
            // TP_TCP_IS_COMPLETE_NOT_READY_OK,    // 
            //
            switch (test_path)
            {
            case TP_TCP_IS_COMPLETE_NULL_PARAM_FAIL:
                // No expected call here
                break;
            case TP_TCP_IS_COMPLETE_SELECT_FAIL:
                TEST_PATH(TP_TCP_IS_COMPLETE_SELECT_FAIL, select(test_socket + 1, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
                break;
            case TP_TCP_IS_COMPLETE_ERRSET_FAIL:
                TEST_PATH_NO_FAIL(TP_TCP_IS_COMPLETE_ERRSET_FAIL, select(test_socket + 1, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
                break;
            case TP_TCP_IS_COMPLETE_READY_OK:
                TEST_PATH_NO_FAIL(TP_TCP_IS_COMPLETE_READY_OK, select(test_socket + 1, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
                break;
            case TP_TCP_IS_COMPLETE_NOT_READY_OK:
                TEST_PATH_NO_FAIL(TP_TCP_IS_COMPLETE_NOT_READY_OK, select(test_socket + 1, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
                break;
            default: ASSERT_FAIL("Unhandled test path")
            }

            begin_act(test_path);       ////// Begin the Act phase 

            //////////////////////////////////////////////////////////////////////////////////////////////////////
            // The socket_async_is_create_complete test paths
            //
            // TP_TCP_IS_COMPLETE_NULL_PARAM_FAIL, // supplying a null is_complete
            // TP_TCP_IS_COMPLETE_SELECT_FAIL,     // the select call fails
            // TP_TCP_IS_COMPLETE_ERRSET_FAIL,     // a non-empty error set
            // TP_TCP_IS_COMPLETE_READY_OK,        // 
            // TP_TCP_IS_COMPLETE_NOT_READY_OK,    // 
            //

            /////////////////////////////////////////////////////////////////////////////////////////////////////
            // Set up input parameters
            // We set is_complete to the opposite of what's expected so we can spot a change
            bool is_complete = test_path != TP_TCP_IS_COMPLETE_READY_OK;
            bool* is_complete_param = test_path == TP_TCP_IS_COMPLETE_NULL_PARAM_FAIL ? NULL : &is_complete;

            /////////////////////////////////////////////////////////////////////////////////////////////////////
            // Call the function under test
            int create_complete_result = socket_async_is_create_complete(test_socket, is_complete_param);

            /////////////////////////////////////////////////////////////////////////////////////////////////////
            // Begin assertion phase

            // Does create_complete_result match expectations?
            switch (test_path)
            {
            case TP_TCP_IS_COMPLETE_NULL_PARAM_FAIL:    /* Tests_SRS_SOCKET_ASYNC_30_026: [ If the is_complete parameter is NULL, socket_async_is_create_complete shall log an error and return FAILURE. ]*/
            case TP_TCP_IS_COMPLETE_SELECT_FAIL:        /* Tests_SRS_SOCKET_ASYNC_30_028: [ On failure, the is_complete value shall be set to false and socket_async_create shall return FAILURE. ]*/
            case TP_TCP_IS_COMPLETE_ERRSET_FAIL:        /* Tests_SRS_SOCKET_ASYNC_30_028: [ On failure, the is_complete value shall be set to false and socket_async_create shall return FAILURE. ]*/
                ASSERT_ARE_NOT_EQUAL_WITH_MSG(int, create_complete_result, 0, "Unexpected create_complete_result success");
                break;
            case TP_TCP_IS_COMPLETE_READY_OK:       /* Codes_SRS_SOCKET_ASYNC_30_027: [ On success, the is_complete value shall be set to the completion state and socket_async_create shall return 0. ]*/
            case TP_TCP_IS_COMPLETE_NOT_READY_OK:   /* Codes_SRS_SOCKET_ASYNC_30_027: [ On success, the is_complete value shall be set to the completion state and socket_async_create shall return 0. ]*/
                ASSERT_ARE_EQUAL_WITH_MSG(int, create_complete_result, 0, "Unexpected create_complete_result failure");
                break;
            default: ASSERT_FAIL("Unhandled test path")
            }

            // Does is_compete match expectations?
            switch (test_path)
            {
            case TP_TCP_IS_COMPLETE_NULL_PARAM_FAIL:
            case TP_TCP_IS_COMPLETE_SELECT_FAIL:
            case TP_TCP_IS_COMPLETE_ERRSET_FAIL:
                // Undefined result here
                break;
            case TP_TCP_IS_COMPLETE_NOT_READY_OK:   /* Codes_SRS_SOCKET_ASYNC_30_027: [ On success, the is_complete value shall be set to the completion state and socket_async_create shall return 0. ]*/
                ASSERT_ARE_EQUAL_WITH_MSG(bool, is_complete, false, "Unexpected returned is_complete is true");
                break;
            case TP_TCP_IS_COMPLETE_READY_OK:       /* Codes_SRS_SOCKET_ASYNC_30_027: [ On success, the is_complete value shall be set to the completion state and socket_async_create shall return 0. ]*/
                ASSERT_ARE_EQUAL_WITH_MSG(bool, is_complete, true, "Unexpected returned is_complete is false");
                break;
            default: ASSERT_FAIL("Unhandled test path")
            }

            end_assertions();   ////// End the Assertion phase and verify call sequence 
        }
    }

    TEST_FUNCTION(socket_async_create_tcp_test)
    {
        for (test_path = TP_TCP_SOCKET_FAIL; test_path <= TP_TCP_CONNECT_SUCCESS; test_path++)
        {
            begin_arrange(test_path);   ////// Begin the Arrange phase     
            init_keep_alive_values();

            // The socket_async_create_tcp test paths
            //// Create TCP
            // TP_TCP_SOCKET_FAIL,			// socket create fail
            // TP_TCP_SOCKET_OPT_0_FAIL,   // setsockopt set keep-alive fail 0
            // TP_TCP_SOCKET_OPT_1_FAIL,   // setsockopt set keep-alive fail 1
            // TP_TCP_SOCKET_OPT_2_FAIL,   // setsockopt set keep-alive fail 2
            // TP_TCP_SOCKET_OPT_3_FAIL,   // setsockopt set keep-alive fail 3
            // TP_TCP_SOCKET_OPT_DEFAULT_FAIL, // setsockopt default disable keep-alive fail
            // TP_TCP_SOCKET_OPT_SET_OK,   // setsockopt set keep-alive OK
            // TP_TCP_SOCKET_OPT_SET_SYS_DEFAULTS_OK,   // setsockopt use system defaults for keep-alive
            // TP_TCP_BIND_FAIL,			// socket bind fail
            // TP_TCP_CONNECT_FAIL,		// socket connect fail
            // TP_TCP_CONNECT_IN_PROGRESS,	// socket connect in progress
            // TP_TCP_CONNECT_SUCCESS,     // socket connect instant success
            //

            switch (test_path)
            {
            case TP_TCP_SOCKET_FAIL:
                TEST_PATH(TP_TCP_SOCKET_FAIL, socket(AF_INET, SOCK_STREAM, 0));
                break;
            case TP_TCP_SOCKET_OPT_0_FAIL:
            case TP_TCP_SOCKET_OPT_1_FAIL:
            case TP_TCP_SOCKET_OPT_2_FAIL:
            case TP_TCP_SOCKET_OPT_3_FAIL:
            case TP_TCP_SOCKET_OPT_SET_OK:
                // Here we are explicitly setting the keep-alive options
                TEST_PATH_NO_FAIL(TP_TCP_SOCKET_FAIL, socket(AF_INET, SOCK_STREAM, 0));
                TEST_PATH(TP_TCP_SOCKET_OPT_0_FAIL, setsockopt(test_socket, IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                TEST_PATH(TP_TCP_SOCKET_OPT_1_FAIL, setsockopt(test_socket, IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                TEST_PATH(TP_TCP_SOCKET_OPT_2_FAIL, setsockopt(test_socket, IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                TEST_PATH(TP_TCP_SOCKET_OPT_3_FAIL, setsockopt(test_socket, IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                TEST_PATH_NO_FAIL(TP_TCP_SOCKET_OPT_SET_OK, bind(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                TEST_PATH_NO_FAIL(TP_TCP_SOCKET_OPT_SET_OK, connect(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                TEST_PATH_NO_FAIL(TP_TCP_BIND_FAIL, bind(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                TEST_PATH_NO_FAIL(TP_TCP_CONNECT_FAIL, connect(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG));

                break;
            case TP_TCP_SOCKET_OPT_SET_SYS_DEFAULTS_OK:
                // Here we don't set any socket options
                TEST_PATH_NO_FAIL(TP_TCP_SOCKET_OPT_SET_SYS_DEFAULTS_OK, socket(AF_INET, SOCK_STREAM, 0));
                TEST_PATH_NO_FAIL(TP_TCP_SOCKET_OPT_SET_SYS_DEFAULTS_OK, bind(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                TEST_PATH_NO_FAIL(TP_TCP_SOCKET_OPT_SET_SYS_DEFAULTS_OK, connect(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                 break;
            case TP_TCP_SOCKET_OPT_DEFAULT_FAIL:
            case TP_TCP_BIND_FAIL:
            case TP_TCP_CONNECT_FAIL:
            case TP_TCP_CONNECT_IN_PROGRESS:
            case TP_TCP_CONNECT_SUCCESS:
                // Here we are turning keep-alive off by default
                TEST_PATH_NO_FAIL(TP_TCP_SOCKET_FAIL, socket(AF_INET, SOCK_STREAM, 0));
                TEST_PATH(TP_TCP_SOCKET_OPT_DEFAULT_FAIL, setsockopt(test_socket, IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                TEST_PATH(TP_TCP_BIND_FAIL, bind(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                TEST_PATH(TP_TCP_CONNECT_FAIL, connect(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                break;
            default: ASSERT_FAIL("Unhandled test path")
            }

            begin_act(test_path);       ////// Begin the Act phase 

            /////////////////////////////////////////////////////////////////////////////////////////////////////
            // Set up input parameters
            SOCKET_ASYNC_OPTIONS options_value = { test_keep_alive , test_keep_idle , test_keep_interval, test_keep_count };
            options_value.keep_alive = test_path == TP_TCP_SOCKET_OPT_SET_SYS_DEFAULTS_OK ? test_keep_alive_sys_default : test_keep_alive;
            SOCKET_ASYNC_OPTIONS* options = NULL;
            if (test_path >= TP_TCP_SOCKET_OPT_0_FAIL && test_path <= TP_TCP_SOCKET_OPT_SET_SYS_DEFAULTS_OK)
            {
                options = &options_value;
            }
            /* Tests_SRS_SOCKET_ASYNC_30_013: [ The is_UDP parameter shall be true for a UDP connection, and false for TCP. ]*/
            bool is_udp = false;

            /////////////////////////////////////////////////////////////////////////////////////////////////////
            // Call the function under test
            SOCKET_ASYNC_HANDLE create_result = socket_async_create(test_ipv4, test_port, is_udp, options);

            /////////////////////////////////////////////////////////////////////////////////////////////////////
            // Begin assertion phase

            // Does create_result match expectations?
            switch (test_path)
            {
            case TP_TCP_SOCKET_FAIL:    /* Tests_SRS_SOCKET_ASYNC_30_010: [ If socket option creation fails, socket_async_create shall log an error and return SOCKET_ASYNC_INVALID_SOCKET. ]*/
                
            /* Tests_SRS_SOCKET_ASYNC_30_020: [ If socket option setting fails, the sock value shall be set to SOCKET_ASYNC_INVALID_SOCKET and socket_async_create shall log an error and return FAILURE. ]*/
            case TP_TCP_SOCKET_OPT_0_FAIL:
            case TP_TCP_SOCKET_OPT_1_FAIL:
            case TP_TCP_SOCKET_OPT_2_FAIL:
            case TP_TCP_SOCKET_OPT_3_FAIL:
            case TP_TCP_SOCKET_OPT_DEFAULT_FAIL:

            case TP_TCP_BIND_FAIL:      /* Tests_SRS_SOCKET_ASYNC_30_021: [ If socket binding fails, socket_async_create shall log an error and return SOCKET_ASYNC_INVALID_SOCKET. ]*/
            case TP_TCP_CONNECT_FAIL:   /* Tests_SRS_SOCKET_ASYNC_30_022: [ If socket connection fails, socket_async_create shall log an error and return SOCKET_ASYNC_INVALID_SOCKET. ]*/
                ASSERT_ARE_EQUAL_WITH_MSG(int, create_result, SOCKET_ASYNC_INVALID_SOCKET, "Unexpected create_result success");
                break;

            /* Tests_SRS_SOCKET_ASYNC_30_018: [ On success, socket_async_create shall return the created and configured SOCKET_ASYNC_HANDLE. ]*/
            case TP_TCP_SOCKET_OPT_SET_SYS_DEFAULTS_OK:
            case TP_TCP_CONNECT_IN_PROGRESS:
            case TP_TCP_CONNECT_SUCCESS:
            case TP_TCP_SOCKET_OPT_SET_OK:
                ASSERT_ARE_EQUAL_WITH_MSG(int, create_result, test_socket, "Unexpected create_result failure");
                break;
            default: ASSERT_FAIL("Unhandled test path")
            }

            // Do the keep-alive values match expectations?
            switch (test_path)
            {
            case TP_TCP_SOCKET_FAIL:
            case TP_TCP_SOCKET_OPT_0_FAIL:
            case TP_TCP_SOCKET_OPT_DEFAULT_FAIL:
            case TP_TCP_SOCKET_OPT_SET_SYS_DEFAULTS_OK: /* Tests_SRS_SOCKET_ASYNC_30_015: [ If the optional options parameter is non-NULL and is_UDP is false, and options->keep_alive is negative, socket_async_create not set the socket keep-alive options. ]*/
                ASSERT_KEEP_ALIVE_UNTOUCHED();
                break;

            case TP_TCP_SOCKET_OPT_1_FAIL:
            case TP_TCP_SOCKET_OPT_2_FAIL:
            case TP_TCP_SOCKET_OPT_3_FAIL:
                // These are various uninteresting intermediate states of failing to fully
                // set keep-alive values, and are not worth separate testing.
                break;

            case TP_TCP_SOCKET_OPT_SET_OK:  /* Tests_SRS_SOCKET_ASYNC_30_014: [ If the optional options parameter is non-NULL and is_UDP is false, socket_async_create shall set the socket options to the provided values. ]*/
                ASSERT_KEEP_ALIVE_SET();
                break;

                /* Tests_SRS_SOCKET_ASYNC_30_017: [ If the optional options parameter is NULL and is_UDP is false, socket_async_create shall disable TCP keep-alive. ]*/
            case TP_TCP_BIND_FAIL:
            case TP_TCP_CONNECT_FAIL:
            case TP_TCP_CONNECT_IN_PROGRESS:
            case TP_TCP_CONNECT_SUCCESS:
                ASSERT_KEEP_ALIVE_FALSE();
                break;
            default: ASSERT_FAIL("Unhandled test path")
            }

            end_assertions();   ////// End the Assertion phase and verify call sequence 
        }
    }

    TEST_FUNCTION(socket_async_create_udp_test)
    {

        for (test_path = TP_UDP_SOCKET_FAIL; test_path <= TP_UDP_CONNECT_SUCCESS; test_path++)
        {
            begin_arrange(test_path);   ////// Begin the Arrange phase     

            // The socket_async_create_udp test paths
            //// Create UDP
            // TP_UDP_SOCKET_FAIL,			// socket create fail
            // TP_UDP_BIND_FAIL,			// socket bind fail
            // TP_UDP_CONNECT_FAIL,		    // socket connect fail
            // TP_UDP_CONNECT_IN_PROGRESS,	// socket connect in progress
            // TP_UDP_CONNECT_SUCCESS,      // socket connect instant success
            //

            TEST_PATH(TP_UDP_SOCKET_FAIL, socket(AF_INET, SOCK_DGRAM, 0));
            TEST_PATH(TP_UDP_BIND_FAIL, bind(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
            TEST_PATH(TP_UDP_CONNECT_FAIL, connect(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG));

            begin_act(test_path);       ////// Begin the Act phase 

            /////////////////////////////////////////////////////////////////////////////////////////////////////
            // Set up input parameters
            /* Tests_SRS_SOCKET_ASYNC_30_013: [ The is_UDP parameter shall be true for a UDP connection, and false for TCP. ]*/
            bool is_udp = true;

            SOCKET_ASYNC_HANDLE create_result = socket_async_create(test_ipv4, test_port, is_udp, NULL);

            /////////////////////////////////////////////////////////////////////////////////////////////////////
            // Begin assertion phase

            // Does create_result match expectations?
            switch (test_path)
            {
            case TP_UDP_SOCKET_FAIL:        /* Tests_SRS_SOCKET_ASYNC_30_010: [ If socket option creation fails, socket_async_create shall log an error and return SOCKET_ASYNC_INVALID_SOCKET. ]*/
            case TP_UDP_BIND_FAIL:          /* Tests_SRS_SOCKET_ASYNC_30_021: [ If socket binding fails, socket_async_create shall log an error and return SOCKET_ASYNC_INVALID_SOCKET. ]*/
            case TP_UDP_CONNECT_FAIL:       /* Tests_SRS_SOCKET_ASYNC_30_022: [ If socket connection fails, socket_async_create shall log an error and return SOCKET_ASYNC_INVALID_SOCKET. ]*/
                ASSERT_ARE_EQUAL_WITH_MSG(int, create_result, SOCKET_ASYNC_INVALID_SOCKET, "Unexpected create_result success");
                break;

                /* Tests_SRS_SOCKET_ASYNC_30_018: [ On success, socket_async_create shall return the created and configured SOCKET_ASYNC_HANDLE. ]*/
            case TP_UDP_CONNECT_IN_PROGRESS:
            case TP_UDP_CONNECT_SUCCESS:
                ASSERT_ARE_EQUAL_WITH_MSG(int, create_result, test_socket, "Unexpected create_result failure");
                break;
            default: ASSERT_FAIL("Unhandled test path");
            }

            end_assertions();   ////// End the Assertion phase and verify call sequence 
        }
    }


END_TEST_SUITE(socket_async_ut)
