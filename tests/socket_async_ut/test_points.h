// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// This file is made an integral part of socket_async_ut.c with a #include. It
// is broken out for readability. 



// This is a list of all the possible test points plus the various happy paths for a 
// create/read/write/destroy sequence. 
// Test points that look like XXXXX_OK are actually a success path. If there is no
// "_OK" in the name, then that test point is an expected failure.
// Test points that look like XXXXX_1 or XXXXX_OK_1 are one of a group of different ways
// to either succeed or fail at that test point
typedef enum TEST_POINT_TAG
{
    // Create UDP
    TP_UDP_SOCKET_FAIL,			// socket create fail
    TP_UDP_BIND_FAIL,			// socket bind fail
    TP_UDP_CONNECT_FAIL,		// socket connect fail
    TP_UDP_CONNECT_IN_PROGRESS,	// socket connect in progress
    TP_UDP_CONNECT_SUCCESS,     // socket connect instant success

    // Create TCP
    TP_TCP_SOCKET_FAIL,			// socket create fail
    TP_TCP_SOCKET_OPT_0_FAIL,   // setsockopt set keep-alive fail 0
    TP_TCP_SOCKET_OPT_1_FAIL,   // setsockopt set keep-alive fail 1
    TP_TCP_SOCKET_OPT_2_FAIL,   // setsockopt set keep-alive fail 2
    TP_TCP_SOCKET_OPT_3_FAIL,   // setsockopt set keep-alive fail 3
    TP_TCP_SOCKET_OPT_DEFAULT_FAIL, // setsockopt default disable keep-alive fail
    TP_TCP_SOCKET_OPT_SET_OK,   // setsockopt set keep-alive OK
    TP_TCP_BIND_FAIL,			// socket bind fail
    TP_TCP_CONNECT_FAIL,		// socket connect fail
    TP_TCP_CONNECT_IN_PROGRESS,	// socket connect in progress
    TP_TCP_CONNECT_SUCCESS,     // socket connect instant success

    // Is create complete
    TP_TCP_IS_COMPLETE_NULL_PARAM_FAIL, // supplying a null is_complete
    TP_TCP_IS_COMPLETE_SELECT_FAIL,     // the select call fails
    TP_TCP_IS_COMPLETE_ERRSET_FAIL,     // a non-empty error set
    TP_TCP_IS_COMPLETE_READY_OK,        // 
    TP_TCP_IS_COMPLETE_NOT_READY_OK,    // 

    // Send
    TP_SEND_NULL_BUFFER_FAIL,           // send with null buffer
    TP_SEND_NULL_SENT_COUNT_FAIL,       // send with null sent count
    TP_SEND_FAIL,                       // send failed
    TP_SEND_WAITING_OK,                 // send not ready
    TP_SEND_OK,                         // send worked

    // Receive
    TP_RECEIVE_NULL_BUFFER_FAIL,           // receive with null buffer
    TP_RECEIVE_NULL_RECEIVED_COUNT_FAIL,   // receive with null received count
    TP_RECEIVE_FAIL,                       // receive failed
    TP_RECEIVE_WAITING_OK,                 // receive not ready
    TP_RECEIVE_OK,                         // receive worked


    // Destroy is a pass-thru, and not really testable
    TP_DESTROY_OK,

    // NOTE!!!! Update test_point_names below when adding to this enum
    TP_FINAL_OK     // Always keep as last entry
} TEST_POINT;

typedef struct X {
    int fp;
    const char* name;
} X;

#define TEST_POINT_NAME(p) { p, #p },

