#ifndef MAKE_GRAPH_H_INCLUDED
#define MAKE_GRAPH_H_INCLUDED

#include "tree_info.h"
#include "error_handler.h"

error_code tree_plot_to_gnuplot(tree_t *treeData, size_t varIndex,
                                double xMin, double xMax,
                                size_t dotsCount, const char *dataPath, const char *pngPath);

#endif