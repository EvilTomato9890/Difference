
#ifndef LIST_OPERATIONS_H_INCLUDED
#define LIST_OPERATIONS_H_INCLUDED

#include "list_info.h"

error_code list_init(list_t* list,
                     size_t capacity
                     ON_DEBUG(, ver_info_t ver_info));


error_code list_dest(list_t* list);

ssize_t list_insert_after(list_t* list, ssize_t insert_index, tree_t* val);

ssize_t list_insert_auto(list_t* list, ssize_t insert_index, tree_t* val);

ssize_t list_insert_before(list_t* list, ssize_t insert_index, tree_t* val);

error_code list_pop_back(list_t* list);
ssize_t list_push_back(list_t* list, tree_t* val);

error_code list_pop_front(list_t* list);
ssize_t list_push_front(list_t* list, tree_t* val);

error_code list_remove(list_t* list, ssize_t remove_index);

error_code list_remove_auto(list_t* list, ssize_t remove_index);

error_code list_swap(list_t* list, ssize_t first_idx, ssize_t second_idx);

error_code list_linearize(list_t* list);

error_code list_shrink_to_fit(list_t* list, bool keep_growth);
#endif 
