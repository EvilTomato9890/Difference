#include "list_info.h"
#include "logger.h"
#include "asserts.h"
#include "error_handler.h"
#include "list_operations.h"
#include <cstdlib>
#include <cstring>

#include "list_verification.h"

//==============================================================================

static void init_free_list(list_t* list, size_t start_index, size_t end_index) ;
static error_code list_recalloc(list_t* list, size_t new_capacity) ; 
static error_code normalize_capacity(list_t* list);
static error_code list_reorganize_free(list_t* list);

//==============================================================================

static void init_free_list(list_t* list, size_t start_index, size_t end_index) {
    HARD_ASSERT(list      != nullptr, "list is nullptr");
    HARD_ASSERT(list->arr != nullptr, "arr is nullptr");
    for (size_t i = start_index; i + 1 < end_index; ++i) {
        list->arr[i].next = i + 1;
        list->arr[i].prev = -1;
        list->arr[i].val  = POISON;
    }
    list->arr[end_index - 1].next = -1;
    list->arr[end_index - 1].prev = -1;
    list->arr[end_index - 1].val  = POISON;
}

static error_code list_recalloc(list_t* list, size_t new_capacity) {
    HARD_ASSERT(list      != nullptr, "list is nullptr");
    HARD_ASSERT(list->arr != nullptr, "arr is nullptr");

    error_code error = 0;
    ON_DEBUG(
        error |= list_verify(list, VER_INIT, DUMP_IMG, "Before recalloc to %lu", new_capacity);
        if (error != ERROR_NO) return error;
    )

    if (new_capacity == 0) {
        LOGGER_WARNING("Attempting to realloc to zero capacity");
    }

    size_t alloc_count = new_capacity ON_DEBUG(+ 1);
    size_t new_bytes   = alloc_count * sizeof(node_t);
    LOGGER_DEBUG("Reallocating %lu bytes for list", new_bytes);

    node_t* new_block = (node_t*)(realloc(list->arr, new_bytes));
    if (new_block == nullptr) {
        LOGGER_ERROR("Realloc failed");
        return ERROR_MEM_ALLOC;
    }
    list->arr = new_block;
    new_block[0].val = CANARY_NUM;
    ON_DEBUG(
        new_block[new_capacity].val = CANARY_NUM;
    )

    size_t old_capacity = list->capacity;
    if (list->free_head == -1) {
        list->free_head = old_capacity;
        init_free_list(list, old_capacity, new_capacity);
    } else {
        ssize_t last_free = list->free_head;
        while (last_free != -1 && list->arr[last_free].next != -1) {
            last_free = list->arr[last_free].next;
        }
        if (last_free != -1) {
            list->arr[last_free].next = old_capacity;
        }
        init_free_list(list, old_capacity, new_capacity);
    }
    list->capacity = new_capacity;

    ON_DEBUG(
        error |= list_verify(list, VER_INIT, DUMP_IMG, "After recalloc to %lu", new_capacity);
    )
    return error;
}

static error_code normalize_capacity(list_t* list) {
    HARD_ASSERT(list      != nullptr, "list is nullptr");
    HARD_ASSERT(list->arr != nullptr, "arr  is nullptr");
    LOGGER_DEBUG("Normalising capacity (size: %lu, capacity: %lu)",
                 list->size, list->capacity);

    if (list->size + 1 > list->capacity) {
        size_t new_capacity = (size_t)((double)list->capacity * GROWTH_FACTOR);
        LOGGER_DEBUG("Growing list to capacity %lu", new_capacity);
        return list_recalloc(list, new_capacity);
    }
    return ERROR_NO;
}

//==============================================================================

error_code list_init(list_t* list_return, size_t capacity ON_DEBUG(, ver_info_t ver_info)) {
    HARD_ASSERT(list_return != nullptr, "list_return is nullptr");
    LOGGER_DEBUG("Initialising list with requested capacity %lu", capacity);

    error_code error = 0;
    if (capacity < MIN_LIST_SIZE) {
        LOGGER_INFO("Requested capacity too small, adjusting to %lu", MIN_LIST_SIZE);
        capacity = MIN_LIST_SIZE;
    }

    size_t alloc_count = capacity ON_DEBUG(+ 1); 
    LOGGER_DEBUG("Allocating %lu nodes", alloc_count);
    node_t* arr = (node_t*)(calloc(alloc_count, sizeof(node_t)));
    if (arr == nullptr) {
        LOGGER_ERROR("calloc failed during list initialisation");
        return ERROR_MEM_ALLOC;
    }

    for (size_t i = 0; i + 1 < capacity; ++i) {
        arr[i].next = i + 1;
        arr[i].prev = -1;
        arr[i].val  = POISON;
    }
    arr[capacity - 1].next = -1;
    arr[capacity - 1].prev = -1;
    arr[capacity - 1].val  = POISON;

    arr[0].next = 0;
    arr[0].prev = 0;
    arr[0].val = CANARY_NUM;

    ON_DEBUG(
        arr[capacity].val = CANARY_NUM;
    ) 

    list_t list = {};
    list.arr       = arr;
    list.capacity  = capacity;
    list.size      = 1;          
    list.head      = 0;
    list.tail      = 0;
    list.free_head = 1;
    ON_DEBUG(
        list.ver_info = ver_info;
    )

    *list_return = list;
    ON_DEBUG(
        error |= list_verify(&list, VER_INIT, DUMP_IMG, "After list_init");
    )
    return error;
}

