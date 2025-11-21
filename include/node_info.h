#ifndef NODE_INFO_H_INCLUDED
#define NODE_INFO_H_INCLUDED

#define HANDLE_FUNC(name, ...) name,

enum func_type_t {
    #include "../source/copy_past_file"
};

#undef HANDLE_FUNC


typedef u_int64_t const_val_type;
typedef double    var_val_type;
enum node_type_t {
    FUNCTION,
    CONSTANT,
    VARIABLE
};

union value_t {
    const_val_type constant;
    size_t         var_idx;
    func_type_t    func;

};

#endif