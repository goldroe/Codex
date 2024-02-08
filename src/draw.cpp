inline internal string string_make(char *data, int count);
internal void free_string(string *s);
internal f32 get_string_width(string s, FontAtlas *atlas);
internal string get_line_string(Buffer *buffer, s32 line);
inline internal u8 char_from_pos(Buffer *buffer, s64 pos);
internal string buffer_text(Buffer *buffer);
internal void append(StringBuilder *builder, string s);
internal void free_builder(StringBuilder *b);
internal string join(string first, string second);

internal void free_render_target(RenderTarget *target) {
    for (RenderBatch *batch = target->batches; batch != nullptr; ) {
        RenderBatch *temp = batch;
        batch = batch->next;
        temp->vertices.clear();
        free(temp);
    }
    target->texture = 0;
    target->batches = nullptr;
    target->current = nullptr;
}

internal RenderBatch *new_render_batch(RenderTarget *target) {
    RenderBatch *batch = (RenderBatch *)malloc(sizeof(RenderBatch));
    block_zero(batch, sizeof(RenderBatch));
    if (target->batches == nullptr) {
        target->batches = batch;
        target->current = batch;
    } else {
        target->current->next = batch;
    }
    target->current = batch;
    return batch;
}

internal void set_texture(RenderTarget *target, u32 texture) {
    if (target->texture != texture) {
        RenderBatch *batch = new_render_batch(target);
        batch->texture = texture;
        target->texture = texture;
    }
}

internal void set_clip_box(RenderTarget *target, Rect rect) {
    // set clip box for text "containers" 
}

internal void push_batch_vertices(RenderTarget *target, Vertex *vertices, s32 count) {
    RenderBatch *batch = target->current;
    if (target->current == nullptr) {
        batch = new_render_batch(target);
    }

    for (s32 i = 0; i < count; i++) {
        batch->vertices.push(vertices[i]);
    }
}

internal void draw_rectangle(RenderTarget *target, Rect rect, Color color) {
    Vertex vertices[6];
    vertices[0] = { rect.x0, rect.y1, 0.0f, 0.0f, color.r, color.g, color.b };
    vertices[1] = { rect.x0, rect.y0, 0.0f, 0.0f, color.r, color.g, color.b };
    vertices[2] = { rect.x1, rect.y0, 0.0f, 0.0f, color.r, color.g, color.b };
    vertices[3] = vertices[0];
    vertices[4] = vertices[2];
    vertices[5] = { rect.x1, rect.y1, 0.0f, 0.0f, color.r, color.g, color.b };

    // maybe don't since uv(0,0) is white?
    set_texture(target, target->fallback_texture);

    push_batch_vertices(target, vertices, ARRAYCOUNT(vertices));
}

internal void draw_glyph(RenderTarget *target, FontAtlas *atlas, Vector2 p, u8 codepoint, Color color) {
    FontGlyph glyph = atlas->glyphs[codepoint];
    Vertex vertices[6];
    vertices[0] = {p.x, p.y, glyph.to, glyph.by / atlas->height,                                      color.r, color.g, color.b};
    vertices[1] = {p.x, p.y - glyph.by, glyph.to, 0.0f,                                               color.r, color.g, color.b};
    vertices[2] = {p.x + glyph.bx, p.y - glyph.by, glyph.to + glyph.bx / atlas->width, 0.0f,          color.r, color.g, color.b};
    vertices[3] = {p.x, p.y, glyph.to, glyph.by / atlas->height,                                      color.r, color.g, color.b};
    vertices[4] = {p.x + glyph.bx, p.y - glyph.by, glyph.to + glyph.bx / atlas->width, 0.0f,          color.r, color.g, color.b};
    vertices[5] = {p.x + glyph.bx, p.y, glyph.to + glyph.bx / atlas->width, glyph.by / atlas->height, color.r, color.g, color.b};

    push_batch_vertices(target, vertices, ARRAYCOUNT(vertices));
}

internal void draw_text(RenderTarget *target, string text, FontAtlas *atlas, Vector2 offset, Vector2 position, Color color) {
    set_texture(target, atlas->id);

    Vector2 start = Vector2(0.0f, -offset.y);
    for (size_t i = 0; i < text.count; i++) {
        if (text.data[i] == '\n') {
            start.x = 0.0f;
            start.y += atlas->glyph_height;
        }

        FontGlyph glyph = atlas->glyphs[text.data[i]];
        float x = position.x - offset.x + start.x + glyph.bl;
        float y = position.y + start.y - glyph.bt + glyph.by + atlas->ascend;
        Vector2 p = Vector2(x, y);

#if 0
        // line wrapping
        if (x + glyph.bx > 1200) {
            start.y += atlas->glyph_height;
            start.x = 0;
            x = 0;
            y += atlas->glyph_height;
        }
#endif
    
        draw_glyph(target, atlas, p, text.data[i], color);

        start.x += glyph.ax;
    }
}

