#include "list_verification.h"
#include "logger.h"
#include "error_handler.h"
#include "asserts.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#ifndef VERIFY_DEBUG

#include "list_info.h"

error_code list_verify(list_t* list, ver_info_t ver_info, dump_mode_t mode, const char* fmt, ...) {
    (void)list;
    (void)ver_info;
    (void)mode;
    (void)fmt;
    return ERROR_NO;
}

void list_dump(list_t* list, ver_info_t ver_info, bool is_visual, const char* fmt, ...) {
    (void)list;
    (void)ver_info;
    (void)is_visual;
    (void)fmt;
} 

#else

static const double dpi       = 96.0;
static const double max_w_in  = 750.0 / dpi;
static const double max_h_in  = 500.0 / dpi;

//==============================================================================

#define LIGHT1_BLUE  "#6b8fd6"
#define LIGHT2_BLUE  "#e9f2ff"
#define LIGHT1_RED   "#ffecec"
#define LIGHT2_RED   "#d88d88"
#define LIGHT3_RED   "#d66d66"
#define RED          "#ff0000"
#define LIGHT_RED    "#ff4d4f"
#define CYAN         "#5aa99b"
#define LIGHT_CYAN   "#8be9fd"
#define LIGHT_CIAN   "#e9fbf5"
#define PURPLE       "#8e6bd6"
#define LIGHT_PURPLE "#f1eafd"
#define BLACK        "#000000"  
#define DARK_BLACK   "#0b0e14"
#define LIGHT1_BROWN "#fff3e0"
#define LIGHT2_BROWN "#e6e1cf"
#define LIGHT_GRAY   "#999999"

//==============================================================================

#define ELEM_0_BORDER       LIGHT1_BLUE
#define ELEM_0_BACK         LIGHT2_BLUE
#define FREE_NODE_BORDER    LIGHT3_RED
#define FREE_NODE_BACK      LIGHT1_RED
#define HEAD_BACK           LIGHT_CIAN
#define HEAD_BORDER         CYAN
#define TAIL_BACK           LIGHT_PURPLE
#define TAIL_BORDER         PURPLE
#define BASIC_BORDER        BLACK
#define BASIC_BACK          LIGHT1_BROWN
#define BAD_BOX_BORDER      LIGHT3_RED
#define BAD_BOX_BACK        LIGHT1_RED
#define EDGE_FREE           LIGHT2_RED
#define EDGE_TO_BAD_BOX     LIGHT3_RED
#define EDGE_BASIC          LIGHT_GRAY
#define EDGE_WRONG          RED
//------------------------------------------------------------------------------
#define HTML_BACKGROUND     DARK_BLACK
#define HTML_TEXT           LIGHT2_BROWN
#define HTML_TIME           LIGHT_CYAN
#define HTML_REASON         LIGHT_RED
#define HTML_BORDER         LIGHT1_BLUE

//==============================================================================

static void vfmt(char* buf, size_t capacity, const char* fmt, va_list ap) {
    if (!fmt) { if (capacity) buf[0] = '\0'; return; }
    vsnprintf(buf, capacity, fmt, ap);
}

static int  dump_make_graphviz_svg(const list_t* list, const char* base_name);
static void dump_write_html(const list_t* list, ver_info_t ver_info, int idx,
                            const char* comment, const char* svg_path, bool is_visual);

//------------------------------------------------------------------------------

static void emit_nodes(const list_t *list, FILE *file);
static void emit_invis_rank_edges(size_t capacity, FILE *file);

static void emit_bad_box_and_link(
    FILE *file, const char *box_tag, size_t owner_index, ssize_t bad_index,
    const char *edge_style, int reverse_dir);

static void emit_edges_free(const list_t *list, FILE *file);
static void emit_edges_next(const list_t *list, char *bidir_next, char *bidir_prev, FILE *file);
static void emit_edges_prev(const list_t *list, char *bidir_next, char *bidir_prev, FILE *file);

static int  run_dot_to_svg(const char *dot_path, const char *svg_path);

//==============================================================================

static inline int idx_ok(ssize_t idx, size_t capacity) {
    return idx >= 0 && (size_t)idx < capacity;
}

