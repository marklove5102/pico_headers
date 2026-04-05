/*
    Text Example

    Renders text to the screen using a TrueType font.

    Demonstrates:
     * Setting up an SDL window and GL context
     * Initializing pico_gfx and pico_font
     * Loading a TrueType font
     * Creating a font atlas texture
     * Generating text geometry via pf_draw_text
     * Drawing text with alpha blending
*/

#include <SDL.h>

#include <assert.h>
#include <stdio.h>

#define PICO_FONT_IMPLEMENTATION
#include "../pico_font.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define PICO_GFX_GL
#define PICO_GFX_IMPLEMENTATION
#include "../pico_gfx.h"

#define SOKOL_SHDC_IMPL
#include "text_shader.h"

typedef struct
{
    float pos[2];
    float uv[2];
} vertex_t;

typedef float mat4_t[16];
typedef float rgb_t[3];

#define MAX_VERTICES 4096

typedef struct
{
    vertex_t vertices[MAX_VERTICES];
    int count;
} text_batch_t;

unsigned char* read_file(const char* filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char *data = malloc(fsize + 1);

    if (data)
    {
        fread(data, fsize, 1, f);
        data[fsize] = 0; // Null-terminate
    }

    fclose(f);

    return data;
}

// Callback invoked per glyph quad by pf_draw_text
static int draw_callback(const pf_quad_t* quad, void* user)
{
    text_batch_t* batch = (text_batch_t*)user;
    if (batch->count + 6 > MAX_VERTICES) return 0;

    vertex_t* v = &batch->vertices[batch->count];

    // Triangle 1
    v[0] = (vertex_t){ {quad->x0, quad->y0}, {quad->u0, quad->v0} };
    v[1] = (vertex_t){ {quad->x0, quad->y1}, {quad->u0, quad->v1} };
    v[2] = (vertex_t){ {quad->x1, quad->y1}, {quad->u1, quad->v1} };

    // Triangle 2
    v[3] = (vertex_t){ {quad->x0, quad->y0}, {quad->u0, quad->v0} };
    v[4] = (vertex_t){ {quad->x1, quad->y1}, {quad->u1, quad->v1} };
    v[5] = (vertex_t){ {quad->x1, quad->y0}, {quad->u1, quad->v0} };

    batch->count += 6;
    return 1;
}

typedef struct
{
    pg_ctx_t* ctx;
    pg_texture_t* tex;
} upload_ctx_t;