// The list of test_point_names is to help human-readability of the output
static X test_point_names[] =
{
    // Create UDP
    TEST_POINT_NAME(TP_UDP_SOCKET_FAIL)
    TEST_POINT_NAME(TP_UDP_BIND_FAIL)
    TEST_POINT_NAME(TP_UDP_CONNECT_FAIL)
    TEST_POINT_NAME(TP_UDP_CONNECT_IN_PROGRESS)
    TEST_POINT_NAME(TP_UDP_CONNECT_SUCCESS)

    // Create TCP
    TEST_POINT_NAME(TP_TCP_SOCKET_FAIL)
    TEST_POINT_NAME(TP_TCP_SOCKET_OPT_0_FAIL)
    TEST_POINT_NAME(TP_TCP_SOCKET_OPT_1_FAIL)
    TEST_POINT_NAME(TP_TCP_SOCKET_OPT_2_FAIL)
    TEST_POINT_NAME(TP_TCP_SOCKET_OPT_3_FAIL)
    TEST_POINT_NAME(TP_TCP_SOCKET_OPT_DEFAULT_FAIL)
    TEST_POINT_NAME(TP_TCP_SOCKET_OPT_SET_OK)

    TEST_POINT_NAME(TP_TCP_BIND_FAIL)
    TEST_POINT_NAME(TP_TCP_CONNECT_FAIL)
    TEST_POINT_NAME(TP_TCP_CONNECT_IN_PROGRESS)
    TEST_POINT_NAME(TP_TCP_CONNECT_SUCCESS)

    // Is create complete
    TEST_POINT_NAME(TP_TCP_IS_COMPLETE_NULL_PARAM_FAIL)
    TEST_POINT_NAME(TP_TCP_IS_COMPLETE_SELECT_FAIL)
    TEST_POINT_NAME(TP_TCP_IS_COMPLETE_ERRSET_FAIL)
    TEST_POINT_NAME(TP_TCP_IS_COMPLETE_READY_OK)
    TEST_POINT_NAME(TP_TCP_IS_COMPLETE_NOT_READY_OK)

    // Send
    TEST_POINT_NAME(TP_SEND_NULL_BUFFER_FAIL)
    TEST_POINT_NAME(TP_SEND_NULL_SENT_COUNT_FAIL)
    TEST_POINT_NAME(TP_SEND_FAIL)
    TEST_POINT_NAME(TP_SEND_WAITING_OK)
    TEST_POINT_NAME(TP_SEND_OK)

    // Receive
    TEST_POINT_NAME(TP_RECEIVE_NULL_BUFFER_FAIL)
    TEST_POINT_NAME(TP_RECEIVE_NULL_RECEIVED_COUNT_FAIL)
    TEST_POINT_NAME(TP_RECEIVE_FAIL)
    TEST_POINT_NAME(TP_RECEIVE_WAITING_OK)
    TEST_POINT_NAME(TP_RECEIVE_OK)

    // Destroy is a pass-thru, and not really testable
    TEST_POINT_NAME(TP_DESTROY_OK)

    TEST_POINT_NAME(TP_FINAL_OK)
};


static void test_point_label_output(TEST_POINT fp)
{
    printf("\n\nTest point: %d  %s\n", fp, test_point_names[fp].name);
}


// test_points is a lookup table that provides an index 
// to pass to umock_c_negative_tests_fail_call(0) given
// a provided fail point enum value. If the index is 255,
// that means don't call umock_c_negative_tests_fail_call().
static uint16_t test_points[TP_FINAL_OK + 1];
static uint16_t expected_call_count = 0;


static void InitTestPoints()
{
    expected_call_count = 0;
    memset(test_points, 0xff, sizeof(test_points));
}


// TEST_POINT means that the call is expected at the provided fail point and beyond,
// and the framework will fail the call the first time it hits it.
// The messy macro on line 2 of TEST_POINT is the expansion of STRICT_EXPECTED_CALL
#define TEST_POINT(fp, call) if(test_point >= fp) {  \
    C2(get_auto_ignore_args_function_, call)(C2(umock_c_strict_expected_,call), #call);			\
    test_points[fp] = expected_call_count;	\
    expected_call_count++;		\
}

// NO_FAIL_TEST_POINT means that this call is expected at the provided test point and beyond,
// and the framework will not fail the call.
// The messy macro on line 2 of NO_FAIL_TEST_POINT is the expansion of STRICT_EXPECTED_CALL
#define NO_FAIL_TEST_POINT(fp, call) if(test_point >= fp) {  \
    C2(get_auto_ignore_args_function_, call)(C2(umock_c_strict_expected_,call), #call);			\
    expected_call_count++;		\
}

// TEAR_DOWN_POINT means that this call is expected everywhere past the provided
// test point, and the framework will not fail the call. The semantics of this call are only
// slightly different from NO_FAIL_TEST_POINT, but this semantic improves readability
// for setting up calls such as Close which are part of a tear-down process.
// The messy macro on line 2 of TEAR_DOWN_POINT is the expansion of STRICT_EXPECTED_CALL
#define TEAR_DOWN_POINT(fp, call) if(test_point > fp) {  \
    C2(get_auto_ignore_args_function_, call)(C2(umock_c_strict_expected_,call), #call);			\
    expected_call_count++;		\
}
