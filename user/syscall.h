#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#define SYS_EXIT 2
#define SYS_WRITE 14

#define STDOUT 1

static inline unsigned int sys_write(int fd, const void *buf,
                                     unsigned int len) {
  unsigned int ret;
  __asm__ __volatile__("int $0x80"
                       : "=a"(ret)
                       : "a"(SYS_WRITE), "b"(fd), "c"(buf), "d"(len));

  return ret;
}

static inline void sys_exit(unsigned int code) {
  __asm__ __volatile__("int $0x80" : : "a"(SYS_EXIT), "b"(code));
  while (1) {
  }
}

#endif
