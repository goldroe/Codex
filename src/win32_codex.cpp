#include <Windows.h>
#include <Windowsx.h>
#include <timeapi.h>
#include <shlwapi.h>
#include "win32_gl.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <glad/glad.h>

#include <assert.h>
#include <stdio.h>

#include <stdint.h>
#define internal static
#define local_persist static
#define global static
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef s32 b32;
typedef float f32;
typedef double f64;

#define WIDTH 1200
#define HEIGHT 900

#define FONT_NAME "fonts/consolas.ttf"
#define FONT_HEIGHT 18

template <typename V, typename L, typename H> V clamp(const V &value, const L &min, const H &max) {
    if (value < min) return V(min);
    if (value > max) return V(max);
    return value;
}

internal void block_zero(void *mem, size_t size) {
    memset(mem, 0, size);
}

#include "xpath.h"
#include "array.cpp"
#include "codex.cpp"

#include "draw.cpp"
#include "render.cpp"

internal inline float win32_get_seconds_elapsed(LARGE_INTEGER start, LARGE_INTEGER end) {
    float Result = (float)(end.QuadPart - start.QuadPart) / (float)performance_frequency.QuadPart;
    return Result;
}

internal inline LARGE_INTEGER win32_get_wall_clock() {
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;
}

internal void init_opengl_extensions() {
    // NOTE: Before we can load extensions, we need a dummy OpenGL context, created using a dummy window.
    // We use a dummy window because you can only set the pixel format for a window once. For the
    // real window, we want to use wglChoosePixelFormatARB (so we can potentially specify options
    // that aren't available in PIXELFORMATDESCRIPTOR), but we can't load and use that before we
    // have a context.
    WNDCLASSA window_class{};
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = DefWindowProcA;
    window_class.hInstance = GetModuleHandle(0);
    window_class.lpszClassName = "Dummy_WGL_djuasiodwa";
    
    if (!RegisterClassA(&window_class)) {
        printf("Failed to register dummy OpenGL window.");
    }
    
    HWND dummy_window = CreateWindowExA(0, window_class.lpszClassName, "Dummy OpenGL Window", 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, window_class.hInstance, 0);
    
    if (!dummy_window) {
        printf("Failed to create dummy OpenGL window.");
    }
    
    HDC dummy_dc = GetDC(dummy_window);
    
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    
    int pixel_format = ChoosePixelFormat(dummy_dc, &pfd);
    if (!pixel_format) {
        printf("Failed to find a suitable pixel format.");
    }
    if (!SetPixelFormat(dummy_dc, pixel_format, &pfd)) {
        printf("Failed to set the pixel format.");
    }
    
    HGLRC dummy_context = wglCreateContext(dummy_dc);
    if (!dummy_context) {
        printf("Failed to create a dummy OpenGL rendering context.");
    }
    
    if (!wglMakeCurrent(dummy_dc, dummy_context)) {
        printf("Failed to activate dummy OpenGL rendering context.");
    }
    
    wglCreateContextAttribsARB = (wglCreateContextAttribsARB_type*)wglGetProcAddress("wglCreateContextAttribsARB");
    wglChoosePixelFormatARB = (wglChoosePixelFormatARB_type*)wglGetProcAddress("wglChoosePixelFormatARB");
    
    wglMakeCurrent(dummy_dc, 0);
    wglDeleteContext(dummy_context);
    ReleaseDC(dummy_window, dummy_dc);
    DestroyWindow(dummy_window);
}

