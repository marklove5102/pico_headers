@vs vs
@glsl_options flip_vert_y

layout(location = 0) in vec3 a_pos;
layout(location = 2) in vec2 a_uv;

layout(binding = 0) uniform vs_block {
    mat4 u_mvp;
};

out vec2 uv;

void main() {
    gl_Position = u_mvp * vec4(a_pos, 1.0);
    uv = a_uv;
}

@end

@fs fs

layout (location = 0) in vec2 uv;

layout(binding = 1) uniform fs_block {
    vec3 u_color;
};

layout (binding = 0) uniform texture2D u_tex;
layout (binding = 1) uniform sampler   u_smp;

out vec4 frag_color;

void main() {
    float alpha = texture(sampler2D(u_tex, u_smp), uv).r;
    frag_color = vec4(u_color, alpha);
}

@end

@program text vs fs
