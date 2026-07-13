#ifndef USER_ERROR_H
#define USER_ERROR_H

#include "types.h"
#include "user_abi.h"

static inline u32 user_error(u32 err) { return USER_ERROR(err); }

static inline int user_is_error(u32 value) { return value >= USER_ERROR_BASE; }

#endif
