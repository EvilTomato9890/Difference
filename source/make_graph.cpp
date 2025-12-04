#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>

#include "tree_info.h"
#include "tree_operations.h"
#include "differentiator.h"
#include "error_handler.h"
#include "make_graph.h"
#include "logger.h"

//================================================================================

static const int graph_max_file_name  = 256;
static const int graph_dots_count     = 100;

//================================================================================

struct graph_session_state_t {
    FILE* gnuplot_file;
    int   curve_count;
    int   data_index;
    bool  is_active;
    char  image_path[graph_max_file_name];
};

static graph_session_state_t graph_session = { nullptr, 0, 0, false, {0} };

//================================================================================


static void graph_build_path(char* path_buffer, size_t buffer_size,
                             const char* user_name, const char* pattern_name,
                             int index_value) {
    HARD_ASSERT(path_buffer != nullptr, "graph_build_path: path_buffer is nullptr");
    HARD_ASSERT(buffer_size > 0,        "graph_build_path: buffer_size is zero");

    if (user_name == nullptr || user_name[0] == '\0') {
        snprintf(path_buffer, buffer_size, pattern_name, index_value);
        return;
    }

    size_t user_name_length = strlen(user_name);
    size_t prefix_length    = strlen("graphs/");
    if (user_name_length + prefix_length + 1 > buffer_size) {
        LOGGER_WARNING("graph_build_path: user_name is too long, will be truncated");
    }

    snprintf(path_buffer, buffer_size, "graphs/%s", user_name);
}

//------------------------------------------------------------------------------

static void graph_write_common_header(FILE* gnuplot_file,
                                      const char* image_path) {
    fprintf(gnuplot_file, "set terminal pngcairo size 1280,720\n");
    fprintf(gnuplot_file, "set output '%s'\n", image_path);
    fprintf(gnuplot_file, "set grid\n");
    fprintf(gnuplot_file, "set xlabel 'x'\n");
    fprintf(gnuplot_file, "set ylabel 'f(x)'\n");
    fprintf(gnuplot_file, "plot \\\n");
}


static void graph_write_single_header(FILE* gnuplot_file,
                                      const char* data_path,
                                      const char* image_path) {
    graph_write_common_header(gnuplot_file, image_path);
    fprintf(gnuplot_file,
            "'%s' using 1:2 with lines title 'f(x)'\n",
            data_path);
    fprintf(gnuplot_file, "\n\nunset output\n");
}

//================================================================================

static error_code graph_open_data_file(FILE** data_file, char* path_buffer,
                                       size_t buffer_size,
                                       const char* user_name, int index_value) {
    HARD_ASSERT(data_file   != nullptr, "graph_open_data_file: data_file is nullptr");
    HARD_ASSERT(path_buffer != nullptr, "graph_open_data_file: path_buffer is nullptr");
    HARD_ASSERT(buffer_size > 0,        "graph_open_data_file: buffer_size is zero");

    system("mkdir -p graphs");;

    graph_build_path(path_buffer, buffer_size,
                     user_name, "graphs/plot_data_%d.dat",
                     index_value);

    *data_file = fopen(path_buffer, "w");
    if (*data_file == nullptr) {
        LOGGER_ERROR("graph_open_data_file: fopen('%s') failed", path_buffer);
        return ERROR_OPEN_FILE;
    }

    return ERROR_NO;
}

//================================================================================

error_code dat_add_tree_graph(FILE* data_file, tree_t* tree_data,
                              size_t var_idx,
                              double x_min, double x_max) {
    HARD_ASSERT(data_file            != nullptr, "dat_add_tree_graph: data_file is nullptr");
    HARD_ASSERT(tree_data            != nullptr, "dat_add_tree_graph: tree_data is nullptr");
    HARD_ASSERT(tree_data->var_stack != nullptr,
                "dat_add_tree_graph: tree_data->var_stack is nullptr");

    error_code calc_error = ERROR_NO;
    double     step_size  = (x_max - x_min) /
                            (double)(graph_dots_count - 1);

    for (size_t index_point = 0; index_point < (size_t)graph_dots_count; ++index_point) {
        double value_x = x_min + step_size * (double)index_point;

        put_var_val(tree_data, var_idx, (var_val_type)value_x);

        var_val_type value_y = calculate_nodes_recursive(tree_data, tree_data->root, &calc_error);
        if (calc_error != ERROR_NO) {
            LOGGER_ERROR("dat_add_tree_graph: calculate_nodes_recursive failed");
            return calc_error;
        }

        fprintf(data_file, "%.15g %.15g\n", value_x, (double)value_y);
    }

    return ERROR_NO;
}

