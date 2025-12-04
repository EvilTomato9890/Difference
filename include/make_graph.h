#ifndef MAKE_GRAPH_H
#define MAKE_GRAPH_H

#include "error_handler.h"
#include "tree_info.h"

error_code dat_add_tree_graph(FILE* data_file, tree_t* tree_data,
                              size_t var_idx, double x_min, double x_max);


error_code tree_plot_to_gnuplot(tree_t* tree_data, size_t var_idx, double x_min, double x_max,
                                const char* data_name, const char* image_name);

error_code dat_add_tree_first(tree_t* tree_data, size_t var_idx, double x_min, double x_max,
                              const char* data_name, const char* image_name, const char* legend_title);

error_code dat_add_tree(tree_t* tree_data, size_t var_idx,
                        double x_min, double x_max,
                        const char* data_name, const char* legend_title);

void dat_finish_all_graphs();


#endif 