static error_code validate_main_chain(const list_t* list, size_t capacity, const char** error_description) {
    error_code error = 0;
    if (!idx_ok(list->head, capacity)) return 0;

    char* seen = (char*)calloc(capacity, 1);
    if (!seen) { LOGGER_ERROR("alloc failed"); return ERROR_MEM_ALLOC; }

    ssize_t curr = list->head;
    size_t steps = 0;
    while (idx_ok(curr, capacity) && curr > 0 && !seen[curr]) {
        seen[curr] = 1;
        ssize_t next = list->arr[curr].next;

        if (idx_ok(next, capacity)) {
            if (list->arr[next].prev != curr) {
                LOGGER_ERROR("mismatch prev for %ld <- %ld", next, curr);
                *error_description = "mismatch in main chain";
                error |= ERROR_INVALID_STRUCTURE;
            }
        } else if (next != -1) {
            LOGGER_ERROR("next node at %ld -> %ld not in chain", curr, next);
            *error_description = "next node not in main chain";
            error |= ERROR_INVALID_STRUCTURE;
            break;
        }

        curr = next;
        if (++steps > capacity + 1) {
            LOGGER_ERROR("loop suspected in next-chain");
            error |= ERROR_INVALID_STRUCTURE;
            break;
        }
    }
    free(seen);
    return error;
}

static error_code validate_free_chain(const list_t* list, size_t capacity, const char** error_description) {
    error_code error = 0;
    if (!idx_ok(list->free_head, capacity)) return 0;

    char* seen = (char*)calloc(capacity, 1);
    if (!seen) { LOGGER_ERROR("alloc failed (free)"); return ERROR_MEM_ALLOC; }

    ssize_t curr = list->free_head;
    size_t steps = 0;
    while (idx_ok(curr, capacity) && curr > 0 && !seen[curr]) {
        seen[curr] = 1;
        ssize_t next = list->arr[curr].next;

        if (next != -1 && !idx_ok(next, capacity)) {
            LOGGER_ERROR("free next not in chain %ld -> %ld", curr, next);
            *error_description = "free next node not in chain";
            error |= ERROR_INVALID_STRUCTURE;
            break;
        }
        curr = next;
        if (++steps > capacity + 1) {
            LOGGER_ERROR("loop suspected in free-chain");
            *error_description = "loop suspected in free-chain";
            error |= ERROR_INVALID_STRUCTURE;
            break;
        }
    }
    free(seen);
    return error;
}

error_code list_verify(list_t* list,
                       ver_info_t ver_info,
                       dump_mode_t mode,
                       const char* fmt, ...) {
    error_code error = 0;

    char comment[1024];
    va_list ap = {}; 
    va_start(ap, fmt);
    vfmt(comment, sizeof(comment), fmt, ap);
    va_end(ap);

    const char* error_description = "";
    if (!list) {
        LOGGER_ERROR("list is NULL");
        error |= ERROR_NULL_ARG;
        if (mode != DUMP_NO) {
            list_dump(NULL, ver_info, false, "list is NULL\n%s", comment);
        }
        return error;
    }

    const size_t capacity = list->capacity;

    if (capacity == 0) {
        LOGGER_ERROR("capacity == 0");
        error_description = "capacity == 0";
        error |= ERROR_INVALID_STRUCTURE;
    }
    if (!list->arr) {
        LOGGER_ERROR("arr is NULL");
        error_description = "arr is NULL";
        error |= ERROR_NULL_ARG;
    }
    if (list->size > capacity) {
        LOGGER_ERROR("size(%zu) > capacity(%zu)", list->size, capacity);
        error_description = "size > capacity";
        error |= ERROR_BIG_SIZE;
    }
    if (!idx_ok(list->head, capacity)) {
        LOGGER_WARNING("head out of bounds: %ld", list->head);
        error_description = "head out of bounds";
        error |= ERROR_INVALID_STRUCTURE;
    }
    if (list->free_head != -1 && !idx_ok(list->free_head, capacity)) {
        LOGGER_WARNING("free_head out of bounds: %ld", list->free_head);
        error_description = "free_head out of bounds";
        error |= ERROR_INVALID_STRUCTURE;
    }

    if (list->arr && capacity > 0) {
        if (list->arr[0].val != CANARY_NUM) {
            LOGGER_ERROR("Left canary corrupted: expected %g, got %g", CANARY_NUM, list->arr[0].val);
            error_description = "left canary corrupted";
            error |= ERROR_CANARY;
        }
        if (list->arr[capacity].val != CANARY_NUM) {
            LOGGER_ERROR("Right canary corrupted: expected %g, got %g", CANARY_NUM, list->arr[capacity].val);
            error_description = "right canary corrupted";
            error |= ERROR_CANARY;
        }
        
        error |= validate_main_chain(list, capacity, &error_description);
        error |= validate_free_chain(list, capacity, &error_description);
        error_description = "Corrupted chain";
    }

    if (error != 0 && mode != DUMP_NO) {
        const bool want_visual = (mode == DUMP_IMG);
        const bool can_visual  = want_visual && list->arr && (capacity > 0);
        list_dump(list, ver_info, can_visual, "List_verify: %s\n Comment: %s", error_description, comment);
    }
    return error;
}

