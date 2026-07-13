#ifndef USER_ABI_H
#define USER_ABI_H

#include "types.h"

#define USER_STAT_TYPE_FILE 1

#define USER_O_RDONLY 0
#define USER_O_WRONLY 1
#define USER_O_RDWR 2
#define USER_O_ACCMODE 3
#define USER_O_CREATE 4
#define USER_O_TRUNC 8
#define USER_O_APPEND 16

#define USER_SEEK_SET 0
#define USER_SEEK_CUR 1
#define USER_SEEK_END 2

#define USER_OK 0

#define USER_ERR_INVAL 1
#define USER_ERR_NOT_FOUND 2
#define USER_ERR_NO_SPACE 3
#define USER_ERR_IO 4
#define USER_ERR_UNSUPPORTED 5
#define USER_ERR_BADF 6
#define USER_ERR_PERM 7
#define USER_ERR_AGAIN 8

#define USER_ERROR_BASE 0xFFFFFF00
#define USER_ERROR(err) (0xFFFFFFFF - ((err) - 1))

typedef struct {
  u32 size;
  u32 type;
} user_stat_t;

#endif