internal void draw_view(RenderTarget *target, View *view, FontAtlas *atlas) {
    // background
    if (view->is_commandbuf) {
        draw_rectangle(target, view->rect, rgb_to_color(0xC4A872));
    } else {
        draw_rectangle(target, view->rect, theme_background);
    }
    
    // cursor line
    {
        float y = (view->cursor.line * atlas->glyph_height) - (view->line_offset * atlas->glyph_height);
        draw_rectangle(target, {view->rect.x0, y, view->rect.x1, y + atlas->glyph_height}, theme_line);
    }

    // text
    string text = buffer_text(view->buffer);
    if (view->is_commandbuf) {
        draw_text(target, text, atlas, Vector2(0.0f, view->line_offset * atlas->glyph_height), Vector2(view->rect.x0, view->rect.y0), theme_commandbuf_fg);
    } else {
        draw_text(target, text, atlas, Vector2(0.0f, view->line_offset * atlas->glyph_height), Vector2(view->rect.x0, view->rect.y0), theme_foreground);
    }
    free_string(&text);

    // selection
    if (view->select_active) {
        Cursor start = view->cursor;
        Cursor end = view->select_cursor;
        if (end.pos < start.pos) {
            Cursor temp = start;
            start = end;
            end = temp;
        }

        {
            string s = get_line_string(view->buffer, start.line);
            string shift = {s.data + start.col, s.count - start.col};
            f32 x = 0.0f;
            f32 y = (start.line * atlas->glyph_height) - (view->line_offset * atlas->glyph_height);
            f32 width = get_string_width(shift, atlas);
            free_string(&s);
            draw_rectangle(target, {x, y, x + width, y + atlas->glyph_height}, theme_select);
        }

        for (int line = start.line + 1; line < end.line; line++) {
            Rect r{};
            r.x0 = view->rect.x0;
            r.x1 = view->rect.x1;
            r.y0 = view->rect.y0 + (line * atlas->glyph_height) - (view->line_offset * atlas->glyph_height);
            r.y1 = r.y0 + atlas->glyph_height;
            float y = (line * atlas->glyph_height) - (view->line_offset * atlas->glyph_height);
            draw_rectangle(target, r, theme_select);

            string s = get_line_string(view->buffer, line);
            draw_text(target, s, atlas, Vector2(0.0f, view->line_offset * atlas->glyph_height), Vector2(view->rect.x0, view->rect.y0 + (line - view->line_offset) * atlas->glyph_height), theme_background);
            free_string(&s);
        }

        {
            string s = get_line_string(view->buffer, end.line);
            assert(end.col < s.count);
            string shift = {s.data, end.col};
            f32 y = (end.line * atlas->glyph_height) - (view->line_offset * atlas->glyph_height);
            f32 x0 = 0.0f;
            f32 x1 = x0 + get_string_width(shift, atlas);
            draw_rectangle(target, {x0, y, x1, y + atlas->glyph_height}, theme_select);
            free_string(&s);
        }
    }

    // cursor bg and fg
    float cursor_x = view->rect.x0;
    float cursor_y = view->rect.y0 + view->cursor.line * atlas->glyph_height;
    cursor_y -= view->line_offset * atlas->glyph_height;
    for (s64 pos = view->cursor.pos - view->cursor.col; pos < view->cursor.pos; pos++) {
        u8 c = char_from_pos(view->buffer, pos);
        FontGlyph glyph = atlas->glyphs[c];
        cursor_x += glyph.ax;
    }
    u8 c = view->buffer->text->contents ? char_from_pos(view->buffer, view->cursor.pos) : ' ';
    float cursor_width = atlas->glyphs[c].ax;
    if (cursor_width == 0.0f) cursor_width = atlas->glyphs[' '].ax;
    Rect cursor_rect = {cursor_x, cursor_y, cursor_x + cursor_width, cursor_y + atlas->glyph_height};
    draw_rectangle(target, cursor_rect, theme_cursor);
    draw_text(target, string_make((char *)&c, 1), atlas, Vector2(), Vector2(cursor_x, cursor_y), theme_foreground);

    // file bar
    if (view->buffer->file_name.count > 0) {
        draw_rectangle(target, {0, (float)target->height - atlas->glyph_height, (float)target->width, (float)target->height}, theme_commandbuf_bg);
        StringBuilder builder{};
        append(&builder, view->buffer->file_name);
        int line = view->cursor.line + 1;
        int col = view->cursor.col;
        size_t n = snprintf(NULL, 0, "  (%d, %d)", line, col);
        char *buf = (char *)malloc(n + 1);
        snprintf(buf, n + 1, "  (%d, %d)", line, col);
        append(&builder, string_make(buf, (int)n));

        draw_text(target, string_make(builder.data, (int)builder.count), atlas, Vector2(), Vector2(0.0f, (float)target->height - atlas->glyph_height), theme_commandbuf_fg);

        free_builder(&builder);
    }
}
