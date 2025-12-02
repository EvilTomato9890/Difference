#ifndef VOID_STACK_H_INCLUDED
#define VOID_STACK_H_INCLUDED

#include <limits.h>
#include <stdlib.h>
#include <stdint.h>

#include "debug_meta.h"
#include "error_handlers.h"
#include "node_info.h"

typedef void* void_st_type;

struct void_stack_t {
	ON_CANARY_DEBUG(
		int canary_begin;
	)

	void_st_type* data;
	void_st_type* original_ptr;
	ON_DEBUG(
		ver_info_t ver_info;
		bool is_constructed = true;
	)
	size_t size;
	size_t capacity;

	ON_CANARY_DEBUG(
		int canary_end;
	)
};

error_code void_stack_init(void_stack_t* void_stack_return, size_t capacity ON_DEBUG(, ver_info_t ver_info));

error_code void_stack_destroy(void_stack_t* void_stack);

error_code void_stack_verify(const void_stack_t* const void_stack);

error_code void_stack_push(void_stack_t* void_stack, void_st_type elem);

void_st_type void_stack_pop(void_stack_t* void_stack, error_code* error_return); //Лучше возвращать ошибку или значение?

error_code void_stack_clone(const void_stack_t* source, void_stack_t* dest ON_DEBUG(, ver_info_t ver_info));

#endif