#include "user_test.h"
#include "user_syscall.h"

char user_test_msg[] = "ser mode says hi\n";
char user_test_bad_ptr_msg[] = "bad pointer rejected\n";
const u32 user_test_msg_len = sizeof(user_test_msg);
const u32 user_test_bad_ptr_msg_len = sizeof(user_test_bad_ptr_msg);

char user_test_input_buf[64];
char user_test_prompt[] = "type line: ";
char user_test_got_msg[] = "you typed: ";
const u32 user_test_input_buf_len = sizeof(user_test_input_buf);
const u32 user_test_prompt_len = sizeof(user_test_prompt);
const u32 user_test_got_msg_len = sizeof(user_test_got_msg);

void user_test_main(void) {
  user_write_char('U');
  user_write_string(user_test_msg);

  u32 ret = user_write_buffer((char *)0xFFFFFFFF, 5);
  if (ret == 0xFFFFFFFF) {
    user_write_string(user_test_bad_ptr_msg);
  }

  u32 n = 0;
  user_write_string(user_test_prompt);
  while (n == 0) {
    user_yield();
    n = user_read_line(user_test_input_buf, user_test_input_buf_len);
  }

  if (n != 0xFFFFFFFF) {
    user_write_string(user_test_got_msg);
    user_write_string(user_test_input_buf);
    user_write_char('\n');
  }

  u32 *bad = (u32 *)0xFFFFFFFF;
  u32 x = *bad;
  (void)x;

  user_exit(0);
}
