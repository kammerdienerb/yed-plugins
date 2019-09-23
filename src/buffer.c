#include "internal.h"

static yed_buffer yed_new_buff(void) {
    yed_buffer buff;

    buff.lines = array_make(yed_line);
	buff.path  = NULL;

    return buff;
}

static void yed_append_to_line(yed_line *line, char c) {
    array_push(line->chars, c);
}

static void yed_append_to_buff(yed_buffer *buff, char c) {
    yed_line *line;

    if (c == '\n') {
        line        = array_next_elem(buff->lines);
        line->chars = array_make(char);
    } else {
        if (array_len(buff->lines) == 0) {
            line        = array_next_elem(buff->lines);
            line->chars = array_make(char);
        } else {
            line = array_last(buff->lines);
        }

        yed_append_to_line(line, c);
    }
}

static void yed_fill_buff_from_file(yed_buffer *buff, const char *path) {
    FILE *f;
    char  c;

    f = fopen(path, "r");
    if (!f) {
        ERR;
        return;
    }

    while ((c = fgetc(f)) != EOF) {
        yed_append_to_buff(buff, c);
    }

	buff->path = path;

    fclose(f);
}

static void yed_write_buff_to_file(yed_buffer *buff, const char *path) {
    FILE       *f;
    yed_line   *line;
    const char *nl;

    f = fopen(path, "w");
    if (!f) {
        ERR;
        return;
    }

    nl = "";

    array_traverse(buff->lines, line) {
        fprintf(f, "%s", nl);
        fwrite(array_data(line->chars), 1, array_len(line->chars), f);
        nl = "\n";
    }

    fclose(f);
}