//==============================================================================

void list_dump(list_t* list,
                     ver_info_t ver_info,
                     bool is_visual,
                     const char* fmt, ...) {
    static int dump_idx = 0;
    system("mkdir -p dumps");

    char comment[1024];
    va_list ap = {};
    va_start(ap, fmt);
    vfmt(comment, sizeof(comment), fmt, ap);
    va_end(ap);

    char base[256];
    snprintf(base, sizeof(base), "dumps/dump_%03d", dump_idx);

    char svg_path[300] = "";
    if (is_visual && list && list->arr && list->capacity > 0) {
        if (dump_make_graphviz_svg(list, base)) {
            snprintf(svg_path, sizeof(svg_path), "%s.svg", base);
        } else {
            LOGGER_WARNING("Graphviz failed; text-only dump written");
        }
    }

    dump_write_html(list, ver_info, dump_idx, comment, svg_path, is_visual);

    LOGGER_INFO("Dump #%ld written%s%s",
                dump_idx,
                svg_path[0] ? " with SVG: " : "",
                svg_path[0] ? svg_path : "");

    dump_idx++;
}


int dump_make_graphviz_svg(const list_t* list, const char* base_name) {

    LOGGER_DEBUG("dump_make_graphviz_svg started");

    char dot_path[256], svg_path[256];
    snprintf(dot_path, sizeof(dot_path), "%s.dot", base_name);
    snprintf(svg_path, sizeof(svg_path), "%s.svg", base_name);

    FILE* file = fopen(dot_path, "w");
    if (!file) {
        LOGGER_ERROR("dump_make_graphviz_svg: can't open %s", dot_path);
        return 0;
    }

    fprintf(file, "digraph G{\n");
    fprintf(file, "  rankdir=LR;\n");
    fprintf(file, "  splines=ortho;\n");
    fprintf(file, "  nodesep=0.9;\n");
    fprintf(file, "  ranksep=0.75;\n");
    fprintf(file, "  node [fontname=\"Fira Mono\",shape=record,style=\"filled,rounded\",fontsize=11];\n");

    fprintf(file,
        "  node_0[shape=record,"
        "label=\"ind: 0 | val: %g | { prev: %ld | next: %ld }\"," 
        "color=\"" ELEM_0_BORDER "\",style=\"filled,bold,rounded\",fillcolor=\"" ELEM_0_BACK "\"];\n",
        list->arr[0].val, list->arr[0].prev, list->arr[0].next);

    emit_nodes(list, file);
    emit_invis_rank_edges(list->capacity, file);

    char *bidir_next = (char*)calloc(list->capacity, 1);
    char *bidir_prev = (char*)calloc(list->capacity, 1);
    if (!bidir_next || !bidir_prev) {
        free(bidir_next);
        free(bidir_prev);
        fclose(file);
        LOGGER_ERROR("dump_make_graphviz_svg: no memory for flags");
        return 0;
    }

    emit_edges_free(list, file);
    emit_edges_next(list, bidir_next, bidir_prev, file);
    emit_edges_prev(list, bidir_next, bidir_prev, file);

    free(bidir_next);
    free(bidir_prev);

    fprintf(file,
        "  node_free [label=free_head,color=\"" FREE_NODE_BORDER  "\",shape=rectangle,style=\"filled,rounded\",fillcolor=\"" FREE_NODE_BACK "\"];\n");
    fprintf(file, "  node_free -> node_%ld [color=\"" EDGE_FREE "\", style=dashed, constraint=false];\n", list->free_head);

    fprintf(file, "}\n");
    fclose(file);

    return run_dot_to_svg(dot_path, svg_path);
}

