uniform sampler2D tex;
uniform mat4 projection;

#ifdef VERTEX_SHADER
layout (location = 0) in vec2 vertex;
layout (location = 1) in vec2 coords;
layout (location = 2) in vec3 color;
out vec2 uv;
out vec3 text_color;

void main() {
    gl_Position = projection * vec4(vertex, 0, 1);
    uv = coords;
    text_color = color;
}
#endif

#ifdef PIXEL_SHADER
in vec2 uv;
in vec3 text_color;
out vec4 frag_color;

void main() {
    float a = texture(tex, uv).r;
    frag_color = vec4(text_color, a);
}
#endif