//================================================================================

static error_code graph_plot_single_internal(tree_t* tree_data,
                                             size_t var_idx,
                                             double x_min, double x_max,
                                             const char* data_name,
                                             const char* image_name) {
    HARD_ASSERT(tree_data            != nullptr, "graph_plot_single_internal: tree_data is nullptr");
    HARD_ASSERT(tree_data->var_stack != nullptr, "graph_plot_single_internal: tree_data->var_stack is nullptr");
    HARD_ASSERT(image_name           != nullptr, "graph_plot_single_internal: image_name is nullptr");

    static int file_counter_single = 0;

    char data_path_buffer [graph_max_file_name] = {};
    char image_path_buffer[graph_max_file_name] = {};

    FILE* data_file = nullptr;

    error_code open_error = graph_open_data_file(&data_file, data_path_buffer, graph_max_file_name,
                                                 data_name, file_counter_single);
    if (open_error != ERROR_NO) return open_error;

    graph_build_path(image_path_buffer, graph_max_file_name,
                     image_name, "graphs/plot_png_%d.png",
                     file_counter_single);

    file_counter_single++;

    error_code fill_error = dat_add_tree_graph(data_file, tree_data,
                                               var_idx, x_min, x_max);
    if (fclose(data_file) != 0) {
        LOGGER_WARNING("graph_close_data_file: fclose failed");
    }
    if (fill_error != ERROR_NO) return fill_error;
    

    FILE* gnuplot_file = popen("gnuplot", "w");
    if (gnuplot_file == nullptr) {
        LOGGER_ERROR("graph_plot_single_internal: popen gnuplot failed");
        return ERROR_OPEN_FILE;
    }

    graph_write_single_header(gnuplot_file,
                              data_path_buffer,
                              image_path_buffer);

    fflush(gnuplot_file);
    pclose(gnuplot_file);

    return ERROR_NO;
}

//------------------------------------------------------------------------------

error_code tree_plot_to_gnuplot(tree_t* tree_data, size_t var_idx,
                                double x_min, double x_max,
                                const char* data_name, const char* image_name) {
    LOGGER_DEBUG("tree_plot_to_gnuplot: started");
    return graph_plot_single_internal(tree_data, var_idx,
                                      x_min, x_max,
                                      data_name, image_name);
}

//================================================================================

static void graph_session_reset(graph_session_state_t* session_state) {
    if (session_state == nullptr) {
        return;
    }

    session_state->gnuplot_file = nullptr;
    session_state->curve_count  = 0;
    session_state->data_index   = 0;
    session_state->is_active    = false;
    memset(session_state->image_path, 0, sizeof(session_state->image_path));
}


static error_code graph_session_open(graph_session_state_t* session_state,
                                     const char* image_name) {
    HARD_ASSERT(session_state != nullptr, "graph_session_open: session_state is nullptr");

    if (session_state->is_active &&
        session_state->gnuplot_file != nullptr) {
        fprintf(session_state->gnuplot_file, "\nunset output\n");
        fflush(session_state->gnuplot_file);
        pclose(session_state->gnuplot_file);
        graph_session_reset(session_state);
    }

    system("mkdir -p graphs");

    const char* effective_image_name = nullptr;
    if(image_name != nullptr && image_name[0] != '\0') {
        effective_image_name = image_name;
    } else {
        effective_image_name = "multi_plot.png";
    }

    graph_build_path(session_state->image_path, graph_max_file_name,
                     effective_image_name,
                     "graphs/multi_plot.png", 0);

    session_state->gnuplot_file = fopen("graphs/gnuplot.plt", "w");
    if (session_state->gnuplot_file == nullptr) {
        LOGGER_ERROR("graph_session_open: popen gnuplot failed");
        graph_session_reset(session_state);
        return ERROR_OPEN_FILE;
    }

    graph_write_common_header(session_state->gnuplot_file,
                              session_state->image_path);

    session_state->curve_count = 0;
    session_state->data_index  = 0;
    session_state->is_active   = true;

    return ERROR_NO;
}


