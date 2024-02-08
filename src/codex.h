#ifndef CODEX_H
#define CODEX_H

#define ARRAYCOUNT(array) (sizeof((array)) / sizeof((array)[0]))
#define CONSTZ(str) { str, sizeof(str) - 1}
#define STRZ(str)  { str, (int)strlen(str)}

struct StringMatch {
    s64 pos;
    s64 count;
};

struct InputEvent {
    u32 key;
};

struct Vector2 {
    f32 x;
    f32 y;

    Vector2() {
        x = 0.0f;
        y = 0.0f;
    }

    Vector2(f32 x, f32 y) {
        this->x = x;
        this->y = y;
    }
};

struct Vertex {
    f32 x, y;
    f32 u, v;
    f32 r, g, b;
};

struct Color {
    f32 r, g, b;
};

struct Cursor {
    s64 pos;
    s32 line;
    s32 col;
};

enum LineEnding {
    CR,
    LF,
    CRLF
};

struct string {
    char *data;
    int count;
    char const &operator[](int i) {
        return data[i];
    }
};

struct StringBuilder {
    char *data;
    size_t count;
    size_t capacity;
};

struct FontGlyph {
    f32 ax;
    f32 ay;
    f32 bx;
    f32 by;
    f32 bt;
    f32 bl;
    f32 to;
};

struct FontAtlas {
    FontGlyph glyphs[128];
    GLuint id;
    int width;
    int height;
    int max_bmp_height;
    float ascend;
    float descend;
    int bbox_height;
    float glyph_width;
    float glyph_height;
};

struct Rect {
    f32 x0;
    f32 y0;
    f32 x1;
    f32 y1;
};

struct TextBuffer {
    u8 *contents;
    s64 gap_start;
    s64 gap_end;
    s64 end;
    Array<s64> line_bases;
};

struct Buffer {
    string file_name;
    TextBuffer *text;

    string default_directory;
    LineEnding line_ending;

    Buffer *next;
};

struct Keymap;
struct View {
    FontAtlas *atlas;
    Rect rect;
    Buffer *buffer;
    s32 lines;
    Cursor cursor;

    Keymap *keymap;

    s32 line_offset;
    s32 col_offset;

    b32 select_active;
    Cursor select_cursor;

    b32 is_commandbuf;

    View *next;
};

struct RenderBatch {
    u32 texture;
    Array<Vertex> vertices;
    RenderBatch *next;
};

struct RenderTarget {
    s32 width;
    s32 height;
    u32 texture;
    u32 fallback_texture;
    RenderBatch *batches;
    RenderBatch *current;
};

struct Application {
    Buffer *buffer_list;

    View *view_list;
    View *active_view;

    View *command_view;
    b32 command_mode;
    Array<string> command_args;
};

typedef void (*CommandProc)(Application *);

#define COMMAND_SIG(name) internal void name(Application *app)

// @note Keymaps
//    C-X           C-F
//    Key -> keymap -> key -> command 
// [Ctrl-Alt-Shift] [0left0-0xFE]
//        3             8
#define CTRL (1 << 8)
#define ALT (1 << 9)
#define SHIFT (1 << 10)
constexpr u16 max_key_count = (1 << (8 + 3));

struct TypeableCommand {
    string name;
    Array<string> aliases;
    CommandProc proc;
};

struct MappableCommand {
    string name;
    CommandProc proc;
};

struct KeyBind {
    enum { Command = 1, Map} kind;
    union {
        MappableCommand command;
        Keymap *map;
    };
};

struct Keymap {
    KeyBind bindings[max_key_count];

    inline void bind(u32 key, CommandProc command) {
        KeyBind binding{};
        binding.kind = KeyBind::Command;
        binding.command.name = {};
        binding.command.proc = command;
        bindings[key] = binding;
    }

    inline void bind(u32 key, Keymap *map) {
        KeyBind binding{};
        binding.kind = KeyBind::Map;
        binding.map = map;
        bindings[key] = binding;
    }
};

enum Edit {
    Insertion = 1,
    Deletion,
};

struct EditRecord {
    Edit kind;
    s64 pos;
    StringBuilder text;
};

internal Color rgb_to_color(u32 rgb);

#endif // CODEX_H
