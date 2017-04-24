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
 * The gballoc.h will replace the malloc, free, and realloc by the my_gballoc functions, in this case,
 *    if you define these mock functions after include the gballoc.h, you will create an infinity recursion,
 *    so, places the my_gballoc functions before the #include "azure_c_shared_utility/gballoc.h"
 */
void* my_gballoc_malloc(size_t size)
{
	return malloc(size);
}

void* my_gballoc_realloc(void* ptr, size_t size)
{
	return realloc(ptr, size);
}

void my_gballoc_free(void* ptr)
{
	free(ptr);
}

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
#include "umock_c_negative_tests.h"
#include "azure_c_shared_utility/macro_utils.h"

/**
 * Include the mockable headers here.
 * These are the headers that contains the functions that you will replace to execute the test.
 *
 * For instance, if you will test a target_create() function in the target.c that calls a callee_open() function 
 *   in the callee.c, you must define callee_open() as a mockable function in the callee.h.
 *
 * Observe that we will replace the functions in callee.h here, so we don't care about its real implementation,
 *   in fact, on this example, we even have the callee.c.
 *
 * Include all header files that you will replace the mockable functions in the ENABLE_MOCKS session below.
 *
 */
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

static int test_socket = (int)0x1;
int fcntl(int fd, int cmd, ... /* arg */) { (void)fd; (void)cmd; return 0; }
#define BAD_BUFFER_COUNT 10000
char test_msg[] = "Send this";

#include "test_points.h"
#include "keep_alive.h"

static TEST_POINT test_point;

