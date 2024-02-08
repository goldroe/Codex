global GLuint main_shader;

internal GLuint shader_load(const char *file_name) {
    printf("Loading gl shader:%s...", file_name);
    FILE *file = fopen(file_name, "rb");
    assert(file);
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    char *shader_src = (char *)malloc(file_size + 1);
    fread(shader_src, file_size, 1, file);
    shader_src[file_size] = 0;
    fclose(file);
    
    GLuint shader = glCreateProgram();
    int status = 0;
    int n;
    char log[512] = {};
    const char *vertex_header =
        "#version 330 core\n"
        "#define VERTEX_SHADER\n";
    const char *fragment_header = 
        "#version 330 core\n"
        "#define PIXEL_SHADER\n";
	
    const char *source_array[2] = {};
    source_array[0] = vertex_header;
    source_array[1] = shader_src;

    GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vshader, 2, source_array, NULL);
    glCompileShader(vshader);
    glGetShaderiv(vshader, GL_COMPILE_STATUS, &status);
    if (!status) {
        printf("Failed to compile vertex shader!\n");
    }
	
    glGetShaderInfoLog(vshader, 512, &n, log);
    if (n > 0) {
        printf("Error in vertex shader!\n");
        printf(log);
    }
	
    source_array[0] = fragment_header;
    GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fshader, 2, source_array, NULL);
    glCompileShader(fshader);
    glGetShaderiv(fshader, GL_COMPILE_STATUS, &status);
    if (!status) {
        printf("Failed to compile fragment shader!\n");
    }
    
    glGetShaderInfoLog(fshader, 512, &n, log);
    if (n > 0) {
        printf("Error in fragment shader!\n");
        printf(log);
    }
	
    glAttachShader(shader, vshader);
    glAttachShader(shader, fshader);
    glLinkProgram(shader);
    glDeleteShader(vshader);
    glDeleteShader(fshader);

    printf("SUCCESS\n");

    return shader;
}

internal void gl_render(RenderTarget *target) {
    local_persist bool initialized = false;
    if (!initialized) {
        initialized = true;

        // printf("SETTING UP TEXT SHADERS AND BUFFERS\n");
        main_shader = shader_load("src/text.glsl");
        glUseProgram(main_shader);

        GLuint vao;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        GLuint vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float), 0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void *)(2*sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void *)(4*sizeof(float)));

        u32 id;
        u8 white_pixel[] = {0xff, 0xff, 0xff, 0xff};
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white_pixel);
        target->fallback_texture = id;
    }

    glViewport(0, 0, target->width, target->height);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float projection[] = {
        2.0f / target->width, 0.0f, 0.0f, 0.0f,
        0.0f, -2.0f / target->height, 0.0f, 0.0f,
        0.0f, 0.0f, -2.0f, 0.0f,
        -1.0f, 1.0f, -1.0f, 1.0f
    };
    glUniformMatrix4fv(glGetUniformLocation(main_shader, "projection"), 1, GL_FALSE, projection);

    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);

   for (RenderBatch *batch = target->batches; batch != nullptr; batch = batch->next) {
       u32 texture = batch->texture;
       if (batch->texture == 0) {
           texture = target->fallback_texture;
       }
       glBindTexture(GL_TEXTURE_2D, texture);
       glBufferData(GL_ARRAY_BUFFER, batch->vertices.count * sizeof(Vertex), batch->vertices.data, GL_STREAM_DRAW);
       glDrawArrays(GL_TRIANGLES, 0, (int)batch->vertices.count);
    }
}