internal HGLRC init_opengl(HDC real_dc) {
    init_opengl_extensions();
    
    // Now we can choose a pixel format the modern way, using wglChoosePixelFormatARB.
    int pixel_format_attribs[] = {
        WGL_DRAW_TO_WINDOW_ARB,     GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB,     GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB,      GL_TRUE,
        WGL_ACCELERATION_ARB,       WGL_FULL_ACCELERATION_ARB,
        WGL_PIXEL_TYPE_ARB,         WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB,         32,
        WGL_DEPTH_BITS_ARB,         24,
        WGL_STENCIL_BITS_ARB,       8,
        0
    };
    
    int pixel_format;
    UINT num_formats;
    wglChoosePixelFormatARB(real_dc, pixel_format_attribs, 0, 1, &pixel_format, &num_formats);
    if (!num_formats) {
        printf("Failed to set the OpenGL 3.3 pixel format.");
    }
    
    PIXELFORMATDESCRIPTOR pfd;
    DescribePixelFormat(real_dc, pixel_format, sizeof(pfd), &pfd);
    if (!SetPixelFormat(real_dc, pixel_format, &pfd)) {
        printf("Failed to set the OpenGL 3.3 pixel format.");
    }
    
    // Specify that we want to create an OpenGL 3.3 core profile context
    int gl33_attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0,
    };
    
    HGLRC gl33_context = wglCreateContextAttribsARB(real_dc, 0, gl33_attribs);
    if (!gl33_context) {
        printf("Failed to create OpenGL 3.3 context.");
    }
    
    if (!wglMakeCurrent(real_dc, gl33_context)) {
        printf("Failed to activate OpenGL 3.3 rendering context.");
    }
    
    return gl33_context;
}

LRESULT CALLBACK win32_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    static bool control_down = false;
    static bool alt_down = false;
    static bool shift_down = false;
    
    LRESULT result = 0;
    switch (message) {
    case WM_KEYDOWN:
    case WM_KEYUP: {
        bool is_down = (lparam & (1 << 31)) == 0;
        bool was_down = (lparam & (1 << 30)) == 1;
        UINT vk_code = (UINT)wparam;
        switch (vk_code) {
        case VK_CONTROL:
            control_down = is_down;
            break;
        case VK_MENU:
            alt_down = is_down;
            break;
        case VK_SHIFT:
            shift_down = is_down;
            break;
         default:
            if (is_down) {
                u8 c = 0;
                BYTE keyboard_state[256]{};
                GetKeyboardState(keyboard_state);
                // @note ToAscii changes key combinations like Ctrl + M into different ascii characters but we want to actually keep the non-translated keys for keymap
                keyboard_state[VK_CONTROL] = 0;
                WORD characters{};
                if (ToAscii(vk_code, (lparam >> 16) & 0xFF, keyboard_state, &characters, (alt_down || shift_down)) == 1) {
                    c = (u8)characters;
                    last_insert_char = c;
                    // printf("Ascii: %c Val:%d\n", c, c);
                    u32 key = (control_down ? CTRL : 0) | (alt_down ? ALT : 0) | c;
                    InputEvent event = {key};
                    input_events.push(event);
                    if (execute_command(application->active_view->keymap, input_events)) {
                        input_events.clear();
                    }
                } else {
                    c = 0;
                    // last_input_event.character = 0;
                    // printf("NonAscii: %d\n", vk_code);
                }
            }
        }
        break;
    }
    case WM_CLOSE:
        window_should_close = true;
        DestroyWindow(hwnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        result = DefWindowProcA(hwnd, message, wparam, lparam);
    }
    return result;
}


