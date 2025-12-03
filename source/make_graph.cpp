#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>

#include "tree_info.h"
#include "tree_operations.h"
#include "differentiator.h"
#include "error_handler.h"
#include "make_graph.h"
#include "logger.h"

const int MAX_FILE_NAME = 256;

static void print_dat_header(FILE *gnu_file, const char *data_file_name, const char *png_file_name) {
    fprintf(gnu_file, "set terminal pngcairo size 1280,720\n");
    fprintf(gnu_file, "set output '%s'\n", png_file_name);
    fprintf(gnu_file, "set grid\n");
    fprintf(gnu_file, "set xlabel 'x'\n");
    fprintf(gnu_file, "set ylabel 'f(x)'\n");
    fprintf(gnu_file, "plot '%s' using 1:2 with lines title 'f(x)'\n", data_file_name);
    fprintf(gnu_file, "unset output\n");
}

static void fill_file_name_buff(char* buff, size_t buff_size, const char* file_name, const char* basic_file_name, int file_cnt) {
    HARD_ASSERT(buff != nullptr, "Buff is nullptr");
    if(file_name == nullptr) {
        snprintf(buff, buff_size, basic_file_name, file_cnt);
    } else {
        size_t path_len = strlen(file_name);
        if(path_len + 1 > buff_size) {
            LOGGER_WARNING("tree_plot_to_gnuplot: len data_path > %d, saved %d symblos", MAX_FILE_NAME, MAX_FILE_NAME - 1);
            snprintf(buff, buff_size, "graphs/%s", file_name); //REVIEW - Или 2 memcpy
        }
        snprintf(buff, buff_size, "graphs/%s", file_name);
    }
}
//REVIEW - aa
error_code tree_plot_to_gnuplot(tree_t* tree, size_t var_idx,
                                double x_min, double x_max,
                                size_t dots_cnt, const char* data_path, const char* png_path) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(tree->var_stack != nullptr, "tree->var_stack is nullptr");
    HARD_ASSERT(var_idx < tree->var_stack->size, "var_idx is out of range");
    HARD_ASSERT(data_path != nullptr, "data_path is nullptr");
    HARD_ASSERT(png_path != nullptr, "png_path is nullptr");

    LOGGER_DEBUG("tree_plot_to_gnuplot: started");

    static int fileCounter = 0;
    system("mkdir -p graphs");


    char data_file_name[MAX_FILE_NAME] = {};
    char  png_file_name[MAX_FILE_NAME]  = {};
    fill_file_name_buff(data_file_name, MAX_FILE_NAME, data_path, "graphs/plot_data_%d.dat", fileCounter);
    fill_file_name_buff(png_file_name,  MAX_FILE_NAME, png_path,  "graphs/plot_png_%d.png",  fileCounter);
    fileCounter++;

    FILE *data_file = fopen(data_file_name, "w");
    if (!data_file) {
        LOGGER_ERROR("fopen data_file_name");
        return ERROR_OPEN_FILE;
    }

    error_code error = ERROR_NO;
    const double step_size = (x_max - x_min) / (double)(dots_cnt - 1);

    for (size_t i = 0; i < dots_cnt; ++i) {
        double x_value = x_min + step_size * (double)i;

        (void)put_var_val(tree, var_idx, (var_val_type)x_value);

        var_val_type y_value = calculate_nodes_recursive(tree, tree->root, &error);
        if (error != ERROR_NO) {
            fclose(data_file);
            return error;
        }

        fprintf(data_file, "%.15g %.15g\n", x_value, (double)y_value);
    }

    fclose(data_file);

    FILE *gnu_file = popen("gnuplot", "w");
    if (!gnu_file) {
        LOGGER_ERROR("popen gnuplot");
        return ERROR_OPEN_FILE;
    }

    print_dat_header(gnu_file, data_file_name, png_file_name);

    fflush(gnu_file);
    pclose(gnu_file);
    return ERROR_NO;
}