//------------------------------------------------------------------------------

static void emit_nodes(const list_t *list, FILE *file) {
    const size_t capacity = list->capacity;
    for (size_t i = 1; i < capacity; ++i) {
        const ssize_t next_index  = list->arr[i].next;
        const ssize_t prev_index  = list->arr[i].prev;
        const double val      = list->arr[i].val;

        const ssize_t is_free = (prev_index == -1 || val == POISON);
        const ssize_t is_head = (i == (size_t)list->head);
        const ssize_t is_tail = (i == (size_t)list->tail);

        if (is_free) {
            fprintf(file,
                "  node_%zu[shape=record,"
                "label=\" ind: %zu | val: %g | { prev: %ld | next: %ld } \","
                "style=\"filled,rounded\",color=\"" FREE_NODE_BORDER "\",fillcolor=\"" FREE_NODE_BACK "\"];\n",
                i, i, val, prev_index, next_index);
        } else if (is_head) {
            fprintf(file,
                "  node_%zu[shape=record,"
                "label=\" ind: %zu (HEAD) | val: %g | { prev: %ld | next: %ld } \","
                "color=\"" HEAD_BORDER "\",fillcolor=\"" HEAD_BACK "\"];\n",
                i, i, val, prev_index, next_index);
        } else if (is_tail) {
            fprintf(file,
                "  node_%zu[shape=record,"
                "label=\" ind: %zu (TAIL) | val: %g | { prev: %ld | next: %ld } \","
                "color=\"" TAIL_BORDER "\",fillcolor=\"" TAIL_BACK "\"];\n",
                i, i, val, prev_index, next_index);
        } else {
            fprintf(file,
                "  node_%zu[shape=record,"
                "label=\" ind: %zu | val: %g | { prev: %ld | next: %ld } \","
                "color=\"" BASIC_BORDER "\",fillcolor=\"" BASIC_BACK "\"];\n",
                i, i, val, prev_index, next_index);
        }
    }
}

static void emit_invis_rank_edges(size_t capacity, FILE *file) {
    for (size_t i = 0; i + 1 < capacity; ++i) {
        fprintf(file, "  node_%zu -> node_%zu [weight=1000,style=invis];\n", i, i + 1);
    }
}

static void emit_bad_box_and_link(
    FILE *file, const char *box_tag, size_t owner_index, ssize_t bad_index,
    const char *edge_style, int reverse_dir)
{
    fprintf(file,
        "  node_bad_%s_%zu [label=\"idx %ld\\n(N/A)\",shape=rectangle,"
        "style=\"filled,rounded\",color=\"" BAD_BOX_BORDER "\",fillcolor=\"" BAD_BOX_BACK "\"];\n",
        box_tag, owner_index, bad_index);

    if (reverse_dir) {
        fprintf(file, "  node_bad_%s_%zu -> node_%zu [%s];\n",
                box_tag, owner_index, owner_index, edge_style);
    } else {
        fprintf(file, "  node_%zu -> node_bad_%s_%zu [%s];\n",
                owner_index, box_tag, owner_index, edge_style);
    }

    fprintf(file,
        "  node_%zu -> node_bad_%s_%zu [style=invis, constraint=true, weight=60, minlen=1];\n",
        owner_index, box_tag, owner_index);
    fprintf(file, "  {rank=same; node_%zu; node_bad_%s_%zu;}\n",
            owner_index, box_tag, owner_index);
}

//------------------------------------------------------------------------------

