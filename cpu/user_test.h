#ifndef USER_TEST_H
#define USER_TEST_H

#include "types.h"

extern char user_test_msg[];
extern const u32 user_test_msg_len;
extern char user_test_bad_ptr_msg[];
extern const u32 user_test_bad_ptr_msg_len;

extern char user_test_input_buf[];
extern const u32 user_test_input_buf_len;
extern char user_test_prompt[];
extern const u32 user_test_prompt_len;
extern char user_test_got_msg[];
extern const u32 user_test_got_msg_len;

extern char user_fault_test_msg[];
extern const u32 user_fault_test_len;

void user_test_main(void);
void user_fault_test_main(void);

#endif
