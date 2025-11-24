#ifndef ERROR_HANDLERS_H_INCLUDED
#define ERROR_HANDLERS_H_INCLUDED

enum error_type {
	ERROR_NO 			     = 0,
	ERROR_INCORRECT_ARGS     = 1 << 0,
    ERROR_MEM_ALLOC          = 1 << 1,
    ERROR_INCORRECT_INDEX    = 1 << 2,
	ERROR_INCORRECT_ARGS_CMD = 1 << 3,
	ERROR_INSERT_FAIL        = 1 << 4,
	ERROR_INVALID_STRUCTURE  = 1 << 5,
	ERROR_CANARY			 = 1 << 7,
	ERROR_NULL_ARG			 = 1 << 8,
	ERROR_BIG_SIZE			 = 1 << 9,
	ERROR_NON_ZERO_ELEM      = 1 << 10,
	ERROR_MISSED_ELEM		 = 1 << 11,
	ERROR_OPEN_FILE          = 1 << 12
};

typedef long error_code;

//==============================================================================

#define RETURN_IF_ERROR(error_, ...) 					\
	do {											 	\
		if((error_) != 0) { 							\
			LOGGER_ERROR("Error code: %lu", (error_));  \
			__VA_ARGS__									\
			return (error_); 							\
		}												\
	} while(0)

#endif