// Callback invoked per dirty atlas page by pf_upload_atlas
static int upload_callback(size_t page, const unsigned char* pixels,
                           int width, int height, void* user)
{
    (void)page;
    upload_ctx_t* uc = (upload_ctx_t*)user;

    if (!uc->tex)
    {
        uc->tex = pg_create_texture(uc->ctx, width, height,
                                    PG_PIXEL_FORMAT_RED, pixels,
                                    (size_t)(width * height), NULL);
    }
    else
    {
        pg_update_texture(uc->tex, (char*)pixels, width, height);
    }

    return 1;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    assert(pg_backend() == PG_BACKEND_GL);

    printf("Text rendering demo\n");

    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,     8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,   8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,    8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,   8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, true);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
    SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow("Text Example",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          1024, 768,
                                          SDL_WINDOW_OPENGL);

    int pixel_w, pixel_h;
    SDL_GL_GetDrawableSize(window, &pixel_w, &pixel_h);

    SDL_GL_SetSwapInterval(1);
    SDL_GLContext context = SDL_GL_CreateContext(window);

    pg_init();

    // Initialize context
    pg_ctx_t* ctx = pg_create_context(pixel_w, pixel_h, NULL);
    pg_shader_t* shader = pg_create_shader(ctx, text);

    // Load TTF font
    unsigned char* ttf = read_file("devroye.ttf");
    assert(ttf);

    // Create font atlas and face
    pf_atlas_t* atlas = pf_create_atlas(512, 512);
    pf_face_t* face = pf_create_face(atlas, ttf, 48.0f);

    // Generate text geometry using pico_font
    text_batch_t batch = { .count = 0 };


    #define LINE1 "Hello, World!"
    #define LINE2 "Pico Font + Pico GFX"

    const char* lines[] = { LINE1, LINE2 };
    int num_lines = 2;

    // Measure total height using the full multiline string
    float total_h;
    pf_measure_text(face, LINE1"\n"LINE2, NULL, &total_h);

    pf_font_metrics_t metrics = { 0 };
    pf_get_font_metrics(face, &metrics);

    float line_height = metrics.line_height;
    float cur_y = pixel_h / 2.f - total_h / 2.f;

    // Draw each line individually, centered horizontally
    for (int i = 0; i < num_lines; i++)
    {
        float line_w;
        pf_measure_text(face, lines[i], &line_w, NULL);

        float lx = pixel_w / 2.f - line_w / 2.f;
        float ly = cur_y;
        pf_draw_text(face, lines[i], &lx, &ly, draw_callback, &batch);

        cur_y += line_height;
    }

    // Upload font atlas to GPU texture
    upload_ctx_t upload = { .ctx = ctx, .tex = NULL };
    pf_upload_atlas(atlas, upload_callback, &upload);
    assert(upload.tex);

    // Create vertex buffer from text geometry
    pg_buffer_t* vertex_buffer = pg_create_vertex_buffer(ctx,
        PG_USAGE_STATIC, batch.vertices, batch.count, batch.count,
        sizeof(vertex_t));

    // Pipeline with alpha blending for text rendering
    pg_pipeline_t* pipeline = pg_create_pipeline(ctx, shader, &(pg_pipeline_opts_t)
    {
        .layout =
        {
            .attrs =
            {
                [ATTR_text_a_pos] = { .format = PG_VERTEX_FORMAT_FLOAT2,
                                      .offset = offsetof(vertex_t, pos) },

                [ATTR_text_a_uv]  = { .format = PG_VERTEX_FORMAT_FLOAT2,
                                      .offset = offsetof(vertex_t, uv) },
            },
        },

        .blend_enabled = true,
        .blend =
        {
            .color_src = PG_SRC_ALPHA,
            .color_dst = PG_ONE_MINUS_SRC_ALPHA,
            .color_eq  = PG_ADD,
            .alpha_src = PG_ONE,
            .alpha_dst = PG_ONE_MINUS_SRC_ALPHA,
            .alpha_eq  = PG_ADD,
        },
    });

    // Orthographic projection mapping pixel coordinates to clip space
    float w = (float)pixel_w;
    float h = (float)pixel_h;

    vs_block_t vs_block =
    {
        .u_mvp =
        {
            2.0f/w, 0.0f,   0.0f, 0.0f,
            0.0f,   2.0f/h, 0.0f, 0.0f,
            0.0f,   0.0f,  -1.0f, 0.0f,
           -1.0f,  -1.0f,   0.0f, 1.0f,
        }
    };

    fs_block_t fs_block =
    {
        .u_color = { 1.f, 1.f, 1.f }
    };

    // Set uniform blocks
    pg_set_uniform_block(shader, "vs_block", &vs_block);
    pg_set_uniform_block(shader, "fs_block", &fs_block);

    // Create sampler and bind texture
    pg_sampler_t* sampler = pg_create_sampler(ctx, NULL);
    pg_bind_sampler(shader, "u_smp", sampler);
    pg_bind_texture(shader, "u_tex", upload.tex);

    // Set the pipeline
    pg_set_pipeline(ctx, pipeline);

    // The main loop
    bool done = false;

    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_QUIT:
                    done = true;
                    break;

                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym)
                    {
                        case SDLK_ESCAPE:
                            done = true;
                            break;
                    }
                    break;
            }
        }

        // Draw text to the screen
        pg_begin_pass(ctx, NULL, true);

        pg_bind_buffer(ctx, 0, vertex_buffer);
        pg_draw(ctx, 0, batch.count, 1);

        pg_end_pass(ctx);

        // Flush draw commands
        pg_flush(ctx);

        // Swap buffers
        SDL_GL_SwapWindow(window);
    }

    pg_destroy_texture(upload.tex);
    pg_destroy_sampler(sampler);

    pg_destroy_buffer(vertex_buffer);

    pg_destroy_pipeline(pipeline);
    pg_destroy_shader(shader);
    pg_destroy_context(ctx);

    pg_shutdown();

    pf_destroy_face(face);
    pf_destroy_atlas(atlas);
    free(ttf);

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}