static void emit_edges_free(const list_t *list, FILE *file) {
    const size_t capacity = list->capacity;
    for (size_t i = 0; i < capacity; ++i) {
        const ssize_t prev_index = list->arr[i].prev;
        if (prev_index != -1) continue; 

        const ssize_t next_index = list->arr[i].next;
        if (next_index >= 0 && (size_t)next_index < capacity) {
            fprintf(file,
                "  node_%zu -> node_%ld [color=\"" EDGE_FREE "\", style=dashed, constraint=false];\n",
                i, next_index);
        } else if (next_index != -1) {
            emit_bad_box_and_link(file, "f", i, next_index,
                                  "color=\"" EDGE_TO_BAD_BOX "\", style=dashed, constraint=false", 0);
        }
    }
}

static void emit_edges_next(const list_t *list, char *bidir_next, char *bidir_prev, FILE *file) {
    const size_t capacity = list->capacity;
    for (size_t i = 0; i < capacity; ++i) {
        const ssize_t prev_index = list->arr[i].prev;
        if (prev_index == -1) continue; 

        const ssize_t next_index = list->arr[i].next;
        if (next_index >= 0 && (size_t)next_index < capacity) {
            if (list->arr[next_index].prev == (ssize_t)i) {
                if (!bidir_next[i]) {
                    fprintf(file,
                        "  node_%zu -> node_%ld [dir=both, color=\"" EDGE_BASIC "\", constraint=false];\n",
                        i, next_index);
                    bidir_prev[next_index] = 1;
                }
            } else {
                fprintf(file,
                    "  node_%zu -> node_%ld [color=\"" EDGE_WRONG "\", penwidth=2.2, constraint=false];\n",
                    i, next_index);
            }
        } else if (next_index != -1) {
            emit_bad_box_and_link(file, "n", i, next_index,
                                  "style=\"bold\", color=\"" EDGE_TO_BAD_BOX "\", constraint=false", 0);
        }
    }
}

static void emit_edges_prev(const list_t *list, char *bidir_next, char *bidir_prev, FILE *file) {
    const size_t capacity = list->capacity;
    for (size_t i = 0; i < capacity; ++i) {
        const ssize_t prev_index = list->arr[i].prev;
        if (prev_index == -1) continue; 

        if (prev_index >= 0 && (size_t)prev_index < capacity) {
            if (list->arr[prev_index].next == (ssize_t)i) {
                if (!bidir_prev[i]) {
                    fprintf(file,
                        "  node_%ld -> node_%zu [dir=both, color=\"" EDGE_BASIC "\", constraint=false];\n",
                        prev_index, i);
                    bidir_next[prev_index] = 1;
                }
            } else {
                fprintf(file,
                    "  node_%ld -> node_%zu [color=\"" EDGE_WRONG "\", penwidth=2.2, constraint=false];\n",
                    prev_index, i);
            }
        } else if (prev_index != -1) {
            emit_bad_box_and_link(file, "p", i, prev_index,
                                  "style=\"bold\", color=\"" EDGE_TO_BAD_BOX "\", constraint=false", 1);
        }
    }
}

static int run_dot_to_svg(const char *dot_path, const char *svg_path) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "dot -Tsvg \"%s\" -o \"%s\"",
             dot_path, svg_path);
    return system(cmd) == 0;
}

//==============================================================================