static error_code graph_session_plot_first(graph_session_state_t* session_state,
                                           const char* data_path,
                                           const char* legend_title) {
    HARD_ASSERT(session_state != nullptr, "graph_session_plot_first: session_state is nullptr");
    HARD_ASSERT(data_path     != nullptr, "graph_session_plot_first: data_path is nullptr");

    const char* effective_title = nullptr;
    if(legend_title != nullptr && legend_title[0] != '\0') {
        effective_title = legend_title;
    } else {
        effective_title = "f(x)";
    }

    fprintf(session_state->gnuplot_file,
            "'%s' using 1:2 with lines title '%s',\\\n",
            data_path, effective_title);

    fflush(session_state->gnuplot_file);
    session_state->curve_count = 1;

    return ERROR_NO;
}


static error_code graph_session_plot_append(graph_session_state_t* session_state,
                                            const char* data_path,
                                            const char* legend_title) {
    HARD_ASSERT(session_state != nullptr, "graph_session_plot_append: session_state is nullptr");
    HARD_ASSERT(data_path     != nullptr, "graph_session_plot_append: data_path is nullptr");

    const char* effective_title = nullptr;
    if(legend_title != nullptr && legend_title[0] != '\0') {
        effective_title = legend_title;
    } else {
        effective_title = "f(x)";
    }

    fprintf(session_state->gnuplot_file,
            "'%s' using 1:2 with lines title '%s',\\\n",
            data_path, effective_title);

    fflush(session_state->gnuplot_file);
    session_state->curve_count += 1;

    return ERROR_NO;
}


static error_code graph_session_add_internal(graph_session_state_t* session_state, tree_t* tree_data,
                                             size_t var_idx, double x_min, double x_max,
                                             const char* data_name, const char* legend_title) {
    HARD_ASSERT(session_state        != nullptr, "graph_session_add_internal: session_state is nullptr");
    HARD_ASSERT(tree_data            != nullptr, "graph_session_add_internal: tree_data is nullptr");
    HARD_ASSERT(tree_data->var_stack != nullptr, "graph_session_add_internal: tree_data->var_stack is nullptr");

    if (!session_state->is_active || session_state->gnuplot_file == nullptr) {
        LOGGER_ERROR("graph_session_add_internal: session is not active. "
                     "Call dat_add_tree_first first.");
        return ERROR_OPEN_FILE;
    }

    char data_path_buffer[graph_max_file_name] = {};
    FILE* data_file = nullptr;

    error_code open_error = graph_open_data_file(&data_file, data_path_buffer,
                                                 graph_max_file_name, data_name,
                                                 session_state->data_index);
    if (open_error != ERROR_NO) return open_error;

    session_state->data_index += 1;

    error_code fill_error = dat_add_tree_graph(data_file, tree_data,
                                               var_idx, x_min, x_max);
    if (fclose(data_file) != 0) {
        LOGGER_WARNING("graph_close_data_file: fclose failed");
    }

    if (fill_error != ERROR_NO) return fill_error;

    if (session_state->curve_count == 0) {
        return graph_session_plot_first(session_state, data_path_buffer, legend_title);
    } else {
        return graph_session_plot_append(session_state, data_path_buffer, legend_title);
    }
}

//================================================================================

error_code dat_add_tree_first(tree_t* tree_data, size_t var_idx, double x_min, double x_max,
                              const char* data_name, const char* image_name, const char* legend_title) {

    error_code session_error = graph_session_open(&graph_session,
                                                  image_name);
    if (session_error != ERROR_NO) {
        return session_error;
    }

    return graph_session_add_internal(&graph_session, tree_data,
                                      var_idx, x_min, x_max,
                                      data_name, legend_title);
}

error_code dat_add_tree(tree_t* tree_data, size_t var_idx, double x_min, double x_max,
                        const char* data_name, const char* legend_title) {
    return graph_session_add_internal(&graph_session, tree_data,
                                      var_idx, x_min, x_max,
                                      data_name, legend_title);
}

void dat_finish_all_graphs() {
    if (!graph_session.is_active ||
        graph_session.gnuplot_file == nullptr) {
        return;
    }

    fprintf(graph_session.gnuplot_file, "\nunset output\n");
    fflush(graph_session.gnuplot_file);
    fclose(graph_session.gnuplot_file);
    system("gnuplot graphs/gnuplot.plt");
    graph_session_reset(&graph_session);
}
