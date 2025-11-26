#ifndef STACK_H_INCLUDED
#define STACK_H_INCLUDED

#include <limits.h>
#include <stdlib.h>
#include <stdint.h>

#include "error_handlers.h"
#include "node_info.h"
#include "debug_meta.h"

typedef variable_t st_type;

struct stack_t {
	ON_CANARY_DEBUG(
		int canary_begin;
	)

	st_type* data;
	st_type* original_ptr;
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

error_code stack_init(stack_t* stack_return, size_t capacity ON_DEBUG(, ver_info_t ver_info));

error_code stack_destroy(stack_t* stack);

error_code stack_verify(const stack_t* const stack);

error_code stack_push(stack_t* stack, st_type elem);

st_type stack_pop(stack_t* stack, error_code* error_return); //Лучше возвращать ошибку или значение?

error_code stack_clone(const stack_t* source, stack_t* dest ON_DEBUG(, ver_info_t ver_info));

#endif