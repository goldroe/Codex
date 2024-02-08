#include "codex.h"
#include <algorithm>

#define GAP_SIZE 1024

Color theme_background = rgb_to_color(0xFFFFFF);
Color theme_foreground = rgb_to_color(0);
Color theme_cursor = rgb_to_color(0);
Color theme_select = rgb_to_color(0xC0C0C0);
Color theme_line = rgb_to_color(0xFFFFCD);

Color theme_commandbuf_fg = rgb_to_color(0);
Color theme_commandbuf_bg = rgb_to_color(0xF0F0F0);

global LARGE_INTEGER performance_frequency;
global bool window_should_close;

global Application *application;

global Keymap normal_keymap;
global Keymap insert_keymap;
global Keymap select_keymap;
global Keymap goto_keymap;
global Keymap command_keymap;

global char last_insert_char;
global Array<InputEvent> input_events;

global RenderTarget render_target;

internal s32 get_line_length(Buffer *buffer, s64 line);

internal string string_make(char *str, int count) {
    string s;
    s.data = str;
    s.count = count;
    return s;
}

internal string string_make(char *str) {
    string s{};
    s.data = str;
    s.count = (int)strlen(str);
    return s;
}

internal Array<string> split(string s) {
    Array<string> splits;
    for (int i = 0, start = 0; i < s.count; i++) {
        // found space or end of string
        if (s[i] == ' ') {
            string new_s = string_make(s.data + start, i - start);
            start = i + 1;
            splits.push(new_s);
        } else if (i == s.count - 1) {
            string new_s = string_make(s.data + start, i - start + 1);
            splits.push(new_s);
            break;
        }
    }
    return splits;
} 

internal string join(string first, string second) {
    char *str = (char *)malloc(first.count + second.count + 1);
    memcpy(str, first.data, first.count);
    memcpy(str + first.count, second.data, second.count);
    str[first.count + second.count] = '\0';

    string result;
    result.data = str;
    result.count = first.count + second.count;
    return result;
}

internal void free_string(string *s) {
    if (s->data) {
        free(s->data);
    }
    s->data = nullptr;
    s->count = 0;
}

internal void free_builder(StringBuilder *b) {
    if (b->data) {
        free(b->data);
    }
    b->data = nullptr;
    b->count = 0;
    b->capacity = 0;
}

internal void string_emplace_realloc(StringBuilder *builder, string str) {
    size_t max = SIZE_MAX;
    size_t new_count = builder->count + str.count;
    size_t old_capacity = builder->capacity;
    size_t new_capacity = new_count + 1;

    if (new_capacity > max - new_capacity / 2) {
        // overflow
        new_capacity = max;
    } else {
        // reallocate new count * 1.5
        new_capacity = new_capacity + new_capacity / 2;
    }
    builder->data = (char *)realloc(builder->data, new_capacity + 1);
    builder->capacity = new_capacity;

    size_t old_count = builder->count;
    builder->capacity = new_capacity;
    builder->count = new_count;
    memcpy(builder->data + old_count, str.data, str.count);
    builder->data[new_count] = '\0';
}

internal void string_emplace_capacity(StringBuilder *builder, string str) {
    size_t old_count = builder->count;
    memcpy(builder->data + old_count, str.data, str.count);
    builder->count = old_count + str.count;
    builder->data[builder->count] = '\0';
}

internal void string_emplace(StringBuilder *builder, string str) {
    size_t new_count = builder->count + str.count;
    if (builder->capacity < new_count) {
        string_emplace_realloc(builder, str);
    } else {
        string_emplace_capacity(builder, str);
    }
}

internal void append(StringBuilder *builder, string s) {
    string_emplace(builder, s);
}

internal string copy(string str) {
    string result;
    result.data = (char *)malloc(str.count + 1);
    memcpy(result.data, str.data, str.count + 1);
    result.count = str.count;
    return result;
}

internal void reserve(string *s, int count) {
    assert(count > 0);
    s->data = (char *)malloc(count + 1);
    s->count = count;
}