error_code list_dest(list_t* list) {
    HARD_ASSERT(list      != nullptr, "list is nullptr");
    HARD_ASSERT(list->arr != nullptr, "list->arr is nullptr");
    LOGGER_DEBUG("Destroying list");
    error_code error = ERROR_NO;

    free(list->arr);
    list->arr = nullptr;
    list->capacity = 0;
    list->size = 0;
    list->head = list->tail = list->free_head = 0;
    return error;
}

ssize_t list_insert_after(list_t* list, ssize_t insert_index, double val) {
    HARD_ASSERT(list      != nullptr, "list is nullptr");
    HARD_ASSERT(list->arr != nullptr, "arr is nullptr");
    LOGGER_DEBUG("Inserting value %lf after index %d", val, insert_index);

    ON_DEBUG(
        error_code error = list_verify(list, VER_INIT, DUMP_IMG, "Before insert of %lf at index %d", val, insert_index);
        if (error != ERROR_NO) {
            return -1;
        }
    )
    if (insert_index < 0 || (size_t)(insert_index) >= list->capacity) {
        LOGGER_ERROR("insert_index %d out of bounds", insert_index);
        return -1;
    }

    ssize_t free_index = list->free_head;
    if (free_index == -1) {
        LOGGER_ERROR("No free cells available, grow required");
        error_code grow_err = normalize_capacity(list);
        if (grow_err != ERROR_NO) {
            return -1;
        }
        free_index = list->free_head;
        if (free_index == -1) {
            return -1;
        }
    }
    list->free_head = list->arr[free_index].next;

    ssize_t next_index  = list->arr[insert_index].next;
    list->arr[free_index].val  = val;
    list->arr[free_index].next = next_index;
    list->arr[free_index].prev = insert_index;

    list->arr[insert_index].next = free_index;
    list->arr[next_index].prev   = free_index;

    list->head = list->arr[0].next;
    list->tail = list->arr[0].prev;

    list->size++;
    ON_DEBUG(
        error |= list_verify(list, VER_INIT, DUMP_IMG, "After insert of %lf at index %d", val, insert_index);
        if (error != ERROR_NO) {
            return -1;
        }
    )

    error_code cap_err = normalize_capacity(list);
    if (cap_err != ERROR_NO) {
        LOGGER_ERROR("Failed to normalise capacity after insert");
        return -1;
    }

    return free_index;
}

ssize_t list_insert_auto(list_t* list, ssize_t insert_index, double val) {
    HARD_ASSERT(list      != nullptr, "list is nullptr");
    HARD_ASSERT(list->arr != nullptr, "arr is nullptr");

    if (insert_index < 0 || (size_t)insert_index > list->size) {
        LOGGER_ERROR("list_insert_auto: insert_index %d out of range", insert_index);
        return -1;
    }
    ssize_t physical = list->head;
    for (ssize_t i = 0; i < insert_index; ++i) {
        physical = list->arr[physical].next;
    }
    return list_insert_after(list, physical, val);
}

ssize_t list_insert_before(list_t* list, ssize_t insert_index, double val) {
    HARD_ASSERT(list      != nullptr, "list is nullptr");
    HARD_ASSERT(list->arr != nullptr, "arr is nullptr");
    LOGGER_DEBUG("Inserting before physical index %d", insert_index);

    if (insert_index < 0 || (size_t)(insert_index) >= list->capacity) {
        LOGGER_ERROR("list_insert_before: insert_index %d invalid", insert_index);
        return -1;
    }
    ssize_t prev_index = list->arr[insert_index].prev;
    return list_insert_after(list, prev_index, val);
}

ssize_t list_push_back(list_t* list, double val) {
    HARD_ASSERT(list      != nullptr, "list is nullptr");
    HARD_ASSERT(list->arr != nullptr, "arr is nullptr");
    LOGGER_DEBUG("Pushing back value %lf", val);
    return list_insert_before(list, 0, val);
}

ssize_t list_push_front(list_t* list, double val) {
    HARD_ASSERT(list      != nullptr, "list is nullptr");
    HARD_ASSERT(list->arr != nullptr, "arr is nullptr");
    LOGGER_DEBUG("Pushing front value %lf", val);
    return list_insert_after(list, 0, val);
}