int main(int argc, char **argv) {
    string arg = parse_arguments(argc - 1, argv + 1);
    QueryPerformanceFrequency(&performance_frequency);
    timeBeginPeriod(1);

    const int target_fps = 30;
    const DWORD target_ms_per_frame = (DWORD)(1000.0f * (1.0f / target_fps));
 
    HINSTANCE instance = GetModuleHandle(NULL);
    WNDCLASSA hwnd_class{};
    hwnd_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    hwnd_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    hwnd_class.lpfnWndProc = win32_proc;
    hwnd_class.lpszClassName = "codex_hwnd_class";
    
    if (RegisterClassA(&hwnd_class) == 0) {
        printf("Could not create main window!");
        return -1;
    }
    
    HWND window;
    {
        RECT rc;
        rc.left = 0; rc.right = WIDTH;
        rc.top = 0; rc.bottom = HEIGHT;
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        window = CreateWindowA(hwnd_class.lpszClassName, "Codex", WS_OVERLAPPEDWINDOW|WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, 0, 0, instance, NULL);
    }

    HDC dc = GetDC(window);
    HGLRC glrc = init_opengl(dc);
    
    if (!gladLoadGL()) {
        printf("Failed to initialize glad!");
        return -1;
    }

    // normal
    for (u32 i = 0; i < max_key_count; i++) {
        normal_keymap.bind(i, undefined);
    }
    normal_keymap.bind('i', insert_mode);
    normal_keymap.bind('a', command_mode);
    normal_keymap.bind('v', select_mode);
    normal_keymap.bind('h', move_char_left);
    normal_keymap.bind('j', move_line_down);
    normal_keymap.bind('k', move_line_up);
    normal_keymap.bind('l', move_char_right);
    normal_keymap.bind('e', move_next_word_end);
    // normal_keymap.bind('w', move_next_word_start);
    // normal_keymap.bind('b', move_prev_word_start);
    normal_keymap.bind('o', open_line);
    normal_keymap.bind('O', open_line_above);
    normal_keymap.bind('g', &goto_keymap);
    normal_keymap.bind('d', delete_selection);
    normal_keymap.bind('X', extend_line);
    normal_keymap.bind('x', extend_line_below);
    normal_keymap.bind('I', insert_at_line_start);
    normal_keymap.bind('A', insert_at_line_end);
    normal_keymap.bind(CTRL | 'b', page_up);
    normal_keymap.bind(CTRL | 'f', page_down);
    normal_keymap.bind(CTRL | 's', write_buffer);
    normal_keymap.bind('{', move_paragraph_up);
    normal_keymap.bind('}', move_paragraph_down);

    // insert
    for (u32 i = 0; i < max_key_count; i++) {
        insert_keymap.bind(i, undefined);
    }
    for (u8 i = 0; i < 255; i++) {
        if (isprint(i)) {
            insert_keymap.bind(i, self_insert);
        }
    }
    insert_keymap.bind('\t', insert_tab);
    insert_keymap.bind('\n', open_line);
    insert_keymap.bind('\r', open_line);
    insert_keymap.bind('\b', delete_char_backward);
    insert_keymap.bind(CTRL | ' ', normal_mode);

    // goto
    goto_keymap.bind('g', goto_file_start);
    goto_keymap.bind('e', goto_file_end);
    goto_keymap.bind('h', goto_line_start);
    goto_keymap.bind('l', goto_line_end);

    // select
    select_keymap = normal_keymap;
    select_keymap.bind('v', normal_mode);
    select_keymap.bind('o', exchange_selection_mark);

    // command
    command_keymap = insert_keymap;
    command_keymap.bind('\n', exit_command_mode);
    command_keymap.bind('\r', exit_command_mode);

    // LOAD FREETYPE FONT
    FontAtlas atlas{};
    {
        FT_Library ft_lib;
        int err = FT_Init_FreeType(&ft_lib);
        if (err) {
            printf("Error creaing freetype library: %d\n", err);
        }

        FT_Face face;
        err = FT_New_Face(ft_lib, FONT_NAME, 0, &face);
        if (err == FT_Err_Unknown_File_Format) {
            printf("Format not supported\n");
        } else if (err) {
            printf("Font file could not be read\n");
        }

        err = FT_Set_Pixel_Sizes(face, 0, FONT_HEIGHT);
        if (err) {
            printf("Error setting pixel sizes of font\n");
        }

        int bbox_ymax = FT_MulFix(face->bbox.yMax, face->size->metrics.y_scale) >> 6;
        int bbox_ymin = FT_MulFix(face->bbox.yMin, face->size->metrics.y_scale) >> 6;
        int height = bbox_ymax - bbox_ymin;
        float ascend = face->size->metrics.ascender / 64.f;
        float descend = face->size->metrics.descender / 64.f;
        float bbox_height = (float)(bbox_ymax - bbox_ymin);
        float glyph_height = (float)face->size->metrics.height / 64.f;
        float glyph_width = (float)(face->bbox.xMax - face->bbox.xMin) / 64.f;

        int atlas_width = 0;
        int atlas_height = 0;
        int max_bmp_height = 0;
        for (unsigned char c = 0; c < 128; c++) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                printf("Error loading char %c\n", c);
                continue;
            }

            atlas_width += face->glyph->bitmap.width;
            if (atlas_height < (int)face->glyph->bitmap.rows) {
                atlas_height = face->glyph->bitmap.rows;
            }

            int bmp_height = face->glyph->bitmap.rows + face->glyph->bitmap_top;
            if (max_bmp_height < bmp_height) {
                max_bmp_height = bmp_height;
            }
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        GLuint atlas_id;
        glGenTextures(1, &atlas_id);
        glBindTexture(GL_TEXTURE_2D, atlas_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlas_width, atlas_height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); 
 
        int atlas_x = 0;
        for (unsigned char c = 32; c < 128; c++) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                printf("Error loading char '%c'\n", c);
            }

            FontGlyph *glyph = &atlas.glyphs[c];
            glyph->ax = (float)(face->glyph->advance.x >> 6);
            glyph->ay = (float)(face->glyph->advance.y >> 6);
            glyph->bx = (float)face->glyph->bitmap.width;
            glyph->by = (float)face->glyph->bitmap.rows;
            glyph->bt = (float)face->glyph->bitmap_top;
            glyph->bl = (float)face->glyph->bitmap_left;
            glyph->to = (float)atlas_x / atlas_width;

            glTexSubImage2D(GL_TEXTURE_2D, 0, atlas_x, 0, (int)glyph->bx, (int)glyph->by, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);
            
            atlas_x += face->glyph->bitmap.width;
        }

        atlas.id = atlas_id;
        atlas.width = atlas_width;
        atlas.height = atlas_height;
        atlas.max_bmp_height = max_bmp_height;
        atlas.ascend = ascend;
        atlas.descend = descend;
        atlas.bbox_height = height;
        atlas.glyph_width = glyph_width;
        atlas.glyph_height = glyph_height;

        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

        FT_Done_Face(face);
        FT_Done_FreeType(ft_lib);
    }

    application = application_init();

    render_target.width = WIDTH;
    render_target.height = HEIGHT;


    string file_text{};
    string file_name{};
    if (arg.data) {
        xp_path file_path = xp_path_new(arg.data);
        if (xp_path_relative(file_path)) {
            file_path = xp_fullpath(file_path);
        }
        file_name = string_make((char *)file_path.data, file_path.count);
        string f = read_file(file_name);
        file_text = crlf_to_lf(f);
    } else {
        file_name = CONSTZ("[scratch]");
    }

    {
        View *view = view_init();
        application->active_view = view;
        view->rect = {0, 0, WIDTH, HEIGHT - atlas.glyph_height};
        view->buffer = buffer_init(file_name, file_text);
        view->lines = (int)(HEIGHT / atlas.glyph_height) - 1;
        view->atlas = &atlas;
        
        View *command_view = view_init();
        application->command_view = command_view;
        command_view->keymap = &command_keymap;
        command_view->rect = {0, HEIGHT - atlas.glyph_height, WIDTH, HEIGHT};
        command_view->buffer = buffer_init();
        command_view->lines = 1;
        command_view->is_commandbuf = true;
        command_view->atlas = &atlas;
    }

    LARGE_INTEGER start_counter = win32_get_wall_clock();
    LARGE_INTEGER last_counter = start_counter;

    while (!window_should_close) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            switch (msg.message) {
            case WM_QUIT:
                window_should_close = true;
                break;
            }
            
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        float window_width, window_height;
        {
            int w, h;
            win32_get_window_size(window, &w, &h);
            window_width = (float)w;
            window_height = (float)h;

            if (render_target.width != w || render_target.height != h) {
                resize_application(&render_target, w, h);
            }
            render_target.width = w;
            render_target.height = h;
        }

        for (View *view = application->view_list; view; view = view->next) {
            if (view->is_commandbuf && application->command_mode) {
                draw_view(&render_target, view, &atlas);
            } else if (!view->is_commandbuf) {
                draw_view(&render_target, view, &atlas);
            }
        }

        gl_render(&render_target);

        free_render_target(&render_target);

        SwapBuffers(dc);

        float work_seconds_elapsed = win32_get_seconds_elapsed(last_counter, win32_get_wall_clock());
        DWORD work_ms = (DWORD)(1000.0f * work_seconds_elapsed);
        if (work_ms < target_ms_per_frame) {
            DWORD sleep_ms = target_ms_per_frame - work_ms;
            Sleep(sleep_ms);
        }

        LARGE_INTEGER end_counter = win32_get_wall_clock();
#if 0
        float seconds_elapsed = 1000.0f * win32_get_seconds_elapsed(last_counter, end_counter);
        printf("seconds: %f\n", seconds_elapsed);
#endif
        last_counter = end_counter;
    }

    return 0;
}
