#include "internal.h"

static void yed_init_frames(void) {
    ys->frames       = tree_make_c(yed_frame_id_t, yed_frame_ptr_t, strcmp);
    ys->active_frame = NULL;
}

static yed_frame * yed_add_new_frame(yed_frame_id_t id, int top, int left, int height, int width) {
    yed_frame *frame;

    frame = yed_new_frame(id, top, left, height, width);

    tree_insert(ys->frames, frame->id, frame);

    return frame;
}

static yed_frame * yed_get_frame(yed_frame_id_t id) {
    tree_it(yed_frame_id_t, yed_frame_ptr_t) it;

    it = tree_lookup(ys->frames, id);

    if (tree_it_good(it)) {
        return tree_it_val(it);
    }

    return NULL;
}

static yed_frame * yed_get_or_add_frame(yed_frame_id_t id) {
    yed_frame *frame;

    frame = yed_get_frame(id);

    if (!frame) {
        frame = yed_add_new_frame(id, 1, 1, ys->term_rows - 1, ys->term_cols);
    }

    return frame;
}

static yed_frame * yed_new_frame(yed_frame_id_t id, int top, int left, int height, int width) {
    yed_frame *frame;

    frame = malloc(sizeof(*frame));

    frame->id            = strdup(id);
    frame->buffer        = NULL;
    frame->top           = top;
    frame->left          = left;
    frame->height        = height;
    frame->width         = width;
    frame->cursor_line   = 1;
    frame->buffer_offset = 0;
    frame->cur_x         = left;
    frame->cur_y         = top;
    frame->desired_x     = 1;
    frame->dirty         = 1;

    return frame;
}

static void yed_activate_frame(yed_frame *frame) {
    if (ys->active_frame && ys->active_frame != frame) {
        ys->active_frame->dirty = 1;
    }
    ys->active_frame        = frame;
    ys->active_frame->dirty = 1;
}

static void yed_clear_frame(yed_frame *frame) {
    int r, c;
    int x, y;

    x = ys->cur_x;
    y = ys->cur_y;

    for (r = 0; r < frame->height; r += 1) {
        yed_set_cursor(frame->left, frame->top + r);

        for (c = 0; c < frame->width; c += 1) {
            append_n_to_output_buff(" ", 1);
        }
    }

    yed_set_cursor(x, y);
}

static void yed_frame_draw_line(yed_frame *frame, yed_line *line, int y_offset) {
    int n;

    yed_set_cursor(frame->left, frame->top + y_offset);
    n = MIN(array_len(line->chars), frame->width);
    append_n_to_output_buff(array_data(line->chars), n);
    for (; n < frame->width; n += 1) {
        append_n_to_output_buff(" ", 1);
    }
}

static void yed_frame_draw_buff(yed_frame *frame, yed_buffer *buff, int offset) {
    yed_line *line;
    int lines_drawn;
    int lines_seen;

    lines_drawn = 0;
    lines_seen  = 0;

    array_traverse(buff->lines, line) {
        if (lines_seen++ < offset)    { continue; }

        yed_frame_draw_line(frame, line, lines_drawn);

        lines_drawn += 1;

        if (lines_drawn == frame->height)    { break; }
    }
}

static void yed_frame_set_pos(yed_frame *frame, int top, int left) {
    frame->top  = top;
    frame->left = left;
}

static void yed_frame_set_buff(yed_frame *frame, yed_buffer *buff) {
    frame->buffer = buff;
    frame->dirty  = 1;
}

static void yed_frame_update(yed_frame *frame) {
    if (frame->buffer) {
        if (frame->dirty) {
            yed_frame_draw_buff(frame, frame->buffer, frame->buffer_offset);
        }
        if (frame == ys->active_frame) {
            yed_frame_update_cursor_line(frame);
        }
    } else {
        if (frame->dirty) {
            yed_clear_frame(frame);
        }
    }

    frame->dirty = 0;
}

static int yed_update_frame_buffer_offset(yed_frame *f, int col, int row) {
    int buff_n_lines;

    buff_n_lines = array_len(f->buffer->lines);

    if (buff_n_lines > 10) {

        if ((row > 0 && f->cur_y == f->top + f->height - 1 - 5)
        ||  (row < 0 && f->cur_y == f->top + 5)) {

            f->buffer_offset += row;
            f->cursor_line   += row;
            f->buffer_offset  = MAX(MIN(f->buffer_offset, buff_n_lines - f->height - 1), 0);
            f->dirty          = 1;

            if (row > 0 && f->buffer_offset < buff_n_lines - f->height - 1) {
                return 1;
            }
            if (row < 0 && f->buffer_offset >= 1) {
                return 1;
            }
        }
    }

    return 0;
}