error_code list_remove(list_t* list, ssize_t remove_index) {
    HARD_ASSERT(list      != nullptr, "list is nullptr");
    HARD_ASSERT(list->arr != nullptr, "arr is nullptr");
    LOGGER_DEBUG("Removing node at physical index %d", remove_index);

    error_code error = 0;
    ON_DEBUG(
        error |= list_verify(list, VER_INIT, DUMP_IMG, "Before removal at index %d", remove_index);
        if (error != ERROR_NO) {
            return error;
        }
    )
    if (remove_index <= 0 || (size_t)(remove_index) >= list->capacity) {
        LOGGER_ERROR("list_remove: index %d out of range", remove_index);
        return ERROR_INCORRECT_INDEX;
    }
    if (list_node_is_free(&list->arr[remove_index])) {  
        LOGGER_ERROR("list_remove: node %d is already free", remove_index);
        return ERROR_INCORRECT_INDEX;
    }

    ssize_t prev_index = list->arr[remove_index].prev;
    ssize_t next_index = list->arr[remove_index].next;

    list->arr[prev_index].next = next_index;
    list->arr[next_index].prev = prev_index;

    list->head = list->arr[0].next;
    list->tail = list->arr[0].prev;

    list->arr[remove_index].next = list->free_head;
    list->arr[remove_index].prev = -1;
    list->arr[remove_index].val  = POISON;
    list->free_head = remove_index;

    list->size--;
    ON_DEBUG(
        error |= list_verify(list, VER_INIT, DUMP_IMG, "After removal at index %d", remove_index);
        if (error != ERROR_NO) {
            return error;
        }
    )
    return ERROR_NO;
}

error_code list_remove_auto(list_t* list, ssize_t remove_index) {
    HARD_ASSERT(list      != nullptr, "list is nullptr");
    HARD_ASSERT(list->arr != nullptr, "arr is nullptr");
    LOGGER_DEBUG("Removing logical index %d", remove_index);
    if (remove_index < 0 || (size_t)remove_index >= list->size - 1) {
        LOGGER_ERROR("list_remove_auto: remove_index %d out of range", remove_index);
        return ERROR_INCORRECT_INDEX;
    }
    ssize_t physical_index = list->head;
    for (ssize_t i = 0; i < remove_index; ++i) {
        physical_index = list->arr[physical_index].next;
    }
    return list_remove(list, physical_index);
}

error_code list_pop_back(list_t* list) {
    HARD_ASSERT(list      != nullptr, "list is nullptr");
    HARD_ASSERT(list->arr != nullptr, "arr is nullptr");
    LOGGER_DEBUG("Popping back");
    return list_remove(list, list->arr[0].prev);
}

error_code list_pop_front(list_t* list) {
    HARD_ASSERT(list      != nullptr, "list is nullptr");
    HARD_ASSERT(list->arr != nullptr, "arr is nullptr");
    LOGGER_DEBUG("Popping front");
    return list_remove(list, list->arr[0].next);
}

error_code list_swap(list_t* list, ssize_t first_idx, ssize_t second_idx) {
    HARD_ASSERT(list != nullptr, "list is nullptr");
    HARD_ASSERT(list->arr != nullptr, "arr is nullptr");
    LOGGER_DEBUG("Swapping indices %d and %d", first_idx, second_idx);

    error_code error = 0;
    if (first_idx <= 0 || second_idx <= 0 ||
        (size_t)(first_idx) >= list->capacity ||
        (size_t)(second_idx) >= list->capacity) {
        LOGGER_ERROR("One or both indices out of range");
        return ERROR_INCORRECT_INDEX;
    }
    if (first_idx == 0 || second_idx == 0) {
        LOGGER_ERROR("index 0 is sentinel and cannot be swapped");
        return ERROR_INCORRECT_INDEX;
    }

    node_t* first_elem  = &list->arr[first_idx];
    node_t* second_elem = &list->arr[second_idx];

    if (!list_node_is_free(first_elem) && !list_node_is_free(second_elem)) {
        list->arr[first_elem->next].prev = second_idx;
        list->arr[first_elem->prev].next = second_idx;
        node_t temp = *first_elem;
        *first_elem = *second_elem;
        *second_elem = temp;

    } else if (!list_node_is_free(second_elem)) {
        list->arr[second_elem->next].prev = first_idx;
        list->arr[second_elem->prev].next = first_idx;
        *first_elem = *second_elem;
        second_elem->val  = POISON;
        second_elem->prev = -1;
        second_elem->next = -1;

    } else if(!list_node_is_free(first_elem)) {
        list->arr[first_elem->next].prev = second_idx;
        list->arr[first_elem->prev].next = second_idx;
        *second_elem = *first_elem;
        first_elem->val  = POISON;
        first_elem->prev = -1;
        first_elem->next = -1;

    } else {
        first_elem->val  = POISON;
        first_elem->prev = -1;
        first_elem->next = -1;

        second_elem->val  = POISON;
        second_elem->prev = -1;
        second_elem->next = -1;
    }
    return error;
}