// getsockopt is only used to retrieve extended errors, so this is simpler than
// it might be.
int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
    (void)sockfd;
    (void)level;
    (void)optname;
    (void)optlen;
    int result = EAGAIN;
    switch (test_point)
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
    switch (test_point)
    {
    case TP_TCP_IS_COMPLETE_ERRSET_FAIL: 
        FD_SET(nfds, exceptfds);
        break;
    case TP_TCP_IS_COMPLETE_READY_OK:
        FD_ZERO(exceptfds);
        FD_SET(nfds, writefds);
        break;
    //case TP_TCP_IS_COMPLETE_NOT_READY_OK:
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

/**
 * Tests begin here. Give a name for your test, for instance template_ut, use the same 
 *   name to close the test suite on END_TEST_SUITE(socket_async_ut), and to identify the  
 *   test suit in the main() function 
 *   
 *   RUN_TEST_SUITE(socket_async_ut, failedTestCount);
 *
 */
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
        
        //my_callee_open_must_succeed = true; //As default, callee_open will return a valid pointer.
    }

    /**
     * The test suite will call this function to cleanup your machine for the next test.
     * It is called after execute each test.
     */
    TEST_FUNCTION_CLEANUP(cleans)
    {
        TEST_MUTEX_RELEASE(g_testByTest);
    }

    // This main_sequence test performs all of the test passes that require sequencing of tlsio calls
    // To do this, it expands on the mocking framework's negative tests concept by adding
    // "test point", which are defined in the test_points.h file. These test points capture the 
    // process of testing the tlsio, and reading the test_points.h file first will make the following
    // function make a lot more sense.
    TEST_FUNCTION(socket_async_main_sequence)
    {

        for (test_point = 0; test_point <= TP_FINAL_OK; test_point++)
        {
            // Show the test point description in the output for the sake of 
            // human readability
            test_point_label_output(test_point);

            init_keep_alive_values();
            bool is_UDP = test_point <= TP_UDP_CONNECT_SUCCESS;

            /////////////////////////////////////////////////////////////////////////////
            ///arrange
            /////////////////////////////////////////////////////////////////////////////
            umock_c_reset_all_calls();

            InitTestPoints();

            int negativeTestsInitResult = umock_c_negative_tests_init();
            ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

            // The socket_async_create test points
            //// Create UDP
            // TP_UDP_SOCKET_FAIL,			// socket create fail
            // TP_UDP_BIND_FAIL,			// socket bind fail
            // TP_UDP_CONNECT_FAIL,		// socket connect fail
            // TP_UDP_CONNECT_IN_PROGRESS,	// socket connect in progress
            // TP_UDP_CONNECT_SUCCESS,     // socket connect instant success
            //// Create TCP
            // TP_TCP_SOCKET_FAIL,			// socket create fail
            // TP_TCP_SOCKET_OPT_0_FAIL,   // setsockopt set keep-alive fail 0
            // TP_TCP_SOCKET_OPT_1_FAIL,   // setsockopt set keep-alive fail 1
            // TP_TCP_SOCKET_OPT_2_FAIL,   // setsockopt set keep-alive fail 2
            // TP_TCP_SOCKET_OPT_3_FAIL,   // setsockopt set keep-alive fail 3
            // TP_TCP_SOCKET_OPT_DEFAULT_FAIL, // setsockopt default disable keep-alive fail
            // TP_TCP_SOCKET_OPT_SET_OK,   // setsockopt set keep-alive OK
            // TP_TCP_BIND_FAIL,			// socket bind fail
            // TP_TCP_CONNECT_FAIL,		// socket connect fail
            // TP_TCP_CONNECT_IN_PROGRESS,	// socket connect in progress
            // TP_TCP_CONNECT_SUCCESS,     // socket connect instant success
            //
            if (test_point <= TP_UDP_CONNECT_SUCCESS)
            {
                // The UDP Create test points
                //      TP_UDP_SOCKET_FAIL
                //      
                // These just test socket creation. There is no difference between UDP
                // and TCP socket use or destruction, so that is all handled by the TCP
                // test points
                
                /* Tests_SRS_SOCKET_ASYNC_30_013: [ The is_UDP parameter shall be true for a UDP connection, and false for TCP. ]*/
                TEST_POINT(TP_UDP_SOCKET_FAIL, socket(AF_INET, SOCK_DGRAM, 0));
                TEST_POINT(TP_UDP_BIND_FAIL, bind(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                TEST_POINT(TP_UDP_CONNECT_FAIL, connect(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
            }
            else
            {
                // The rest of the test points are TCP
                /* Tests_SRS_SOCKET_ASYNC_30_013: [ The is_UDP parameter shall be true for a UDP connection, and false for TCP. ]*/
                TEST_POINT(TP_TCP_SOCKET_FAIL, socket(AF_INET, SOCK_STREAM, 0));
                switch (test_point)
                {
                case TP_TCP_SOCKET_OPT_0_FAIL:
                case TP_TCP_SOCKET_OPT_1_FAIL:
                case TP_TCP_SOCKET_OPT_2_FAIL:
                case TP_TCP_SOCKET_OPT_3_FAIL:
                case TP_TCP_SOCKET_OPT_SET_OK:
                    // Here we are explicitly setting the keep-alive options
                    TEST_POINT(TP_TCP_SOCKET_OPT_0_FAIL, setsockopt(test_socket, IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                    TEST_POINT(TP_TCP_SOCKET_OPT_1_FAIL, setsockopt(test_socket, IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                    TEST_POINT(TP_TCP_SOCKET_OPT_2_FAIL, setsockopt(test_socket, IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                    TEST_POINT(TP_TCP_SOCKET_OPT_3_FAIL, setsockopt(test_socket, IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                    NO_FAIL_TEST_POINT(TP_TCP_SOCKET_OPT_SET_OK, bind(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                    NO_FAIL_TEST_POINT(TP_TCP_SOCKET_OPT_SET_OK, connect(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                    break;
                default:
                    // Here we are turning keep-alive off by default
                    TEST_POINT(TP_TCP_SOCKET_OPT_DEFAULT_FAIL, setsockopt(test_socket, IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                    break;
                }
                TEST_POINT(TP_TCP_BIND_FAIL, bind(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
                TEST_POINT(TP_TCP_CONNECT_FAIL, connect(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG));

            }

            // The socket_async_is_create_complete test points
            //
            // TP_TCP_IS_COMPLETE_NULL_PARAM_FAIL, // supplying a null is_complete
            // TP_TCP_IS_COMPLETE_SELECT_FAIL,     // the select call fails
            // TP_TCP_IS_COMPLETE_ERRSET_FAIL,     // a non-empty error set
            // TP_TCP_IS_COMPLETE_READY_OK,        // 
            // TTP_TCP_IS_COMPLETE_NOT_READY_OK,    // 
            //
            switch (test_point)
            {
            case TP_TCP_IS_COMPLETE_SELECT_FAIL:
                TEST_POINT(TP_TCP_IS_COMPLETE_SELECT_FAIL, select(test_socket + 1, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
                break;
            case TP_TCP_IS_COMPLETE_ERRSET_FAIL:
                NO_FAIL_TEST_POINT(TP_TCP_IS_COMPLETE_ERRSET_FAIL, select(test_socket + 1, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
                break;
            case TP_TCP_IS_COMPLETE_READY_OK:
                NO_FAIL_TEST_POINT(TP_TCP_IS_COMPLETE_READY_OK, select(test_socket + 1, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
                break;
            default:
                NO_FAIL_TEST_POINT(TP_TCP_IS_COMPLETE_NOT_READY_OK, select(test_socket + 1, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
                break;
            }


            // Send test points
            //
            // TP_SEND_NULL_BUFFER_FAIL,           // send with null buffer
            // TP_SEND_NULL_SENT_COUNT_FAIL,       // send with null sent count
            // TP_SEND_FAIL,                       // send failed
            // TP_SEND_WAITING_OK,                 // send not ready
            // TP_SEND_OK,     
            // 
            switch (test_point)
            {
            case TP_SEND_FAIL:
                TEST_POINT(TP_SEND_FAIL, send(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
                break;
            case TP_SEND_WAITING_OK:
                TEST_POINT(TP_SEND_WAITING_OK, send(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
                break;
            case TP_SEND_OK:
                NO_FAIL_TEST_POINT(TP_SEND_OK, send(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
                break;
            }

            // Receive test points
            //
            // TP_RECEIVE_NULL_BUFFER_FAIL,           // receive with null buffer
            // TP_RECEIVE_NULL_RECEIVED_COUNT_FAIL,   // receive with null receive count
            // TP_RECEIVE_FAIL,                       // receive failed
            // TP_RECEIVE_WAITING_OK,                 // receive not ready
            // TP_RECEIVE_OK,     
            // 
            switch (test_point)
            {
            case TP_RECEIVE_FAIL:
                TEST_POINT(TP_RECEIVE_FAIL, recv(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
                break;
            case TP_RECEIVE_WAITING_OK:
                TEST_POINT(TP_RECEIVE_WAITING_OK, recv(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
                break;
            case TP_RECEIVE_OK:
                NO_FAIL_TEST_POINT(TP_RECEIVE_OK, recv(test_socket, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
                break;
            }

            // Destroy never fails
            if (test_point == TP_DESTROY_OK)
            {
                /* Tests_SRS_SOCKET_ASYNC_30_071: [ socket_async_destroy shall call the underlying close method on the supplied socket. ]*/
                NO_FAIL_TEST_POINT(TP_DESTROY_OK, close(test_socket));
            }


            umock_c_negative_tests_snapshot();

            umock_c_negative_tests_reset();

            // Each test pass has no more than one place where umock_c_negative_tests_fail_call 
            // will force a failure.   
            uint16_t fail_index = test_points[test_point];
            if (fail_index != 0xffff)
            {
                umock_c_negative_tests_fail_call(fail_index);
            }

            //////////////////////////////////////////////////////////////////////////////////////////////////////
            //////////////////////////////////////////////////////////////////////////////////////////////////////
            ///act
            //////////////////////////////////////////////////////////////////////////////////////////////////////
            //////////////////////////////////////////////////////////////////////////////////////////////////////



            //////////////////////////////////////////////////////////////////////////////////////////////////////
            // The socket_async_create test points
            //// Create UDP
            // TP_UDP_SOCKET_FAIL,			// socket create fail
            // TP_UDP_BIND_FAIL,			// socket bind fail
            // TP_UDP_CONNECT_FAIL,		// socket connect fail
            // TP_UDP_CONNECT_IN_PROGRESS,	// socket connect in progress
            // TP_UDP_CONNECT_SUCCESS,     // socket connect instant success
            //// Create TCP
            // TP_TCP_SOCKET_FAIL,			// socket create fail
            // TP_TCP_SOCKET_OPT_0_FAIL,   // setsockopt set keep-alive fail 0
            // TP_TCP_SOCKET_OPT_1_FAIL,   // setsockopt set keep-alive fail 1
            // TP_TCP_SOCKET_OPT_2_FAIL,   // setsockopt set keep-alive fail 2
            // TP_TCP_SOCKET_OPT_3_FAIL,   // setsockopt set keep-alive fail 3
            // TP_TCP_SOCKET_OPT_DEFAULT_FAIL, // setsockopt default disable keep-alive fail
            // TP_TCP_SOCKET_OPT_SET_OK,   // setsockopt set keep-alive OK
            // TP_TCP_BIND_FAIL,			// socket bind fail
            // TP_TCP_CONNECT_FAIL,		// socket connect fail
            // TP_TCP_CONNECT_IN_PROGRESS,	// socket connect in progress
            // TP_TCP_CONNECT_SUCCESS,     // socket connect instant success
            //

            SOCKET_ASYNC_OPTIONS options_value = { test_keep_alive , test_keep_idle , test_keep_interval, test_keep_count };
            SOCKET_ASYNC_OPTIONS* options = NULL;
            if (test_point >= TP_TCP_SOCKET_OPT_0_FAIL && test_point <= TP_TCP_SOCKET_OPT_SET_OK)
            {
                options = &options_value;
            }
            SOCKET_ASYNC_HANDLE create_result = socket_async_create(0, 0, is_UDP, options);

            // Does create_result match expectations?
            switch (test_point)
            {
            case TP_UDP_SOCKET_FAIL:        /* Tests_SRS_SOCKET_ASYNC_30_010: [ If socket option creation fails, socket_async_create shall log an error and return SOCKET_ASYNC_INVALID_SOCKET. ]*/
            case TP_UDP_BIND_FAIL:          /* Tests_SRS_SOCKET_ASYNC_30_021: [ If socket binding fails, socket_async_create shall log an error and return SOCKET_ASYNC_INVALID_SOCKET. ]*/
            case TP_UDP_CONNECT_FAIL:       /* Tests_SRS_SOCKET_ASYNC_30_022: [ If socket connection fails, socket_async_create shall log an error and return SOCKET_ASYNC_INVALID_SOCKET. ]*/

            /* Tests_SRS_SOCKET_ASYNC_30_020: [ If socket option setting fails, the sock value shall be set to SOCKET_ASYNC_INVALID_SOCKET and socket_async_create shall log an error and return FAILURE. ]*/
            case TP_TCP_SOCKET_OPT_0_FAIL:
            case TP_TCP_SOCKET_OPT_1_FAIL:
            case TP_TCP_SOCKET_OPT_2_FAIL:
            case TP_TCP_SOCKET_OPT_3_FAIL:
            case TP_TCP_SOCKET_OPT_DEFAULT_FAIL:

            case TP_TCP_SOCKET_FAIL:    /* Tests_SRS_SOCKET_ASYNC_30_010: [ If socket option creation fails, socket_async_create shall log an error and return SOCKET_ASYNC_INVALID_SOCKET. ]*/
            case TP_TCP_BIND_FAIL:      /* Tests_SRS_SOCKET_ASYNC_30_021: [ If socket binding fails, socket_async_create shall log an error and return SOCKET_ASYNC_INVALID_SOCKET. ]*/
            case TP_TCP_CONNECT_FAIL:   /* Tests_SRS_SOCKET_ASYNC_30_022: [ If socket connection fails, socket_async_create shall log an error and return SOCKET_ASYNC_INVALID_SOCKET. ]*/
                if (create_result != SOCKET_ASYNC_INVALID_SOCKET)
                {
                    ASSERT_FAIL("Unexpected create_result success");
                }
                break;

            /* Tests_SRS_SOCKET_ASYNC_30_018: [ On success, socket_async_create shall return the created and configured SOCKET_ASYNC_HANDLE. ]*/
            default:
                if (create_result != test_socket)
                {
                    ASSERT_FAIL("Unexpected create_result failure");
                }
                break;
            }

            // Do the keep-alive values match expectations?
            switch (test_point)
            {
            /* Tests_SRS_SOCKET_ASYNC_30_015: [ If the optional options parameter is non-NULL and is_UDP is false, and options->keep_alive is negative, socket_async_create not set the socket keep-alive options. ]*/
            case TP_UDP_SOCKET_FAIL:
            case TP_UDP_BIND_FAIL:
            case TP_UDP_CONNECT_FAIL:
            case TP_UDP_CONNECT_IN_PROGRESS:
            case TP_UDP_CONNECT_SUCCESS:
            case TP_TCP_SOCKET_FAIL:
            case TP_TCP_SOCKET_OPT_0_FAIL:
            case TP_TCP_SOCKET_OPT_DEFAULT_FAIL:
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
            default:    /* Tests_SRS_SOCKET_ASYNC_30_017: [ If the optional options parameter is NULL and is_UDP is false, socket_async_create shall disable TCP keep-alive. ]*/
                // The default cases here are all TCP
                ASSERT_KEEP_ALIVE_FALSE();
                break;
            }
            // end socket_async_create
            /////////////////////////////////////////////////////////////////////////////////////////////////////




            //////////////////////////////////////////////////////////////////////////////////////////////////////
            // The socket_async_is_create_complete test points
            //
            // TP_TCP_IS_COMPLETE_NULL_PARAM_FAIL, // supplying a null is_complete
            // TP_TCP_IS_COMPLETE_SELECT_FAIL,     // the select call fails
            // TP_TCP_IS_COMPLETE_ERRSET_FAIL,     // a non-empty error set
            // TP_TCP_IS_COMPLETE_READY_OK,        // 
            // TTP_TCP_IS_COMPLETE_NOT_READY_OK,    // 
            //
            if (test_point >= TP_TCP_IS_COMPLETE_NULL_PARAM_FAIL)
            {
                // Set is_complete to the opposite of what's expected so we can spot a change
                bool is_complete = test_point != TP_TCP_IS_COMPLETE_READY_OK;
                bool* is_complete_param = test_point == TP_TCP_IS_COMPLETE_NULL_PARAM_FAIL ? NULL : &is_complete;

                int create_complete_result = socket_async_is_create_complete(test_socket, is_complete_param);

                // Does create_complete_result match expectations?
                switch (test_point)
                {
                case TP_TCP_IS_COMPLETE_NULL_PARAM_FAIL:    /* Tests_SRS_SOCKET_ASYNC_30_026: [ If the is_complete parameter is NULL, socket_async_is_create_complete shall log an error and return FAILURE. ]*/
                case TP_TCP_IS_COMPLETE_SELECT_FAIL:        /* Tests_SRS_SOCKET_ASYNC_30_028: [ On failure, the is_complete value shall be set to false and socket_async_create shall return FAILURE. ]*/
                case TP_TCP_IS_COMPLETE_ERRSET_FAIL:        /* Tests_SRS_SOCKET_ASYNC_30_028: [ On failure, the is_complete value shall be set to false and socket_async_create shall return FAILURE. ]*/
                    if (create_complete_result == 0)
                    {
                        ASSERT_FAIL("Unexpected returned create_complete_result value");
                    }
                    break;
                default:
                    if (create_complete_result != 0)
                    {
                        ASSERT_FAIL("Unexpected returned create_complete_result value");
                    }
                    break;
                }

                // Does is_compete match expectations?
                switch (test_point)
                {
                case TP_TCP_IS_COMPLETE_NOT_READY_OK:   /* Codes_SRS_SOCKET_ASYNC_30_027: [ On success, the is_complete value shall be set to the completion state and socket_async_create shall return 0. ]*/
                    if (is_complete)
                    {
                        ASSERT_FAIL("Unexpected returned is_complete value");
                    }
                    break;
                case TP_TCP_IS_COMPLETE_READY_OK:       /* Codes_SRS_SOCKET_ASYNC_30_027: [ On success, the is_complete value shall be set to the completion state and socket_async_create shall return 0. ]*/
                    if (!is_complete)
                    {
                        ASSERT_FAIL("Unexpected returned is_complete value");
                    }
                    break;
                }
            }
            // end socket_async_is_create_complete
            /////////////////////////////////////////////////////////////////////////////////////////////////////


            /////////////////////////////////////////////////////////////////////////////////////////////////////
            // Send test points
            //
            // TP_SEND_NULL_BUFFER_FAIL,           // send with null buffer
            // TP_SEND_NULL_SENT_COUNT_FAIL,       // send with null sent count
            // TP_SEND_FAIL,                       // send failed
            // TP_SEND_WAITING_OK,                 // send not ready
            // TP_SEND_OK,     
            // 
            if (test_point >= TP_SEND_NULL_BUFFER_FAIL && test_point <= TP_SEND_OK)
            {
                const char *buffer = test_point == TP_SEND_NULL_BUFFER_FAIL ? NULL : test_msg;
                size_t sent_count = BAD_BUFFER_COUNT;
                size_t *sent_count_param = test_point == TP_SEND_NULL_SENT_COUNT_FAIL ? NULL : &sent_count;
                int send_result = socket_async_send(test_socket, buffer, sizeof(test_msg), sent_count_param);

                // Does send_result match expectations>?
                switch (test_point)
                {
                case TP_SEND_NULL_BUFFER_FAIL:      /* Tests_SRS_SOCKET_ASYNC_30_033: [ If the buffer parameter is NULL, socket_async_send shall log the error return FAILURE. ]*/
                case TP_SEND_NULL_SENT_COUNT_FAIL:  /* Tests_SRS_SOCKET_ASYNC_30_034: [ If the sent_count parameter is NULL, socket_async_send shall log the error return FAILURE. ]*/
                case TP_SEND_FAIL:      /* Tests_SRS_SOCKET_ASYNC_30_037: [ If socket_async_send fails unexpectedly, socket_async_send shall log the error return FAILURE. ]*/
                    if (send_result == 0)
                    {
                        ASSERT_FAIL("Unexpected returned send_result value");
                    }
                    break;
                case TP_SEND_WAITING_OK:    /* Tests_SRS_SOCKET_ASYNC_30_036: [ If the underlying socket is unable to accept any bytes for transmission because its buffer is full, socket_async_send shall return 0 and the sent_count parameter shall receive the value 0. ]*/
                case TP_SEND_OK:            /* Tests_SRS_SOCKET_ASYNC_30_035: [ If the underlying socket accepts one or more bytes for transmission, socket_async_send shall return 0 and the sent_count parameter shall receive the number of bytes accepted for transmission. ]*/
                    if (send_result != 0)
                    {
                        ASSERT_FAIL("Unexpected returned send_result value");
                    }
                    break;
                }

                // Does sent_count match expectations>?
                switch (test_point)
                {
                /* Tests_SRS_SOCKET_ASYNC_30_036: [ If the underlying socket is unable to accept any bytes for transmission because its buffer is full, socket_async_send shall return 0 and the sent_count parameter shall receive the value 0. ]*/
                case TP_SEND_WAITING_OK:
                    if (sent_count != 0)
                    {
                        ASSERT_FAIL("Unexpected returned sent_count value");
                    }
                    break;

                /* Tests_SRS_SOCKET_ASYNC_30_035: [ If the underlying socket accepts one or more bytes for transmission, socket_async_send shall return 0 and the sent_count parameter shall receive the number of bytes accepted for transmission. ]*/
                case TP_SEND_OK:
                    if (sent_count != sizeof(test_msg))
                    {
                        ASSERT_FAIL("Unexpected returned sent_count value");
                    }
                    break;

                default:
                    if (sent_count != BAD_BUFFER_COUNT)
                    {
                        ASSERT_FAIL("Unexpected returned sent_count value");
                    }
                    break;
                }
            }
            // end send
            /////////////////////////////////////////////////////////////////////////////////////////////////////

            /////////////////////////////////////////////////////////////////////////////////////////////////////
            // Receive test points
            //
            // TP_RECEIVE_NULL_BUFFER_FAIL,           // receive with null buffer
            // TP_RECEIVE_NULL_RECEIVED_COUNT_FAIL,   // receive with null receive count
            // TP_RECEIVE_FAIL,                       // receive failed
            // TP_RECEIVE_WAITING_OK,                 // receive not ready
            // TP_RECEIVE_OK,     
            // 
            if (test_point >= TP_RECEIVE_NULL_BUFFER_FAIL && test_point <= TP_RECEIVE_OK)
            {
                char *buffer = test_point == TP_RECEIVE_NULL_BUFFER_FAIL ? NULL : test_msg;
                size_t received_count = BAD_BUFFER_COUNT;
                size_t *received_count_param = test_point == TP_RECEIVE_NULL_RECEIVED_COUNT_FAIL ? NULL : &received_count;
                int receive_result = socket_async_receive(test_socket, buffer, sizeof(test_msg), received_count_param);

                // Does receive_result match expectations>?
                switch (test_point)
                {
                case TP_RECEIVE_NULL_BUFFER_FAIL:   /* Tests_SRS_SOCKET_ASYNC_30_052: [ If the buffer parameter is NULL, socket_async_receive shall log the error and return FAILURE. ]*/
                case TP_RECEIVE_NULL_RECEIVED_COUNT_FAIL: /* Tests_SRS_SOCKET_ASYNC_30_053: [ If the received_count parameter is NULL, socket_async_receive shall log the error and return FAILURE. ]*/
                case TP_RECEIVE_FAIL:      /* Codes_SRS_SOCKET_ASYNC_30_056: [ If the underlying socket fails unexpectedly, socket_async_receive shall log the error and return FAILURE. ]*/
                    if (receive_result == 0)
                    {
                        ASSERT_FAIL("Unexpected returned send_result value");
                    }
                    break;
                case TP_RECEIVE_WAITING_OK:    /* Tests_SRS_SOCKET_ASYNC_30_055: [ If the underlying socket has no received bytes available, socket_async_receive shall return 0 and the received_count parameter shall receive the value 0. ]*/
                case TP_RECEIVE_OK:            /* Tests_SRS_SOCKET_ASYNC_30_054: [ On success, the underlying socket shall set one or more received bytes into buffer, socket_async_receive shall return 0, and the received_count parameter shall receive the number of bytes received into buffer. ]*/
                    if (receive_result != 0)
                    {
                        ASSERT_FAIL("Unexpected returned send_result value");
                    }
                    break;
                }

                // Does received_count match expectations>?
                switch (test_point)
                {
                /* Tests_SRS_SOCKET_ASYNC_30_055: [ If the underlying socket has no received bytes available, socket_async_receive shall return 0 and the received_count parameter shall receive the value 0. ]*/
                case TP_RECEIVE_WAITING_OK:
                    if (received_count != 0)
                    {
                        ASSERT_FAIL("Unexpected returned received_count value");
                    }
                    break;

                /* Tests_SRS_SOCKET_ASYNC_30_054: [ On success, the underlying socket shall set one or more received bytes into buffer, socket_async_receive shall return 0, and the received_count parameter shall receive the number of bytes received into buffer. ]*/
                case TP_RECEIVE_OK:
                    if (received_count != sizeof(test_msg))
                    {
                        ASSERT_FAIL("Unexpected returned received_count value");
                    }
                    break;

                default:
                    if (received_count != BAD_BUFFER_COUNT)
                    {
                        ASSERT_FAIL("Unexpected returned received_count value");
                    }
                    break;
                }
            }
            //
            // end receive
            /////////////////////////////////////////////////////////////////////////////////////////////////////


            /////////////////////////////////////////////////////////////////////////////////////////////////////
            // socket_async_destroy (never fails)
            if (test_point == TP_DESTROY_OK)
            {
                /* Tests_SRS_SOCKET_ASYNC_30_071: [ socket_async_destroy shall call the underlying close method on the supplied socket. ]*/
                socket_async_destroy(test_socket);
            }
            // end socket_async_destroy
            /////////////////////////////////////////////////////////////////////////////////////////////////////


            /////////////////////////////////////////////////////////////////////////////////////////////////////
            /////////////////////////////////////////////////////////////////////////////////////////////////////
            ///assert
            /////////////////////////////////////////////////////////////////////////////////////////////////////
            /////////////////////////////////////////////////////////////////////////////////////////////////////

            // The assert section is sparse because most of the assertions have been done in the "act" stage.

            /**
            * The follow assert will compare the expected calls with the actual calls. If it is different,
            *    it will show the serialized strings with the differences in the log.
            */
            ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

            ///cleanup
            umock_c_negative_tests_deinit();

        }
    }


END_TEST_SUITE(socket_async_ut)