inline internal bool is_whitespace(u8 ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

internal bool unix_path_is_abs(string path) {
    assert(path.count > 0);
    if (path.data[0] == '/' || path.data[0] == '\\') {
        return true;
    }
    return false;
}

internal Color rgb_to_color(u32 rgb) {
    Color result{};
    result.r = (f32)(0xFF & (rgb >> 16)) / 255.0f;
    result.g = (f32)(0xFF & (rgb >> 8)) / 255.0f;
    result.b = (f32)(0xFF & (rgb >> 0)) / 255.0f;
    return result;
}

inline internal s64 buffer_gap_delta(Buffer *buffer) {
    s64 result = buffer->text->gap_end - buffer->text->gap_start;
    return result;
}

inline internal s64 buffer_length(Buffer *buffer) {
    s64 result = buffer->text->end - buffer_gap_delta(buffer);
    return result;
}

inline internal s64 raw_buffer_pos(Buffer *buffer, s64 pos) {
    s64 result = pos;
    if (pos >= buffer->text->gap_start) {
        result += buffer_gap_delta(buffer);
    }
    return result;
}

inline internal u8 char_from_pos(Buffer *buffer, s64 pos) {
    if (pos >= buffer_length(buffer)) {
        return 0;
    }
    s64 raw_pos = raw_buffer_pos(buffer, pos);
    return buffer->text->contents[raw_pos];
}

inline internal u8 *string_from_pos(Buffer *buffer, s64 pos) {
    u8 *result = buffer->text->contents;
    s64 raw_pos = raw_buffer_pos(buffer, pos);
    result = buffer->text->contents + raw_pos;
    return result;
}

internal Cursor get_cursor_from_pos(Buffer *buffer, s64 pos) {
    Cursor result{};
    result.pos = pos;
    for (s64 p = 0; p < pos; p++) {
        result.col++;
        if (char_from_pos(buffer, p) == '\n') {
            result.line++;
            result.col = 0;
        }
    }
    return result;
}

internal Cursor get_cursor_from_line(Buffer *buffer, s32 line) {
    assert(line <= buffer->text->line_bases.count - 1);
    Cursor result = {};
    result.line = line;
    result.pos = buffer->text->line_bases[line];
    result.col = 0;
    return result;
}

internal u8 view_char(View *view) {
    return char_from_pos(view->buffer, view->cursor.pos);
}

internal string get_line_string(Buffer *buffer, s32 line) {
    int len = get_line_length(buffer, line);
    char *buf = (char *)malloc(len + 1);
    buf[len] = 0;
    for (size_t i = 0, pos = get_cursor_from_line(buffer, line).pos; i < len; i++, pos++) {
        u8 c = char_from_pos(buffer, pos);
        buf[i] = (char)c;
    }
    return {buf, len};
}

internal f32 get_string_width(string s, FontAtlas *atlas) {
    f32 width = 0.0f;
    for (int i = 0; i < s.count; i++) {
        char c = s.data[i];
        FontGlyph glyph = atlas->glyphs[c];
        width += glyph.ax;
    }
    return width;
}

internal void write_file(string file_name, string file_data) {
    HANDLE file_handle = CreateFileA((char *)file_name.data, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle != INVALID_HANDLE_VALUE) {
        DWORD bytes_written = 0;
        if (WriteFile(file_handle, file_data.data, (DWORD)file_data.count, &bytes_written, NULL) && ((int)bytes_written == file_data.count)) {

        } else {
            // TODO: error handling
            printf("WriteFile: Error writing file: %s\n", file_name.data);
        }

        CloseHandle(file_handle);
    } else {
        // TODO: error handling
        printf("CreateFile: error opening file: %s\n", file_name.data);
    }
}

internal string read_file(string file_name) {
    string result{};
    HANDLE file_handle = CreateFileA(file_name.data, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle != INVALID_HANDLE_VALUE) {
        u64 bytes_to_read;
        if (GetFileSizeEx(file_handle, (PLARGE_INTEGER)&bytes_to_read)) {
            assert(bytes_to_read <= UINT32_MAX);
            result.data = (char *)malloc(bytes_to_read + 1);
            DWORD bytes_read;
            if (ReadFile(file_handle, result.data, (DWORD)bytes_to_read, &bytes_read, NULL) && (DWORD)bytes_to_read ==  bytes_read) {
                result.count = (u64)bytes_read;
                result.data[result.count] = '\0';
            } else {
                // TODO: error handling
                printf("ReadFile: error reading file, %s!\n", file_name.data);
            }
       } else {
            // TODO: error handling
            printf("GetFileSize: error getting size of file: %s!\n", file_name.data);
       }
       CloseHandle(file_handle);
    } else {
        // TODO: error handling
        printf("CreateFile: error opening file: %s!\n", file_name.data);
    }
    return result;
}

internal string lf_to_crlf(string str) {
    u8 *buf = (u8 *)calloc(str.count * 2, sizeof(u8));
    u8 *ptr = buf;
    u8 *mem = (u8 *)str.data;
    int len = 0;
    while (mem < (u8 *)str.data + str.count) {
        if (*mem == '\n') {
            *ptr++ = '\r';
            *ptr++ = '\n';
            len++;
        } else {
            *ptr++ = *mem;
        }
        mem++;
        len++;
    }

    buf = (u8 *)realloc(buf, len+1);
    buf[len] = 0;

    string result = {(char *)buf, len};
    return result;
}

internal string crlf_to_lf(string str) {
    u8 *buf = (u8 *)calloc(str.count, sizeof(u8));
    u8 *memory = (u8 *)str.data;
    u8 *ptr = buf;
    size_t len = 0;
    while (memory < (u8 *)str.data + str.count) {
        if (*memory != '\r') {
            *ptr++ = *memory;
            len++;
        }
        memory++;
    }
    buf = (u8 *)realloc(buf, len + 1);
    buf[len] = 0;

    string result;
    result.data = (char *)buf;
    result.count = (int)len;
    return result;
}

internal void update_line_bases(Buffer *buffer) {
    buffer->text->line_bases.clear();

    buffer->text->line_bases.push(0);
    for (s64 i = 0; i < buffer_length(buffer); i++) {
        if (char_from_pos(buffer, i) == '\n') {
            buffer->text->line_bases.push(i + 1);
        }
    }
    buffer->text->line_bases.push(buffer_length(buffer) + 1);
}

internal void buffer_clear(Buffer *buffer) {
    buffer->text->gap_start = 0;
    buffer->text->gap_end = buffer->text->end;
    update_line_bases(buffer);
}

internal string buffer_text(Buffer *buffer) {
    s64 n = buffer_length(buffer);
    u8 *ptr = (u8 *)malloc(n + 1);
    memcpy(ptr,
        buffer->text->contents, buffer->text->gap_start);
    memcpy(ptr + buffer->text->gap_start,
        buffer->text->contents + buffer->text->gap_end,
        buffer->text->end - buffer->text->gap_end);
    ptr[n] = '\0';

    string contents = {(char*)ptr, (int)n};
    return contents;
}

internal void buffer_gap_grow(Buffer *buffer) {
    assert(buffer->text->gap_start == buffer->text->gap_end);

    u8 *temp = (u8 *)calloc(buffer->text->end + GAP_SIZE + 1, sizeof(u8));
    temp[buffer->text->end + GAP_SIZE] = '\0';

    memcpy(temp,
        buffer->text->contents,
        buffer->text->gap_start);
    memcpy(temp + buffer->text->gap_start + GAP_SIZE,
        buffer->text->contents + buffer->text->gap_end,
        buffer->text->end - buffer->text->gap_end);

    free(buffer->text->contents);
    buffer->text->contents = temp;
    buffer->text->gap_end += GAP_SIZE;
    buffer->text->end += GAP_SIZE;
}

internal void gap_shift(Buffer *buffer, s64 new_gap) {
    u8 *temp = (u8 *)malloc((buffer->text->end + 1) * sizeof(u8));
    memset(temp, '-', buffer->text->end);
    temp[buffer->text->end] = '\0';

    s64 gap_delta = buffer_gap_delta(buffer);
    s64 old_gap_start = buffer->text->gap_start;
    s64 old_gap_end = buffer->text->gap_end;

    if (new_gap > old_gap_start) {
        memcpy(temp, buffer->text->contents, old_gap_start); // copy section before old gap
        memcpy(temp + old_gap_start, buffer->text->contents + old_gap_end, new_gap - old_gap_start); //?
        memcpy(temp + new_gap + gap_delta, buffer->text->contents + old_gap_end + new_gap - old_gap_start, buffer->text->end - (new_gap + gap_delta));
    } else {
        memcpy(temp, buffer->text->contents, new_gap);
        memcpy(temp + new_gap + gap_delta, buffer->text->contents + new_gap, old_gap_start - new_gap);
        memcpy(temp + new_gap + gap_delta + (old_gap_start - new_gap), buffer->text->contents + old_gap_end, buffer->text->end - old_gap_end);
    }

    memcpy(buffer->text->contents, temp, buffer->text->end);
    buffer->text->gap_start = new_gap;
    buffer->text->gap_end = new_gap + gap_delta;

    free(temp);
}

internal void buffer_ensure_gap(Buffer *buffer) {
    if (buffer_gap_delta(buffer) <= 0) {
        buffer_gap_grow(buffer);
    }
}

internal int get_line_count(Buffer *buffer) {
    int result = 1;
    // s64 len = buffer_length(text);
    // for (s64 p = 0; p < len; p++) {
    //     if (char_from_pos(text, p) == '\n') result++;
    // }
    result = (int)buffer->text->line_bases.count - 1;
    return result;
}

internal void insert_char(Buffer *buffer, s64 position, u8 c) {
    buffer_ensure_gap(buffer);

    if (position != buffer->text->gap_start) {
        gap_shift(buffer, position);
    }

    buffer->text->contents[position] = c;
    buffer->text->gap_start++;

    update_line_bases(buffer);
}

internal string copy_range(Buffer *buffer, s64 start, s64 end) {
    string result{};
    result.count = (int)(end - start);
    result.data = (char *)malloc(result.count + 1);
    u8 *ptr = buffer->text->contents;
    for (s64 i = 0, pos = start; pos < end; pos++, i++) {
        u8 *str = ptr + raw_buffer_pos(buffer, pos);
        result.data[i] = *str;
    }
    result.data[result.count] = '\0';
    return result;
}

internal void delete_range(Buffer *buffer, s64 start, s64 end) {
    if (buffer->text->gap_start != end) {
        gap_shift(buffer, end);
    }
    size_t str_index = 0;
    u8 *str = string_from_pos(buffer, start);
    for (s64 pos = start; pos < end; pos++) {
        str[str_index++] = 0;
    }
    buffer->text->gap_start = start;

    update_line_bases(buffer);
}

internal void delete_single(Buffer *buffer, s64 pos) {
    delete_range(buffer, pos, pos + 1);
}

internal s32 get_line_length(Buffer *buffer, s64 line) {
    s32 result = 0;
    result = (s32)(buffer->text->line_bases[line + 1] - buffer->text->line_bases[line]); // newline??
    return result;
}

internal s64 get_line_pos(Buffer *buffer, s64 line) {
    assert(line <= (s64)buffer->text->line_bases.count - 1);
    s64 result = buffer->text->line_bases[line];
    return result;
}

internal s64 get_line_end_pos(Buffer *buffer, s64 line) {
    s64 begin = get_line_pos(buffer, line);
    s64 end = begin + get_line_length(buffer, line) - 1;
    return end;
}

COMMAND_SIG(undefined) {
}

COMMAND_SIG(quit_codex) {
    printf("QUITTING\n");
    window_should_close = true;
}

COMMAND_SIG(normal_mode) {
    View *view = app->active_view;
    view->keymap = &normal_keymap;
    view->select_active = false;
}

COMMAND_SIG(insert_mode) {
    View *view = app->active_view;
    view->keymap = &insert_keymap;
    view->select_active = false;
}

COMMAND_SIG(select_mode) {
    View *view = app->active_view;
    view->keymap = &select_keymap;
    view->select_active = true;
    view->select_cursor = view->cursor;
}

COMMAND_SIG(command_mode) {
    app->command_mode = true;
    app->active_view = app->command_view;
    buffer_clear(app->command_view->buffer);
    app->command_view->cursor = get_cursor_from_pos(app->command_view->buffer, 0);
}

COMMAND_SIG(write_buffer);


COMMAND_SIG(extend_line) {
    View *view = app->active_view;
    select_mode(app);
    Cursor begin = get_cursor_from_line(view->buffer, view->cursor.line);
    Cursor end = get_cursor_from_pos(view->buffer, begin.pos + get_line_length(view->buffer, view->cursor.line) - 1);
    view->cursor = end;
    view->select_cursor = begin;
}

COMMAND_SIG(extend_line_below) {
    View *view = app->active_view;
    int line = view->cursor.line + 1;
    if (!view->select_active) {
        select_mode(app);
        line = view->cursor.line;
    }
    if (line >= get_line_count(view->buffer)) line = get_line_count(view->buffer) - 1;
    Cursor end = get_cursor_from_pos(view->buffer, get_line_end_pos(view->buffer, line));
    view->cursor = end;
}

COMMAND_SIG(exchange_selection_mark) {
    View *view = app->active_view;
    Cursor c = view->cursor;
    view->cursor = view->select_cursor;
    view->select_cursor = c;
}

inline internal Buffer *buffer_init(string file_name, string contents);

COMMAND_SIG(open) {
    View *view = app->active_view;
    if (app->command_args.empty()) return;

    string file_name;
    string file_text;
    string arg = app->command_args[0];
    assert(arg.count > 0);

    xp_path path = {(unsigned char *)arg.data, arg.count};
    if (xp_path_relative(path)) {
        // TODO: change to "default" directory
        file_name = join(view->buffer->default_directory, arg);
    } else {
        file_name = copy(arg);
    }
    string f = read_file(file_name);
    file_text = crlf_to_lf(f);
    free(f.data);

    Buffer *buffer = buffer_init(file_name, file_text);
    view->buffer = buffer;
}

COMMAND_SIG(write_buffer) {
    View *view = app->active_view;
    string text = buffer_text(view->buffer);
    string write_str = lf_to_crlf(text);

    string path;
    if (app->command_args.count > 0) {
        string arg = app->command_args[0];
        xp_path p = {(u8 *)arg.data, arg.count};
        if (xp_path_relative(p)) {
            path = copy(arg);
        } else {
            xp_path current = xp_current_path();
            path = join(string_make((char *)current.data, current.count), arg);
            xp_path_free(&current);
        }
    } else {
        path = view->buffer->file_name;
    }

    if (path.data) {
        write_file(path, write_str);
    } else {
        assert(0);
    }

    printf("Writing to %s\n", path.data);

    if (app->command_args.count > 0) {
        free_string(&path); 
    }

    free_string(&text);
    free_string(&write_str);
}

COMMAND_SIG(self_insert) {
    View *view = app->active_view;
    insert_char(view->buffer, view->cursor.pos, last_insert_char);
    view->cursor = get_cursor_from_pos(view->buffer, view->cursor.pos + 1);
}

COMMAND_SIG(goto_file_start) {
    View *view = app->active_view;
    view->cursor = {0, 0, 0};
    view->line_offset = 0;
}

COMMAND_SIG(goto_file_end) {
    View *view = app->active_view;
    view->cursor = get_cursor_from_pos(view->buffer,  buffer_length(view->buffer) - 1);
    view->line_offset = view->cursor.line - view->lines + 2;
}

COMMAND_SIG(goto_line_end) {
    View *view = app->active_view;
    view->cursor = get_cursor_from_pos(view->buffer, get_line_end_pos(view->buffer, view->cursor.line));
}

COMMAND_SIG(goto_line_start) {
    View *view = app->active_view;
    view->cursor = get_cursor_from_pos(view->buffer, get_line_pos(view->buffer, view->cursor.line));
}

COMMAND_SIG(insert_newline) {
    View *view = app->active_view;
    insert_char(view->buffer, view->cursor.pos, '\n');
    view->cursor = get_cursor_from_line(view->buffer, view->cursor.line + 1);
}

COMMAND_SIG(insert_tab) {
    View *view = app->active_view;
    for (int i = 0; i < 4; i++) {
        insert_char(view->buffer, view->cursor.pos, ' ');
    }
    view->cursor = get_cursor_from_pos(view->buffer, view->cursor.pos + 4);
}

COMMAND_SIG(insert_at_line_start) {
    View *view = app->active_view;
    s64 start = get_cursor_from_line(view->buffer, view->cursor.line).pos;
    s64 end = start + get_line_length(view->buffer, view->cursor.line) - 1;
    s64 pos;
    for (pos = start; pos < end; pos++) {
        u8 ch = char_from_pos(view->buffer, pos);
        if (!is_whitespace(ch)) {
            break;
        }
    }
    view->cursor = get_cursor_from_pos(view->buffer, pos);
    insert_mode(app);
}

// TODO: use this?
// COMMAND_SIG(insert_at_line_start) {
//     View *view = app->active_view;
//     int line = view->cursor.line;
//     int indent = get_line_indentation(view->buffer, line);
//     Cursor cursor = get_cursor_from_pos(buffer, view->cursor.pos + indent);
//     view->cursor = cursor;
// }

COMMAND_SIG(insert_at_line_end) {
    View *view = app->active_view;
    s64 pos = get_line_end_pos(view->buffer, view->cursor.line);
    view->cursor = get_cursor_from_pos(view->buffer, pos);
    insert_mode(app);
}

COMMAND_SIG(open_line_above) {
    View *view = app->active_view;
    s64 pos = get_line_pos(view->buffer, view->cursor.line) - 1;
    if (pos < 0) pos = 0;
    insert_char(view->buffer, pos, '\n');
    view->cursor = get_cursor_from_line(view->buffer, view->cursor.line);
    insert_mode(app);
}

int get_line_indentation(Buffer *buffer, int line) {
    int indent = 0;
    if (line >= 0) {
        string s = get_line_string(buffer, line);
        for (int i = 0; i < s.count; i++) {
            if (s[i] != ' ') {
                break;
            }
            indent++;
        }
    }
    return indent;
}

COMMAND_SIG(open_line) {
    View *view = app->active_view;
    int indent = get_line_indentation(view->buffer, view->cursor.line);
    s64 pos = get_line_end_pos(view->buffer, view->cursor.line);
    insert_char(view->buffer, pos++, '\n');
    for (int i = 0; i < indent; i++, pos++) {
        insert_char(view->buffer, pos, ' ');
    }
    view->cursor = get_cursor_from_pos(view->buffer, get_line_end_pos(view->buffer, view->cursor.line + 1));
    insert_mode(app);
}

COMMAND_SIG(delete_char_backward) {
    View *view = app->active_view;
    if (view->cursor.pos > 0) {
        delete_single(view->buffer, view->cursor.pos -1);
        view->cursor = get_cursor_from_pos(view->buffer, view->cursor.pos - 1);
    }
}

COMMAND_SIG(move_char_left) {
    View *view = app->active_view;
    Cursor c = view->cursor;
    if (c.pos > 0) {
        view->cursor = get_cursor_from_pos(view->buffer, view->cursor.pos - 1);
    }
    if (view->cursor.col < view->col_offset) {
        view->col_offset = view->cursor.col;
    }
}

COMMAND_SIG(move_char_right) {
    View *view = app->active_view;
    Cursor c = view->cursor;
    if (c.pos < buffer_length(view->buffer)) {
        view->cursor = get_cursor_from_pos(view->buffer, view->cursor.pos + 1);
    }
    s32 cols = get_line_length(view->buffer, view->cursor.line);
    if (view->cursor.col > view->col_offset + cols - 1) {
        view->col_offset = view->cursor.col - cols + 1;
    }
}

COMMAND_SIG(move_next_word_end) {
    View *view = app->active_view;
    Buffer *buffer = view->buffer;
    s64 length = buffer_length(buffer);

    // go to non-whitespace char
    s64 pos = view->cursor.pos;
    if (is_whitespace(char_from_pos(buffer, pos)) || is_whitespace(char_from_pos(buffer, pos + 1))) {
        for (pos = view->cursor.pos + 1; pos < length; pos++) {
            if (!is_whitespace(char_from_pos(view->buffer, pos))) {
                break;
            }
        }
    }

    s64 bound = pos + 1;
    for (pos = pos + 1; pos < length; pos++) {
        if (is_whitespace(char_from_pos(view->buffer, pos))) {
            bound = pos - 1;
            break;
        }
    }

    bound = clamp(bound, 0, buffer_length(buffer));

    view->cursor = get_cursor_from_pos(buffer, bound);
}

COMMAND_SIG(move_line_up) {
    View *view = app->active_view;
    if (view->cursor.line > 0) {
        Cursor cursor = view->cursor;
        cursor.line = clamp(cursor.line - 1, 0, get_line_count(view->buffer));
        cursor.col = clamp(cursor.col, 0, get_line_length(view->buffer, cursor.line) - 1);
        cursor.pos = get_line_pos(view->buffer, cursor.line) + cursor.col;
        view->cursor = cursor;
        if (view->cursor.line < view->line_offset) {
            view->line_offset = view->cursor.line;
        }
    }
}

COMMAND_SIG(move_line_down) {
    View *view = app->active_view;
    if (view->cursor.line < get_line_count(view->buffer) - 1) {
        Cursor cursor = view->cursor;
        cursor.line = clamp(cursor.line + 1, 0, get_line_count(view->buffer));
        cursor.col = clamp(cursor.col, 0, get_line_length(view->buffer, cursor.line) - 1);
        cursor.pos = get_line_pos(view->buffer, cursor.line) + cursor.col;
        view->cursor = cursor;
        if (cursor.line > view->line_offset + view->lines - 1) {
            view->line_offset = cursor.line - view->lines + 1;
        }
    }
}

bool buffer_line_empty(Buffer *buffer, int line) {
    bool empty = true;
    s64 end = get_line_end_pos(buffer, line);
    for (s64 pos = get_line_pos(buffer, line); pos < end; pos++) {
        u8 c = char_from_pos(buffer, pos);
        if (!is_whitespace(c)) {
            empty = false;
            break;
        }
    }
    return empty;
}

COMMAND_SIG(move_paragraph_up) {
    View *view = app->active_view;
    int line_count = get_line_count(view->buffer);
    int move_line = view->cursor.line;
    for (int line = view->cursor.line - 1; line >= 0; line--) {
        if (buffer_line_empty(view->buffer, line)) {
            move_line = line;
            break;
        }
    }

    view->cursor = get_cursor_from_pos(view->buffer, get_line_pos(view->buffer, move_line));
}

COMMAND_SIG(move_paragraph_down) {
    View *view = app->active_view;
    int line_count = get_line_count(view->buffer);
    int move_line = view->cursor.line;
    for (int line = view->cursor.line + 1; line < line_count; line++) {
        if (buffer_line_empty(view->buffer, line)) {
            move_line = line;
            break;
        }
    }

    view->cursor = get_cursor_from_pos(view->buffer, get_line_pos(view->buffer, move_line));
}

COMMAND_SIG(page_up) {
    View *view = app->active_view;
    s32 line = view->cursor.line;
    line -= view->lines;
    if (line < 0) {
        line = 0;
    }
    view->line_offset = line;
    view->cursor = get_cursor_from_line(view->buffer, line);
}

COMMAND_SIG(page_down) {
    View *view = app->active_view;
    int line = view->cursor.line;
    line += view->lines;
    int line_count = get_line_count(view->buffer);
    if (line >= line_count) {
        line = line_count - 1;
    }
    view->line_offset = line;
    view->cursor = get_cursor_from_line(view->buffer, line);
}

COMMAND_SIG(delete_selection) {
    View *view = app->active_view;
    s64 start = view->cursor.pos;
    s64 end = view->select_cursor.pos;
    if (end < start) {
        s64 p = start;
        start = end;
        end = p;
    }

    if (!view->select_active) {
        start = view->cursor.pos;
        end = start;
    }

    delete_range(view->buffer, start, end + 1);
    view->cursor = get_cursor_from_pos(view->buffer, start);
    view->select_cursor = view->cursor;
    normal_mode(app);
}

Array<StringMatch> buffer_search_forward(Buffer *buffer, string pattern, s64 pos) {
    Array<StringMatch> match_list{};
    s64 last = buffer_length(buffer);
    for (s64 index = pos; index < last; index++) {
        bool matches = true;
        for (int pidx = 0; pidx < pattern.count; pidx++) {
            if (pattern[pidx] != char_from_pos(buffer, index + pidx)) {
                matches = false;
                break;
            }
        }
        if (matches) {
            StringMatch match{};
            match.pos = index;
            match.count = pattern.count;
            match_list.push(match);
        }
    }
    return match_list;
}

COMMAND_SIG(search) {
    if (app->command_args.count < 1) {
        printf("Need word to search..\n");
        return;
    }
    View *view = app->active_view;    
    Buffer *buffer = view->buffer;
    string pattern = app->command_args[0];
    Array<StringMatch> match_list = buffer_search_forward(buffer, pattern, view->cursor.pos);
    if (match_list.count > 0) {
        view->cursor = get_cursor_from_pos(buffer, match_list[0].pos);
    }
    match_list.clear();
}

global TypeableCommand typeable_commands[] = {
    { CONSTZ("write"),  { CONSTZ("w"), CONSTZ("f") }, write_buffer },
    { CONSTZ("quit"),   { CONSTZ("q") }, quit_codex },
    { CONSTZ("open"),   { CONSTZ("o") }, open },
    { CONSTZ("search"), {},             search },
};

COMMAND_SIG(exit_command_mode) {
    View *view = app->active_view;
    app->active_view = app->view_list;
    app->command_mode = false;
    normal_mode(app);
    
    string s = buffer_text(view->buffer);
    Array<string> args = split(s);
    if (args.empty()) return;
    
    string name = args[0];
    args.data++;
    args.count--;

    for (int i = 0; i < ARRAYCOUNT(typeable_commands); i++) {
        TypeableCommand *command = &typeable_commands[i];
        if (strncmp(command->name.data, name.data, command->name.count) == 0) {
            printf("%s\n", command->name.data);
            app->command_args = args;
            command->proc(app);
        }
    }
    free_string(&s);
}

internal bool execute_command(Keymap *keymap, Array<InputEvent> &events) {
    for (int i = 0; i < events.count; i++) {
        if (keymap == nullptr) return true;
        KeyBind bind = keymap->bindings[events[i].key];
        if (bind.kind == KeyBind::Command) {
            bind.command.proc(application);
            return true;
        } else {
            keymap = bind.map;
        }
    }
    return false;
}

internal bool win32_get_window_size(HWND window, int *w, int *h) {
    RECT rc{};
    if (GetClientRect(window, &rc)) {
        *w = rc.right - rc.left;
        *h = rc.bottom - rc.top;
        return true;
    }
    return false;
}

internal void resize_application(RenderTarget *target, int w, int h) {
    int dx = w - target->width;
    int dy = h - target->height;
    Array<View*> views;
    for (View *view = application->view_list; view != nullptr; view = view->next) {
        if (!view->is_commandbuf) {
            view->rect.x1 += dx;
            view->rect.y1 += dy;
            view->lines = (int)((view->rect.y1 - view->rect.y0) / view->atlas->glyph_height);
        }
    }
}

inline internal TextBuffer *text_buffer_init(string contents) {
    TextBuffer *text = (TextBuffer *)malloc(sizeof(TextBuffer));
    block_zero(text, sizeof(TextBuffer));
    text->contents = (u8*)contents.data;
    text->gap_start = 0;
    text->gap_end = 0;
    text->end = contents.count;
    return text;
}

inline internal TextBuffer *text_buffer_init() {
    TextBuffer *text = (TextBuffer *)malloc(sizeof(TextBuffer));
    block_zero(text, sizeof(TextBuffer));
    text->contents = (u8 *)malloc(GAP_SIZE);
    text->gap_start = 0;
    text->end = text->gap_end = GAP_SIZE;
    return text;
}

internal void push_buffer(Application *app, Buffer *buffer) {
    if (app->buffer_list == nullptr) {
        app->buffer_list = buffer;
        return;
    }
    Buffer *buf = app->buffer_list;
    while (buf) {
        if (buf->next == nullptr) {
            buf->next = buffer;
            break;
        }
        buf = buf->next;
    }
}

inline internal Buffer *buffer_init() {
    Buffer *buffer = (Buffer *)malloc(sizeof(Buffer));
    block_zero(buffer, sizeof(Buffer));
    buffer->text = text_buffer_init();
    update_line_bases(buffer);
    push_buffer(application, buffer);
    return buffer;
}

inline internal Buffer *buffer_init(string file_name, string contents) {
    Buffer *buffer = (Buffer *)malloc(sizeof(Buffer));
    block_zero(buffer, sizeof(Buffer));
    buffer->file_name = file_name;
    
    xp_path path = {(unsigned char *)file_name.data, file_name.count};
    path = xp_parent_path(path);
    buffer->default_directory = string_make((char *)path.data, path.count);
    
    buffer->text = text_buffer_init(contents);
    buffer->line_ending = LineEnding::CRLF;
    update_line_bases(buffer);
    push_buffer(application, buffer);
    return buffer;
}

internal void push_view(Application *app, View *view) {
    if (app->view_list == nullptr) {
        app->view_list = view;
        return;
    }
    View *v = app->view_list;
    while (v) {
        if (v->next == nullptr) {
            v->next = view;
            break;
        }
        v = v->next;
    }
}

internal View *view_init() {
    View *view = (View *)malloc(sizeof(View));
    block_zero(view, sizeof(View));
    view->keymap = &normal_keymap;
    push_view(application, view);
    return view;
}

internal Application *application_init() {
    Application *app = (Application *)malloc(sizeof(Application));
    block_zero(app, sizeof(Application));
    return app;
}

internal string parse_arguments(int argc, char **argv) {
    string result{};
    for (int i = 0; i < argc; i++) {
        if (*argv[i] == '-') continue;
        result = STRZ(argv[i]);
    }
    return result;
}
