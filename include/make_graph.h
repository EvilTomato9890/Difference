#ifndef MAKE_GRAPH_H_INCLUDED
#define MAKE_GRAPH_H_INCLUDED

#include "tree_info.h"
#include "error_handler.h"

error_code tree_plot_to_gnuplot(tree_t *treeData, size_t varIndex,
                                double xMin, double xMax,
                                const char *dataPath, const char *pngPath);
error_code dat_add_tree_graph(FILE* data_file, tree_t* tree,
                              size_t var_idx, double x_min, double x_max);

void print_dat_header(FILE *gnu_file, const char *data_file_name, const char *png_file_name);

error_code open_dat_file(FILE** data_file, char*  data_file_name,
                                size_t buff_size, const char* data_path, int file_cnt);

void close_dat_file(FILE* data_file);

error_code plot_dat_with_gnuplot(const char* data_file_name, const char* png_file_name);

#endif