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
typedef enum TEST_PATH_TAG
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
    TP_TCP_SOCKET_OPT_SET_SYS_DEFAULTS_OK,   // setsockopt use system defaults for keep-alive
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

    // NOTE!!!! Update test_path_names below when adding to this enum
    TP_FINAL_OK     // Always keep as last entry
} TEST_PATH;

typedef struct X {
    int fp;
    const char* name;
} X;

#define TEST_PATH_NAME(p) { p, #p },

// The list of test_path_names is to help human-readability of the output
static X test_path_names[] =
{
    // Create UDP
    TEST_PATH_NAME(TP_UDP_SOCKET_FAIL)
    TEST_PATH_NAME(TP_UDP_BIND_FAIL)
    TEST_PATH_NAME(TP_UDP_CONNECT_FAIL)
    TEST_PATH_NAME(TP_UDP_CONNECT_IN_PROGRESS)
    TEST_PATH_NAME(TP_UDP_CONNECT_SUCCESS)

    // Create TCP
    TEST_PATH_NAME(TP_TCP_SOCKET_FAIL)
    TEST_PATH_NAME(TP_TCP_SOCKET_OPT_0_FAIL)
    TEST_PATH_NAME(TP_TCP_SOCKET_OPT_1_FAIL)
    TEST_PATH_NAME(TP_TCP_SOCKET_OPT_2_FAIL)
    TEST_PATH_NAME(TP_TCP_SOCKET_OPT_3_FAIL)
    TEST_PATH_NAME(TP_TCP_SOCKET_OPT_DEFAULT_FAIL)
    TEST_PATH_NAME(TP_TCP_SOCKET_OPT_SET_OK)
    TEST_PATH_NAME(TP_TCP_SOCKET_OPT_SET_SYS_DEFAULTS_OK)

    TEST_PATH_NAME(TP_TCP_BIND_FAIL)
    TEST_PATH_NAME(TP_TCP_CONNECT_FAIL)
    TEST_PATH_NAME(TP_TCP_CONNECT_IN_PROGRESS)
    TEST_PATH_NAME(TP_TCP_CONNECT_SUCCESS)

    // Is create complete
    TEST_PATH_NAME(TP_TCP_IS_COMPLETE_NULL_PARAM_FAIL)
    TEST_PATH_NAME(TP_TCP_IS_COMPLETE_SELECT_FAIL)
    TEST_PATH_NAME(TP_TCP_IS_COMPLETE_ERRSET_FAIL)
    TEST_PATH_NAME(TP_TCP_IS_COMPLETE_READY_OK)
    TEST_PATH_NAME(TP_TCP_IS_COMPLETE_NOT_READY_OK)

    // Send
    TEST_PATH_NAME(TP_SEND_NULL_BUFFER_FAIL)
    TEST_PATH_NAME(TP_SEND_NULL_SENT_COUNT_FAIL)
    TEST_PATH_NAME(TP_SEND_FAIL)
    TEST_PATH_NAME(TP_SEND_WAITING_OK)
    TEST_PATH_NAME(TP_SEND_OK)

    // Receive
    TEST_PATH_NAME(TP_RECEIVE_NULL_BUFFER_FAIL)
    TEST_PATH_NAME(TP_RECEIVE_NULL_RECEIVED_COUNT_FAIL)
    TEST_PATH_NAME(TP_RECEIVE_FAIL)
    TEST_PATH_NAME(TP_RECEIVE_WAITING_OK)
    TEST_PATH_NAME(TP_RECEIVE_OK)

    // Destroy is a pass-thru, and not really testable
    TEST_PATH_NAME(TP_DESTROY_OK)

    TEST_PATH_NAME(TP_FINAL_OK)
};


static void test_path_label_output(TEST_PATH fp)
{
    printf("\n\nTest point: %d  %s\n", fp, test_path_names[fp].name);
}


// test_paths is a lookup table that provides an index 
// to pass to umock_c_negative_tests_fail_call(0) given
// a provided fail point enum value. If the index is 255,
// that means don't call umock_c_negative_tests_fail_call().
static uint16_t test_paths[TP_FINAL_OK + 1];
static uint16_t expected_call_count = 0;


static void InitTestPoints()
{
    expected_call_count = 0;
    memset(test_paths, 0xff, sizeof(test_paths));
}

static void begin_arrange(TEST_PATH test_path)
{
    // Show the test point description in the output for the sake of 
    // human readability
    printf("\n\nTest path: %d  %s\n", test_path, test_path_names[test_path].name);

    // Init the test_paths
    expected_call_count = 0;
    memset(test_paths, 0xff, sizeof(test_paths));

    // Init the negative mocks
    umock_c_reset_all_calls();
    int negativeTestsInitResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

}

static void begin_act(TEST_PATH test_path)
{
    umock_c_negative_tests_snapshot();

    umock_c_negative_tests_reset();

    // Each test pass has no more than one place where umock_c_negative_tests_fail_call 
    // will force a failure.   
    uint16_t fail_index = test_paths[test_path];
    if (fail_index != 0xffff)
    {
        umock_c_negative_tests_fail_call(fail_index);
    }
}

static void end_assertions()
{
    /**
    * The follow assert will compare the expected calls with the actual calls. If it is different,
    *    it will show the serialized strings with the differences in the log.
    */
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    ///cleanup
    umock_c_negative_tests_deinit();
}



// TEST_PATH means that the call is expected at the provided fail point and beyond,
// and the framework will fail the call the first time it hits it.
// The messy macro on line 2 of TEST_PATH is the expansion of STRICT_EXPECTED_CALL
#define TEST_PATH(fp, call) if(test_path >= fp) {  \
    C2(get_auto_ignore_args_function_, call)(C2(umock_c_strict_expected_,call), #call);			\
    test_paths[fp] = expected_call_count;	\
    expected_call_count++;		\
}

// TEST_PATH_NO_FAIL means that this call is expected at the provided test point and beyond,
// and the framework will not fail the call.
// The messy macro on line 2 of TEST_PATH_NO_FAIL is the expansion of STRICT_EXPECTED_CALL
#define TEST_PATH_NO_FAIL(fp, call) if(test_path >= fp) {  \
    C2(get_auto_ignore_args_function_, call)(C2(umock_c_strict_expected_,call), #call);			\
    expected_call_count++;		\
}

// TEAR_DOWN_POINT means that this call is expected everywhere past the provided
// test point, and the framework will not fail the call. The semantics of this call are only
// slightly different from TEST_PATH_NO_FAIL, but this semantic improves readability
// for setting up calls such as Close which are part of a tear-down process.
// The messy macro on line 2 of TEAR_DOWN_POINT is the expansion of STRICT_EXPECTED_CALL
#define TEAR_DOWN_POINT(fp, call) if(test_path > fp) {  \
    C2(get_auto_ignore_args_function_, call)(C2(umock_c_strict_expected_,call), #call);			\
    expected_call_count++;		\
}