static void dump_write_html(const list_t* list, ver_info_t ver_info_called, int idx,
                            const char* comment, const char* svg_path, bool is_visual) {

    LOGGER_DEBUG("dump_write_html started");
    FILE* html = list->dump_file;
    if (!html) {
        LOGGER_ERROR("dump_write_html: can't open dump.html");
        return;
    }

    time_t t = time(NULL);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&t));

    ver_info_t ver_info_created = list->ver_info;

    fprintf(html, "<pre>\n"); 

        fprintf(html,
        "<pre style=\"font-family:'Fira Mono', monospace;"
        "font-size:16px; line-height:1.28;"
        "background:" HTML_BACKGROUND "; color:" HTML_TEXT ";"
        "padding:12px; border-radius:10px;\">\n");

    fprintf(html,
        "<span style=\"color:" HTML_BORDER ";font-weight:700;\">====================[ DUMP #%d ]====================</span>\n",
        idx);
    fprintf(html, "Timestamp: <span style=\"color:" HTML_TIME ";\">%s</span>\n", ts);

    if (comment && comment[0] != '\0')
        fprintf(html, "<span style=\"color:" HTML_REASON ";font-weight:700;\">%s</span>\n", comment);
    else
        fprintf(html, "<span style=\"color:#999;\">(no comment)</span>\n");
    fprintf(html,
        "<span style=\"color:" HTML_BORDER ";font-weight:700;\">===================================================</span>\n");

    fprintf(html, "list ptr : %p\n",  list);
    fprintf(html, "arr  ptr : %p\n",  list ? list->arr      :  NULL);
    fprintf(html, "capacity : %zu\n", list ? list->capacity :  0);
    fprintf(html, "size     : %zu\n", list ? list->size     :  0);
    fprintf(html, "head     : %ld\n",  list ? list->head     : -1);
    fprintf(html, "tail     : %ld\n",  list ? list->tail     : -1);
    fprintf(html, "free_head: %ld\n",  list ? list->free_head: -1);

    fprintf(html, "\n-- Created at (list ver_info) --\n");
    fprintf(html, "file: %s\n",   ver_info_created.file);
    fprintf(html, "func: %s\n",   ver_info_created.func);
    fprintf(html, "line: %d\n",   ver_info_created.line);

    fprintf(html, "\n-- Called at (passed ver_info) --\n");
    fprintf(html, "file: %s\n",   ver_info_called.file);
    fprintf(html, "func: %s\n",   ver_info_called.func);
    fprintf(html, "line: %d\n",   ver_info_called.line);

    if (list && list->arr && list->capacity > 0) {
        fprintf(html, "\n-- Canaries --\n");
        fprintf(html, "Expected canary value: %g\n", CANARY_NUM);
        fprintf(html, "left : %g\n", list->arr[0].val); 
        fprintf(html, "right: %g\n", list->arr[list->capacity].val);
    }

    if (list && list->arr && list->capacity > 0) {
        fprintf(html, "\n");
        fprintf(html, "IDX   NEXT   PREV        VALUE     MARKS\n");
        fprintf(html, "----  ------ ------  ------------  -----\n");

        for (size_t i = 0; i < list->capacity; ++i) {
            ssize_t next =  list->arr[i].next;
            ssize_t  prv =  list->arr[i].prev;
            double   val =  list->arr[i].val;
            if(val == POISON) val = POISON;

            char marks[32]; marks[0] = '\0';
            ssize_t first = 1;

            if (i == 0)                       {strcat(marks,         "ZERO"          ); first = 0;}
            if ((ssize_t)i == list->head)     {strcat(marks, first ? "HEAD" : ",HEAD"); first = 0;}
            if ((ssize_t)i == list->tail)     {strcat(marks, first ? "TAIL" : ",TAIL"); first = 0;}
            if ((ssize_t)i == list->free_head){strcat(marks, first ? "FREE" : ",FREE"); first = 0;}

            fprintf(html, "%-4zu  %-6ld %-6ld  %-12.6g  %-5s",
                    i, next, prv, val, marks);
            if(val == POISON) {
                fprintf(html, "(POISON)");
            }
            fprintf(html, "\n");
        }
    } else {
        fprintf(html, "\n(arr is NULL or capacity == 0)\n");
    }

    fprintf(html, "\nSVG: %s\n", svg_path);
    fprintf(html, "</pre>\n");

    if (svg_path && is_visual) {
        fprintf(html, "<img src=\"%s\" max-width=2250/>\n", svg_path);
        /*FILE* img = fopen(svg_path, "rb");
        if (img) {
            char buf[4096]; size_t n;
            while ((n = fread(buf,1,sizeof(buf),img)) > 0) fwrite(buf,1,n,html);
            fclose(img);
        } else {
            fprintf(html, "<pre>\nOPEN_FAILED\n</pre>\n");
        }
           */ 
    }
    fprintf(html, "<hr>\n");
}

// close VERIFY_DEBUG conditional
#endif /* VERIFY_DEBUG */