static void yed_move_cursor_within_bufferless_frame(yed_frame *f, int col, int row) {
    f->cur_x += col;
    f->cur_y += row;

    LIMIT(f->cur_y, f->top,  f->top + f->height - 1);
    LIMIT(f->cur_x, f->left, f->left + f->width - 1);

    f->desired_x = f->cur_x;
}

static void yed_move_cursor_within_frame(yed_frame *f, int col, int row) {
    int       changed_buff_off;
    yed_line *current_line;

    if (!f->buffer) {
        yed_move_cursor_within_bufferless_frame(f, col, row);
        return;
    }

    changed_buff_off = yed_update_frame_buffer_offset(f, col, row);

    if (!changed_buff_off) {
        f->cur_y += row;
        LIMIT(f->cur_y, f->top,
			  MIN(array_len(f->buffer->lines), f->top + f->height - 1));
    }

    f->cursor_line = f->buffer_offset + (f->cur_y - f->top + 1);
    current_line   = array_item(f->buffer->lines, f->cursor_line - 1);

    if (col == 0) {
        f->cur_x = MIN(f->desired_x, f->left + array_len(current_line->chars));
    } else {
        f->cur_x += col;
        f->cur_x  = MIN(f->cur_x, f->left + array_len(current_line->chars));
        LIMIT(f->cur_x, f->left, f->left + f->width - 1);

        f->desired_x = f->cur_x;
    }
}

static void yed_move_cursor_within_active_frame(int col, int row) {
    if (ys->active_frame) {
        yed_move_cursor_within_frame(ys->active_frame, col, row);
    }
}

static void yed_update_frames(void) {
    yed_frame                                *frame;
    tree_it(yed_frame_id_t, yed_frame_ptr_t)  it;

    tree_traverse(ys->frames, it) {
        frame = tree_it_val(it);
        if (frame == ys->active_frame) {
            continue;
        }
        yed_frame_update(frame);
    }

    if (ys->active_frame) {
        yed_frame_update(ys->active_frame);
        yed_set_cursor(ys->active_frame->cur_x, ys->active_frame->cur_y);
    }
}

static void yed_frame_update_cursor_line(yed_frame *frame) {
    yed_line *line;

    /* Line above */
    if (frame->cur_y > frame->top
    &&  frame->cursor_line > 1) {

        line = array_item(frame->buffer->lines, frame->cursor_line - 2);
        yed_frame_draw_line(frame, line, frame->cur_y - frame->top - 1);
    }

    /* Line below */
    if (frame->cur_y < frame->top + frame->height - 1
    &&  frame->cursor_line < array_len(frame->buffer->lines)) {

        line = array_item(frame->buffer->lines, frame->cursor_line);
        yed_frame_draw_line(frame, line, frame->cur_y - frame->top + 1);
    }

    /* Current line */
    line = array_item(frame->buffer->lines, frame->cursor_line - 1);
    append_to_output_buff(TERM_BG_BLUE);
    append_to_output_buff(TERM_DARK_GRAY);
    yed_frame_draw_line(frame, line, frame->cur_y - frame->top);
    append_to_output_buff(TERM_RESET);
    append_to_output_buff(TERM_CURSOR_HIDE);
}

static void yed_frame_take_key(yed_frame *frame, int key) {
    char *key_str;
    char  key_str_buff[2];

    switch (key) {
        case KEY_UP:        yed_execute_command("cursor-up",    0, NULL); break;
        case KEY_DOWN:      yed_execute_command("cursor-down",  0, NULL); break;
        case KEY_RIGHT:     yed_execute_command("cursor-right", 0, NULL); break;
        case KEY_LEFT:      yed_execute_command("cursor-left",  0, NULL); break;
        case KEY_BACKSPACE: yed_execute_command("delete-back",  0, NULL); break;
        default: {
            if (key == '\n' || !iscntrl(key)) {
                key_str_buff[0] = (char)key;
                key_str_buff[1] = 0;
                key_str         = key_str_buff;
                yed_execute_command("insert", 1, &key_str);
            }
        }
    }
}