static error_code list_reorganize_free(list_t* list) {
    HARD_ASSERT(list      != nullptr, "list is nullptr");
    HARD_ASSERT(list->arr != nullptr, "arr is nullptr");

    LOGGER_DEBUG("Reorganizing free list");

    error_code error = 0;

    if(list->size == 0) {
        return ERROR_NO;
    }

    ssize_t first_free = list->size;  

    if((size_t)first_free == list->capacity) {
        list->free_head = -1;
        return ERROR_NO;
    }

    if ((size_t)first_free > list->capacity) {
        list->free_head = -1;
        return ERROR_BIG_SIZE;
    }

    list->free_head = first_free;
    for (size_t i = (size_t)first_free; i + 1 < list->capacity; ++i) {
        list->arr[i].prev = -1;
        list->arr[i].val  = POISON;
        list->arr[i].next = i + 1;
    }
    list->arr[list->capacity - 1].prev = -1;
    list->arr[list->capacity - 1].val  = POISON;
    list->arr[list->capacity - 1].next = -1;

    ON_DEBUG(
        error |= list_verify(list, VER_INIT, DUMP_IMG, "After reorganize_free");
    )
    return error;;
}

error_code list_linearize(list_t* list) {
    HARD_ASSERT(list      != nullptr, "list is nullptr");
    HARD_ASSERT(list->arr != nullptr, "arr is nullptr");

    LOGGER_DEBUG("Linearizing list");

    error_code error = 0;

    ON_DEBUG(
        error |= list_verify(list, VER_INIT, DUMP_IMG, "Before linearize");
        if (error != ERROR_NO) return error;
    )

    const ssize_t n = list->size - 1;
    if (n <= 0) {
        list->arr[0].next = 0;
        list->arr[0].prev = 0;
        list->head = 0;
        list->tail = 0;
        return list_reorganize_free(list);
    }

    ssize_t cur = list->arr[0].next; 
    for (ssize_t i = 1; i <= n; ++i) {
        if (cur != i) {
            error |= list_swap(list, i, cur);
            if (error != ERROR_NO) return error;
        }
        cur = list->arr[i].next;
    }

    list->arr[1].prev = 0;
    list->arr[1].next = 2;
    for (ssize_t i = 2; i < n; i++) {
        list->arr[i].prev = i - 1;
        list->arr[i].next = i + 1;
    }
    list->arr[n].prev = n - 1;
    list->arr[n].next = 0;

    list->arr[0].next = 1;
    list->arr[0].prev = n;
    list->head = 1;
    list->tail = n;

    return list_reorganize_free(list);
}

error_code list_shrink_to_fit(list_t* list, bool keep_growth) {
    HARD_ASSERT(list      != nullptr, "list is nullptr");
    HARD_ASSERT(list->arr != nullptr, "arr is nullptr");

    LOGGER_DEBUG("Shrinking list to fit (keep_growth=%d)", (int)keep_growth);

    error_code error = 0;
    ON_DEBUG(
        error |= list_verify(list, VER_INIT, DUMP_IMG, "Before shrink_to_fit(keep_growth=%d)", (int)keep_growth);
        if (error != ERROR_NO) return error;
    )
    error |= list_linearize(list);
    if (error != ERROR_NO) return error;
    
    size_t target = 0;
    if(keep_growth) {
        target = (size_t)((double)list->size * (double)GROWTH_FACTOR);
    } else {
        target = (size_t)list->size;
    }

    if (target < MIN_LIST_SIZE) target = MIN_LIST_SIZE;               
    if (target < list->size)    target = list->size;                    
    if (target == list->capacity) {
        return ERROR_NO;
    }

    size_t alloc_count = target ON_DEBUG(+ 1);
    node_t* new_block = (node_t*)realloc(list->arr, alloc_count * sizeof(node_t));
    if (!new_block) {
        LOGGER_ERROR("realloc failed in list_shrink_to_fit");
        error |= ERROR_MEM_ALLOC;
        return error;
    }
    list->arr = new_block;

    ON_DEBUG(
        list->arr[0].val       = CANARY_NUM;
        list->arr[target].val  = CANARY_NUM;
    )

    list->capacity = target;

    error |= list_reorganize_free(list);
    if (error != ERROR_NO) return error;

    ON_DEBUG(
        error |= list_verify(list, VER_INIT, DUMP_IMG, "After shrink_to_fit(target=%lu, keep_growth=%d)",
                          (unsigned long)target, (int)keep_growth);
        if (error != ERROR_NO) return error;
    )

    return ERROR_NO;
}
