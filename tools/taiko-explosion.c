/*
 * "The quick and dirty TnT style hit explosion animation creator"
 * Copyright (c) 2025 McEndu.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Change to true if you want Vulkan/DX12/Metal to output useful info.
#define debug false

// Vertex format.
struct vertex {
  float x;
  float y;
  float z;
  float w;
  float u;
  float v;
  float reserved0;
  float reserved1;
};

// Per-instance data format.
struct instanceData {
  float colorGradientSpec[3][4];
  float alphaGradientSpec[3][4];
  float timeScale;
  float zOffset;
  float uOffset;
  float vOffset;
};

// Vertex-stage uniform format for explosion and text.
struct vertexUniforms {
  float projection[4][4];
  float transformation[4][4];
  float time;
  float reserved[3];
};

// GPU objects.
static SDL_GPUDevice *device;
static SDL_GPUTexture *atlasTexture;
static SDL_GPUTexture *resultTexture;
static SDL_GPUTexture *postprocTexture;
static SDL_GPUSampler *sampler;
static SDL_GPUGraphicsPipeline *explosionPipeline;
static SDL_GPUGraphicsPipeline *textPipeline;
static SDL_GPUGraphicsPipeline *demultiplyPipeline;
static SDL_GPUTransferBuffer *outputDownload;
static SDL_GPUBuffer *vertexBuffer;
static SDL_GPUBuffer *indexBuffer;
static SDL_GPUBuffer *instanceBuffer;

// Result surface.
static int outWidth;
static int outHeight;

// Programe parameters.
static const char *basePath;
static const char *inputPath;
static int result;
static const char *outputBaseName;

// Coordinates are stored in a simple, normalized manner. For rendering,
// the explosion animation takes up a 2.0 by 2.0 area; the rest is
// conventional 2d Cartesian stuff. For the animation atlas, (0, 0)
// refers to the top left and (1, 1) refers to the bottom right.
//
// Matrices are stored in column-major order, as passed down from the
// good old OpenGL, so everything here are transposed from the usual
// layout.

// To accomodate the text, the aspect ratio of a frame is 4:5. Most of
// the input assumes a square coordinate system, so this matrix
// compensates for that.
static const float aspectTransform[][4] = {
    {1.00f, 0.00f, 0.00f, 0.00f},
    {0.00f, 0.80f, 0.00f, 0.00f},
    {0.00f, 0.00f, 1.00f, 0.00f},
    {0.00f, 0.00f, 0.00f, 1.00f},
};

// The rest of the matrices here are 3x3, though the GPU dictates that
// a 3x3 matrix should leave space for a 4th row per the sacred Align
// To Sixteen Byte Boundaries rule.
//
// These matrices takes the normalized time and the value of the green
// channel, both in the range [0.0, 1.0), and maps the coordinates onto
// a gradient in the source texture.

static const float alphaGradient[][4] = {
    {0.125f, 0.000f, 0.000f, 0.000f},
    {0.000f, 0.125f, 0.000f, 0.000f},
    {0.000f, 0.250f, 1.000f, 0.000f},
};

static const float alphaGradientFireworks[][4] = {
    {0.125f, 0.000f, 0.000f, 0.000f},
    {0.000f, 0.125f, 0.000f, 0.000f},
    {0.375f, 0.250f, 1.000f, 0.000f},
};

static const float colorGradient300[][4] = {
    {0.125f, 0.000f, 0.000f, 0.000f},
    {0.000f, 0.125f, 0.000f, 0.000f},
    {0.250f, 0.250f, 1.000f, 0.000f},
};

static const float colorGradient100[][4] = {
    {0.125f, 0.000f, 0.000f, 0.000f},
    {0.000f, 0.125f, 0.000f, 0.000f},
    {0.125f, 0.250f, 1.000f, 0.000f},
};

static const float colorGradientFireworks[][4] = {
    {0.125f, 0.000f, 0.000f, 0.000f},
    {0.000f, 0.125f, 0.000f, 0.000f},
    {0.500f, 0.250f, 1.000f, 0.000f},
};

// This array tells the GPU how to draw a rectangle with two triangles,
// so that I don't need to write duplicated vertices.

static const uint16_t indices[] = {0, 2, 1, 1, 2, 3};

// Dimensions of the explosion textures.

static const SDL_FRect dimensions300 = {0.00f, 0.00f, 0.25f, 0.25f};
static const SDL_FRect dimensions300g = {0.25f, 0.00f, 0.25f, 0.25f};
static const SDL_FRect dimensions100 = {0.50f, 0.00f, 0.25f, 0.25f};
static const SDL_FRect dimensionsFireworks = {0.75f, 0.00f, 0.25f, 0.25f};

// Dimensions of the text textures.

static const SDL_FRect textureArea300 = {0x0p-4f, 0x6p-4f, 0x1p-4f, 0x1p-4f};
static const SDL_FRect textureArea100 = {0x1p-4f, 0x6p-4f, 0x1p-4f, 0x1p-4f};
static const SDL_FRect textureAreaMiss = {0x2p-4f, 0x6p-4f, 0x2p-4f, 0x1p-4f};

// Where the text would end up in the resulting frame.

static const SDL_FRect textRectHit = {-0.25f, 0.25f, 0.5f, -0.5f};
static const SDL_FRect textRectMiss = {-0.5f, 0.25f, 1.0f, -0.5f};

// All the aspects that are different for each animation.

static const struct resultData {
  int frames;
  bool explosion;
  bool fireworks;
  const SDL_FRect *explosionTextureArea;
  const SDL_FRect *textTextureArea;
  const SDL_FRect *textRect;
  const float (*colorGradient)[4];
} results[] = {
    {
        .frames = 18,
        .explosionTextureArea = &dimensions300,
        .colorGradient = colorGradient300,
        .textTextureArea = &textureArea300,
        .textRect = &textRectHit,
        .explosion = true,
        .fireworks = false,
    },
    {
        .frames = 18,
        .explosionTextureArea = &dimensions100,
        .colorGradient = colorGradient100,
        .textTextureArea = &textureArea100,
        .textRect = &textRectHit,
        .explosion = true,
        .fireworks = false,
    },
    {
        .frames = 18,
        .explosionTextureArea = &dimensions300g,
        .colorGradient = colorGradient300,
        .textTextureArea = &textureArea300,
        .textRect = &textRectHit,
        .explosion = true,
        .fireworks = true,
    },
    {
        .frames = 18,
        .explosionTextureArea = &dimensions300,
        .colorGradient = colorGradient100,
        .textTextureArea = &textureArea100,
        .textRect = &textRectHit,
        .explosion = true,
        .fireworks = false,
    },
    {
        .frames = 18,
        .explosionTextureArea = &dimensions300,
        .colorGradient = colorGradient300,
        .textTextureArea = &textureAreaMiss,
        .textRect = &textRectMiss,
        .explosion = false,
        .fireworks = false,
    },
};

// Options understood by the program.

static const char shortopts[] = "2ho:t:";

static const struct option longopts[] = {
    {"animation", required_argument, NULL, 't'},
    {"hd", no_argument, NULL, '2'},
    {"help", no_argument, NULL, 'h'},
    {"output", required_argument, NULL, 'o'},
    {"type", required_argument, NULL, 't'},
    {NULL, 0, NULL, 0},
};

/**
 * Load an SPIR-V shader from a file.
 */
static SDL_GPUShader *loadShader(SDL_GPUDevice *dev, const char *name,
                                 const SDL_GPUShaderCreateInfo *params) {
  // get path
  int pathSize = snprintf(NULL, 0, "%s/%s", basePath, name) + 1;
  char *path = SDL_malloc(pathSize);
  if (!path) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "cannot load shader '%s'", name);
    return NULL;
  }
  snprintf(path, pathSize, "%s/%s", basePath, name);

  // load code
  size_t codeSize;
  uint8_t *code = SDL_LoadFile(path, &codeSize);
  SDL_free(path);
  if (!code) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "cannot load shader '%s'", name);
    return NULL;
  }

  // initialize shader
  SDL_GPUShaderCreateInfo effectiveParams;
  memcpy(&effectiveParams, params, sizeof(SDL_GPUShaderCreateInfo));
  effectiveParams.code = code;
  effectiveParams.code_size = codeSize;

  SDL_GPUShader *shader = SDL_CreateGPUShader(dev, &effectiveParams);
  if (!shader) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "cannot load shader '%s'", name);
  }

  SDL_free(code);
  return shader;
}

/**
 * Initialize program state.
 */
static int init() {
  basePath = SDL_GetBasePath();

  // parameters
  const struct resultData *resultData = &results[result];
  const SDL_FRect *explosionTextureArea = resultData->explosionTextureArea;
  const SDL_FRect *textTextureArea = resultData->textTextureArea;
  const SDL_FRect *textRect = resultData->textRect;

  // device
  device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, debug, NULL);
  if (!device) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "no suitable rendering device found");
    return 1;
  }

  // image
  SDL_Surface *srcImage = IMG_Load(inputPath);
  if (!srcImage) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "cannot load image '%s'",
                 inputPath);
    return 1;
  }

  SDL_Surface *image = SDL_ConvertSurface(srcImage, SDL_PIXELFORMAT_ABGR8888);
  SDL_DestroySurface(srcImage);
  if (!image) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "cannot load image '%s'",
                 inputPath);
    return 1;
  }

  int width = image->w;
  int height = image->h;

  // transfer buffers
  SDL_GPUTransferBuffer *vertexUpload = SDL_CreateGPUTransferBuffer(
      device, &(SDL_GPUTransferBufferCreateInfo){
                  .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                  .size = 12 * sizeof(struct vertex),
                  .props = 0,
              });
  SDL_GPUTransferBuffer *indexUpload = SDL_CreateGPUTransferBuffer(
      device, &(SDL_GPUTransferBufferCreateInfo){
                  .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                  .size = 6 * sizeof(uint16_t),
                  .props = 0,
              });
  SDL_GPUTransferBuffer *instanceUpload = SDL_CreateGPUTransferBuffer(
      device, &(SDL_GPUTransferBufferCreateInfo){
                  .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                  .size = 2 * sizeof(struct instanceData),
                  .props = 0,
              });
  SDL_GPUTransferBuffer *textureUpload = SDL_CreateGPUTransferBuffer(
      device, &(SDL_GPUTransferBufferCreateInfo){
                  .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                  .size = image->pitch * height,
                  .props = 0,
              });

  // shaders
  SDL_GPUShader *explosionVertex =
      loadShader(device, "taiko-explosion.vert.spv",
                 &(SDL_GPUShaderCreateInfo){
                     .format = SDL_GPU_SHADERFORMAT_SPIRV,
                     .stage = SDL_GPU_SHADERSTAGE_VERTEX,
                     .entrypoint = "main",
                     .num_samplers = 0,
                     .num_storage_buffers = 0,
                     .num_storage_textures = 0,
                     .num_uniform_buffers = 1,
                     .props = 0,
                 });

  SDL_GPUShader *explosionFragment =
      loadShader(device, "taiko-explosion.frag.spv",
                 &(SDL_GPUShaderCreateInfo){
                     .format = SDL_GPU_SHADERFORMAT_SPIRV,
                     .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
                     .entrypoint = "main",
                     .num_samplers = 1,
                     .num_storage_buffers = 0,
                     .num_storage_textures = 0,
                     .num_uniform_buffers = 0,
                     .props = 0,
                 });

  SDL_GPUShader *textVertex =
      loadShader(device, "textured.vert.spv",
                 &(SDL_GPUShaderCreateInfo){
                     .format = SDL_GPU_SHADERFORMAT_SPIRV,
                     .stage = SDL_GPU_SHADERSTAGE_VERTEX,
                     .entrypoint = "main",
                     .num_samplers = 0,
                     .num_storage_buffers = 0,
                     .num_storage_textures = 0,
                     .num_uniform_buffers = 1,
                     .props = 0,
                 });

  SDL_GPUShader *textFragment =
      loadShader(device, "textured.frag.spv",
                 &(SDL_GPUShaderCreateInfo){
                     .format = SDL_GPU_SHADERFORMAT_SPIRV,
                     .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
                     .entrypoint = "main",
                     .num_samplers = 1,
                     .num_storage_buffers = 0,
                     .num_storage_textures = 0,
                     .num_uniform_buffers = 1,
                     .props = 0,
                 });

  SDL_GPUShader *demultiplyVertex =
      loadShader(device, "demultiply.vert.spv",
                 &(SDL_GPUShaderCreateInfo){
                     .format = SDL_GPU_SHADERFORMAT_SPIRV,
                     .stage = SDL_GPU_SHADERSTAGE_VERTEX,
                     .entrypoint = "main",
                     .num_samplers = 0,
                     .num_storage_buffers = 0,
                     .num_storage_textures = 0,
                     .num_uniform_buffers = 0,
                     .props = 0,
                 });

  SDL_GPUShader *demultiplyFragment =
      loadShader(device, "demultiply.frag.spv",
                 &(SDL_GPUShaderCreateInfo){
                     .format = SDL_GPU_SHADERFORMAT_SPIRV,
                     .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
                     .entrypoint = "main",
                     .num_samplers = 1,
                     .num_storage_buffers = 0,
                     .num_storage_textures = 0,
                     .num_uniform_buffers = 0,
                     .props = 0,
                 });

  if (!explosionVertex || !explosionFragment || !textVertex || !textFragment ||
      !demultiplyVertex || !demultiplyFragment) {
    goto fail;
  } else if (!vertexUpload || !indexUpload || !instanceUpload ||
             !textureUpload) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "cannot create transfer buffers");
    goto fail;
  }

  // pipelines
  SDL_GPUGraphicsPipelineCreateInfo pipelineParams = {
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
      .rasterizer_state = {0},
      .multisample_state = {0},
      .depth_stencil_state = {0},
      .target_info =
          {
              .color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
                  .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                  .blend_state =
                      {
                          .src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
                          .dst_color_blendfactor =
                              SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                          .color_blend_op = SDL_GPU_BLENDOP_ADD,
                          .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
                          .dst_alpha_blendfactor =
                              SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                          .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                          .enable_blend = true,
                          .enable_color_write_mask = false,
                      },
              }},
              .num_color_targets = 1,
              .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_INVALID,
              .has_depth_stencil_target = false,
          },
      .props = 0,
  };

  pipelineParams.vertex_shader = explosionVertex;
  pipelineParams.fragment_shader = explosionFragment;
  pipelineParams.vertex_input_state =
      (SDL_GPUVertexInputState){
          .vertex_buffer_descriptions =
              (SDL_GPUVertexBufferDescription[]){
                  {
                      .slot = 0,
                      .pitch = sizeof(struct vertex),
                      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                      .instance_step_rate = 0,
                  },
                  {
                      .slot = 1,
                      .pitch = sizeof(struct instanceData),
                      .input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE,
                      .instance_step_rate = 0,
                  },
              },
          .num_vertex_buffers = 2,
          .vertex_attributes =
              (SDL_GPUVertexAttribute[]){
                  {
                      .location = 0,
                      .buffer_slot = 0,
                      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                      .offset = offsetof(struct vertex, x),
                  },
                  {
                      .location = 1,
                      .buffer_slot = 0,
                      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                      .offset = offsetof(struct vertex, u),
                  },
                  {
                      .location = 2,
                      .buffer_slot = 1,
                      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                      .offset =
                          offsetof(struct instanceData, colorGradientSpec[0]),
                  },
                  {
                      .location = 3,
                      .buffer_slot = 1,
                      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                      .offset =
                          offsetof(struct instanceData, colorGradientSpec[1]),
                  },
                  {
                      .location = 4,
                      .buffer_slot = 1,
                      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                      .offset =
                          offsetof(struct instanceData, colorGradientSpec[2]),
                  },
                  {
                      .location = 5,
                      .buffer_slot = 1,
                      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                      .offset =
                          offsetof(struct instanceData, alphaGradientSpec[0]),
                  },
                  {
                      .location = 6,
                      .buffer_slot = 1,
                      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                      .offset =
                          offsetof(struct instanceData, alphaGradientSpec[1]),
                  },
                  {
                      .location = 7,
                      .buffer_slot = 1,
                      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                      .offset =
                          offsetof(struct instanceData, alphaGradientSpec[2]),
                  },
                  {
                      .location = 8,
                      .buffer_slot = 1,
                      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT,
                      .offset = offsetof(struct instanceData, timeScale),
                  },
                  {
                      .location = 9,
                      .buffer_slot = 1,
                      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT,
                      .offset = offsetof(struct instanceData, zOffset),
                  },
                  {
                      .location = 10,
                      .buffer_slot = 1,
                      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                      .offset = offsetof(struct instanceData, uOffset),
                  },
              },
          .num_vertex_attributes = 11,
      },
  explosionPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineParams);

  pipelineParams.vertex_input_state =
      (SDL_GPUVertexInputState){
          .vertex_buffer_descriptions =
              (SDL_GPUVertexBufferDescription[]){
                  {
                      .slot = 0,
                      .pitch = sizeof(struct vertex),
                      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                      .instance_step_rate = 0,
                  },
              },
          .num_vertex_buffers = 1,
          .vertex_attributes =
              (SDL_GPUVertexAttribute[]){
                  {
                      .location = 0,
                      .buffer_slot = 0,
                      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                      .offset = offsetof(struct vertex, x),
                  },
                  {
                      .location = 1,
                      .buffer_slot = 0,
                      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                      .offset = offsetof(struct vertex, u),
                  },
              },
          .num_vertex_attributes = 2,
      },

  pipelineParams.vertex_shader = textVertex;
  pipelineParams.fragment_shader = textFragment;
  textPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineParams);

  pipelineParams.vertex_shader = demultiplyVertex;
  pipelineParams.fragment_shader = demultiplyFragment;
  demultiplyPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineParams);

  if (!explosionPipeline || !textPipeline) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "cannot create pipelines");
    goto fail;
  }

  // sampler
  sampler = SDL_CreateGPUSampler(
      device, &(SDL_GPUSamplerCreateInfo){
                  .min_filter = SDL_GPU_FILTER_NEAREST,
                  .mag_filter = SDL_GPU_FILTER_NEAREST,
                  .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
                  .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT,
                  .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT,
                  .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
                  .mip_lod_bias = 0.0f,
                  .max_anisotropy = 1.0f,
                  .compare_op = SDL_GPU_COMPAREOP_INVALID,
                  .min_lod = 0.0f,
                  .max_lod = 0.0f,
                  .enable_anisotropy = false,
                  .enable_compare = false,
                  .props = 0,
              });
  if (!sampler) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "cannot create sampler");
    goto fail;
  }

  // animation atlas
  atlasTexture = SDL_CreateGPUTexture(
      device, &(SDL_GPUTextureCreateInfo){
                  .type = SDL_GPU_TEXTURETYPE_2D,
                  .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                  .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
                  .width = width,
                  .height = height,
                  .layer_count_or_depth = 1,
                  .num_levels = 1,
                  .sample_count = 0,
                  .props = 0,
              });

  if (!atlasTexture || !textureUpload) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "cannot set up textures");
    goto fail;
  }

  {
    void *buf = SDL_MapGPUTransferBuffer(device, textureUpload, false);
    memcpy(buf, image->pixels, image->pitch * height);
    SDL_UnmapGPUTransferBuffer(device, textureUpload);
  }

  // output data
  outWidth = explosionTextureArea->w * width;
  outHeight = explosionTextureArea->h * height * 1.25;

  resultTexture = SDL_CreateGPUTexture(
      device, &(SDL_GPUTextureCreateInfo){
                  .type = SDL_GPU_TEXTURETYPE_2D,
                  .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                           SDL_GPU_TEXTUREUSAGE_SAMPLER,
                  .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                  .width = outWidth,
                  .height = outHeight,
                  .layer_count_or_depth = 1,
                  .num_levels = 1,
                  .sample_count = 0,
                  .props = 0,
              });
  postprocTexture = SDL_CreateGPUTexture(
      device, &(SDL_GPUTextureCreateInfo){
                  .type = SDL_GPU_TEXTURETYPE_2D,
                  .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
                  .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                  .width = outWidth,
                  .height = outHeight,
                  .layer_count_or_depth = 1,
                  .num_levels = 1,
                  .sample_count = 0,
                  .props = 0,
              });
  outputDownload = SDL_CreateGPUTransferBuffer(
      device, &(SDL_GPUTransferBufferCreateInfo){
                  .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                  .size = outWidth * outHeight * 4,
                  .props = 0,
              });

  if (!resultTexture || !postprocTexture || !outputDownload) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "cannot set up textures");
    goto fail;
  }

  // vertex buffers
  vertexBuffer =
      SDL_CreateGPUBuffer(device, &(SDL_GPUBufferCreateInfo){
                                      .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
                                      .size = 12 * sizeof(struct vertex),
                                      .props = 0,
                                  });
  indexBuffer =
      SDL_CreateGPUBuffer(device, &(SDL_GPUBufferCreateInfo){
                                      .usage = SDL_GPU_BUFFERUSAGE_INDEX,
                                      .size = 6 * sizeof(uint16_t),
                                      .props = 0,
                                  });
  instanceBuffer =
      SDL_CreateGPUBuffer(device, &(SDL_GPUBufferCreateInfo){
                                      .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
                                      .size = 2 * sizeof(struct instanceData),
                                      .props = 0,
                                  });

  if (!vertexBuffer || !indexBuffer || !instanceBuffer) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "cannot create vertex buffers");
    goto fail;
  }

  {
    struct vertex *vertices =
        SDL_MapGPUTransferBuffer(device, vertexUpload, false);

    // explosion
    vertices[0] = (struct vertex){-1.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0};
    vertices[1] = (struct vertex){1.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0};
    vertices[2] = (struct vertex){-1.0, -1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0};
    vertices[3] = (struct vertex){1.0, -1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0};

    vertices[0].u = vertices[1].u = explosionTextureArea->x;
    vertices[2].u = vertices[3].u =
        explosionTextureArea->x + explosionTextureArea->w;
    vertices[0].v = vertices[2].v = explosionTextureArea->y;
    vertices[1].v = vertices[3].v =
        explosionTextureArea->y + explosionTextureArea->h;

    // text
    vertices[4] = (struct vertex){0.0, 0.0, 0x2p-50, 1.0, 0.0, 0.0, 0.0, 0.0};
    vertices[5] = (struct vertex){0.0, 0.0, 0x2p-50, 1.0, 0.0, 0.0, 0.0, 0.0};
    vertices[6] = (struct vertex){0.0, 0.0, 0x2p-50, 1.0, 0.0, 0.0, 0.0, 0.0};
    vertices[7] = (struct vertex){0.0, 0.0, 0x2p-50, 1.0, 0.0, 0.0, 0.0, 0.0};

    vertices[4].x = vertices[6].x = textRect->x;
    vertices[5].x = vertices[7].x = textRect->x + textRect->w;
    vertices[4].y = vertices[5].y = textRect->y;
    vertices[6].y = vertices[7].y = textRect->y + textRect->h;
    vertices[4].u = vertices[6].u = textTextureArea->x;
    vertices[5].u = vertices[7].u = textTextureArea->x + textTextureArea->w;
    vertices[4].v = vertices[5].v = textTextureArea->y;
    vertices[6].v = vertices[7].v = textTextureArea->y + textTextureArea->h;

    // post-processing
    vertices[8] = (struct vertex){-1.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0};
    vertices[9] = (struct vertex){1.0, 1.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0};
    vertices[10] = (struct vertex){-1.0, -1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0};
    vertices[11] = (struct vertex){1.0, -1.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0};

    SDL_UnmapGPUTransferBuffer(device, vertexUpload);
  }

  {
    uint16_t *buf = SDL_MapGPUTransferBuffer(device, indexUpload, false);
    memcpy(buf, indices, sizeof(indices));
    SDL_UnmapGPUTransferBuffer(device, indexUpload);
  }

  {
    struct instanceData *instances =
        SDL_MapGPUTransferBuffer(device, instanceUpload, false);

    // explosion
    const float (*colorGradient)[4] = results[result].colorGradient;
    memcpy(instances[0].colorGradientSpec, colorGradient, sizeof(float[3][4]));
    memcpy(instances[0].alphaGradientSpec, alphaGradient, sizeof(float[3][4]));
    instances[0].timeScale = 1.0 / 9.0;
    instances[0].zOffset = 0x0p-50f;
    instances[0].uOffset = 0.0;
    instances[0].vOffset = 0.0;

    // fireworks
    memcpy(instances[1].colorGradientSpec, colorGradientFireworks,
           sizeof(float[3][4]));
    memcpy(instances[1].alphaGradientSpec, alphaGradientFireworks,
           sizeof(float[3][4]));
    instances[1].timeScale = 1.0 / 24.0;
    instances[1].zOffset = 0x1p-50f;
    instances[1].uOffset = dimensionsFireworks.x - explosionTextureArea->x;
    instances[1].vOffset = dimensionsFireworks.y - explosionTextureArea->y;

    SDL_UnmapGPUTransferBuffer(device, instanceUpload);
  }

  // upload
  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);

  SDL_UploadToGPUTexture(copy,
                         &(SDL_GPUTextureTransferInfo){
                             .transfer_buffer = textureUpload,
                             .offset = 0,
                             .pixels_per_row = width,
                             .rows_per_layer = height,
                         },
                         &(SDL_GPUTextureRegion){
                             .texture = atlasTexture,
                             .layer = 0,
                             .x = 0,
                             .y = 0,
                             .z = 0,
                             .w = width,
                             .h = height,
                             .d = 1,
                         },
                         false);
  SDL_UploadToGPUBuffer(copy,
                        &(SDL_GPUTransferBufferLocation){
                            .transfer_buffer = vertexUpload, .offset = 0},
                        &(SDL_GPUBufferRegion){
                            .buffer = vertexBuffer,
                            .offset = 0,
                            .size = 12 * sizeof(struct vertex),
                        },
                        false);
  SDL_UploadToGPUBuffer(copy,
                        &(SDL_GPUTransferBufferLocation){
                            .transfer_buffer = indexUpload, .offset = 0},
                        &(SDL_GPUBufferRegion){
                            .buffer = indexBuffer,
                            .offset = 0,
                            .size = 6 * sizeof(uint16_t),
                        },
                        false);
  SDL_UploadToGPUBuffer(copy,
                        &(SDL_GPUTransferBufferLocation){
                            .transfer_buffer = instanceUpload, .offset = 0},
                        &(SDL_GPUBufferRegion){
                            .buffer = instanceBuffer,
                            .offset = 0,
                            .size = 2 * sizeof(struct instanceData),
                        },
                        false);

  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(cmd);

  SDL_DestroySurface(image);
  SDL_ReleaseGPUShader(device, explosionVertex);
  SDL_ReleaseGPUShader(device, explosionFragment);
  SDL_ReleaseGPUShader(device, textVertex);
  SDL_ReleaseGPUShader(device, textFragment);
  SDL_ReleaseGPUShader(device, demultiplyVertex);
  SDL_ReleaseGPUShader(device, demultiplyFragment);
  SDL_ReleaseGPUTransferBuffer(device, textureUpload);
  SDL_ReleaseGPUTransferBuffer(device, vertexUpload);
  SDL_ReleaseGPUTransferBuffer(device, indexUpload);
  SDL_ReleaseGPUTransferBuffer(device, instanceUpload);

  return 0;

fail:
  SDL_DestroySurface(image);
  SDL_ReleaseGPUShader(device, explosionVertex);
  SDL_ReleaseGPUShader(device, explosionFragment);
  SDL_ReleaseGPUShader(device, textVertex);
  SDL_ReleaseGPUShader(device, textFragment);
  SDL_ReleaseGPUShader(device, demultiplyVertex);
  SDL_ReleaseGPUShader(device, demultiplyFragment);
  SDL_ReleaseGPUTransferBuffer(device, textureUpload);
  SDL_ReleaseGPUTransferBuffer(device, vertexUpload);
  SDL_ReleaseGPUTransferBuffer(device, indexUpload);
  SDL_ReleaseGPUTransferBuffer(device, instanceUpload);
  return 1;
}

/**
 * Render an explosion animation.
 */
static void renderExplosion(SDL_GPUCommandBuffer *cmd,
                            SDL_GPURenderPass *render, double time) {
  if (!(results[result].explosion))
    return;

  struct vertexUniforms vertexUniforms = {
      .transformation =
          {
              {1.0f, 0.0f, 0.0f, 0.0f},
              {0.0f, 1.0f, 0.0f, 0.0f},
              {0.0f, 0.0f, 1.0f, 0.0f},
              {0.0f, 0.0f, 0.0f, 1.0f},
          },
      .time = time,
  };
  memcpy(&vertexUniforms.projection, aspectTransform, sizeof(aspectTransform));

  SDL_BindGPUGraphicsPipeline(render, explosionPipeline);
  SDL_BindGPUVertexBuffers(render, 0,
                           (SDL_GPUBufferBinding[]){
                               {.buffer = vertexBuffer, .offset = 0},
                               {.buffer = instanceBuffer, .offset = 0},
                           },
                           2);
  SDL_BindGPUIndexBuffer(
      render, &(SDL_GPUBufferBinding){.buffer = indexBuffer, .offset = 0},
      SDL_GPU_INDEXELEMENTSIZE_16BIT);
  SDL_PushGPUVertexUniformData(cmd, 0, &vertexUniforms, sizeof(vertexUniforms));
  SDL_BindGPUFragmentSamplers(
      render, 0,
      (SDL_GPUTextureSamplerBinding[]){
          {.sampler = sampler, .texture = atlasTexture}},
      1);

  if (results[result].fireworks) {
    // Fireworks are drawn for the 300k animation, as a separate instance
    // of the similar main explosion animation.
    SDL_DrawGPUIndexedPrimitives(render, 6, 2, 0, 0, 0);
  } else {
    SDL_DrawGPUIndexedPrimitives(render, 6, 1, 0, 0, 0);
  }
}

/**
 * Easing for the text animation.
 */
static double easeOutElastic(double x) {
  if (x < 0.0)
    return 0.0;

  double c4 = (2.0 * 3.1415926535897932384626433) / 3.0;
  return pow(2, -8 * x) * sin((x * 4.0 - 0.75) * c4) + 1;
}

/**
 * Render a text label denoting whether and how accurate the note is hit.
 */
static void renderText(SDL_GPUCommandBuffer *cmd, SDL_GPURenderPass *render,
                       double time) {
  // animate
  float alpha = 1.0;

  double normalizedTime = time / 16.0;
  double from = -(1.0 / 3.0);
  double to = -0.65625;
  double y = from + easeOutElastic(normalizedTime) * (to - from);

  // render
  struct vertexUniforms vertexUniforms = {
      .transformation =
          {
              {1.0f, 0.0f, 0.0f, 0.0f},
              {0.0f, 1.0f, 0.0f, 0.0f},
              {0.0f, 0.0f, 1.0f, 0.0f},
              {0.0f, y, 0.0f, 1.0f},
          },
  };
  memcpy(&vertexUniforms.projection, aspectTransform, sizeof(aspectTransform));

  SDL_BindGPUGraphicsPipeline(render, textPipeline);
  SDL_BindGPUVertexBuffers(
      render, 0,
      (SDL_GPUBufferBinding[]){
          {.buffer = vertexBuffer, .offset = 4 * sizeof(struct vertex)}},
      1);
  SDL_BindGPUIndexBuffer(
      render, &(SDL_GPUBufferBinding){.buffer = indexBuffer, .offset = 0},
      SDL_GPU_INDEXELEMENTSIZE_16BIT);
  SDL_PushGPUVertexUniformData(cmd, 0, &vertexUniforms, sizeof(vertexUniforms));
  SDL_PushGPUFragmentUniformData(cmd, 0, &alpha, sizeof(float));
  SDL_BindGPUFragmentSamplers(
      render, 0,
      (SDL_GPUTextureSamplerBinding[]){
          {.sampler = sampler, .texture = atlasTexture}},
      1);
  SDL_DrawGPUIndexedPrimitives(render, 6, 1, 0, 0, 0);
}

/**
 * Postprocess the explosion animation for use with osu!.
 */
static void postProcess(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *dst,
                        SDL_GPUTexture *src) {
  SDL_GPURenderPass *render =
      SDL_BeginGPURenderPass(cmd,
                             &(SDL_GPUColorTargetInfo){
                                 .texture = dst,
                                 .mip_level = 0,
                                 .layer_or_depth_plane = 0,
                                 .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},
                                 .load_op = SDL_GPU_LOADOP_CLEAR,
                                 .store_op = SDL_GPU_STOREOP_STORE,
                                 .resolve_texture = NULL,
                                 .resolve_mip_level = 0,
                                 .resolve_layer = 0,
                                 .cycle = false,
                                 .cycle_resolve_texture = false,
                             },
                             1, NULL);

  SDL_BindGPUGraphicsPipeline(render, demultiplyPipeline);
  SDL_BindGPUVertexBuffers(
      render, 0,
      (SDL_GPUBufferBinding[]){
          {.buffer = vertexBuffer, .offset = 8 * sizeof(struct vertex)}},
      1);
  SDL_BindGPUIndexBuffer(
      render, &(SDL_GPUBufferBinding){.buffer = indexBuffer, .offset = 0},
      SDL_GPU_INDEXELEMENTSIZE_16BIT);
  SDL_BindGPUFragmentSamplers(
      render, 0,
      (SDL_GPUTextureSamplerBinding[]){{.sampler = sampler, .texture = src}},
      1);
  SDL_DrawGPUIndexedPrimitives(render, 6, 1, 0, 0, 0);

  SDL_EndGPURenderPass(render);
}

/**
 * Draw an animation frame.
 */
static int process(double time) {
  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);

  // draw
  SDL_GPURenderPass *render =
      SDL_BeginGPURenderPass(commands,
                             &(SDL_GPUColorTargetInfo){
                                 .texture = resultTexture,
                                 .mip_level = 0,
                                 .layer_or_depth_plane = 0,
                                 .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},
                                 .load_op = SDL_GPU_LOADOP_CLEAR,
                                 .store_op = SDL_GPU_STOREOP_STORE,
                                 .resolve_texture = NULL,
                                 .resolve_mip_level = 0,
                                 .resolve_layer = 0,
                                 .cycle = false,
                                 .cycle_resolve_texture = false,
                             },
                             1, NULL);
  renderExplosion(commands, render, time);
  renderText(commands, render, time);
  SDL_EndGPURenderPass(render);

  // post process
  postProcess(commands, postprocTexture, resultTexture);

  // download to CPU
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  SDL_DownloadFromGPUTexture(copy,
                             &(SDL_GPUTextureRegion){
                                 .texture = postprocTexture,
                                 .mip_level = 0,
                                 .layer = 0,
                                 .x = 0,
                                 .y = 0,
                                 .z = 0,
                                 .w = outWidth,
                                 .h = outHeight,
                                 .d = 1,
                             },
                             &(SDL_GPUTextureTransferInfo){
                                 .transfer_buffer = outputDownload,
                                 .offset = 0,
                                 .pixels_per_row = outWidth,
                                 .rows_per_layer = outHeight,
                             });
  SDL_EndGPUCopyPass(copy);

  // submit commands then wait for data to be ready
  SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
  SDL_WaitForGPUFences(device, false, (SDL_GPUFence *[]){fence}, 1);
  SDL_ReleaseGPUFence(device, fence);
  return 0;
}

/**
 * Save outputDownload to the specified filename.
 */
static void saveFrame(const char *path) {
  void *pixels = SDL_MapGPUTransferBuffer(device, outputDownload, false);
  SDL_Surface *surface = SDL_CreateSurfaceFrom(
      outWidth, outHeight, SDL_PIXELFORMAT_ABGR8888, pixels, outWidth * 4);
  IMG_SavePNG(surface, path);
  SDL_DestroySurface(surface);
  SDL_UnmapGPUTransferBuffer(device, outputDownload);
}

/**
 * Destroy all program state on the heap.
 */
static void cleanup() {
  SDL_ReleaseGPUTransferBuffer(device, outputDownload);
  SDL_ReleaseGPUTexture(device, atlasTexture);
  SDL_ReleaseGPUTexture(device, resultTexture);
  SDL_ReleaseGPUTexture(device, postprocTexture);
  SDL_ReleaseGPUSampler(device, sampler);
  SDL_ReleaseGPUGraphicsPipeline(device, explosionPipeline);
  SDL_ReleaseGPUGraphicsPipeline(device, textPipeline);
  SDL_ReleaseGPUGraphicsPipeline(device, demultiplyPipeline);
  SDL_ReleaseGPUBuffer(device, vertexBuffer);
  SDL_ReleaseGPUBuffer(device, indexBuffer);
  SDL_ReleaseGPUBuffer(device, instanceBuffer);
  SDL_DestroyGPUDevice(device);
}

const char helpString[] =
    "usage: %s [-2h] [-t ANIMATION] -o BASENAME TEXTURE\n"
    "Generate gradient-based hit animations for osu!taiko.\n"
    "\n"
    "  -2, --hd               append @2x.png instead of .png to the\n"
    "                         generated frames\n"
    "  -h, --help             display this help\n"
    "  -o, --output BASENAME  specify the base filename for the generated\n"
    "                         frames\n"
    "  -t, --type ANIMATION   specify the animation to generate\n"
    "  --animation ANIMATION  alias for '--type'\n"
    "\n"
    "Available animations:\n"
    "  1. perfect hit (a.k.a. 300)\n"
    "  2. imperfect hit (a.k.a. 100)\n"
    "  3. perfect strong hit (a.k.a. 300k)\n"
    "  4. imperfect strong hit (a.k.a. 100k)\n"
    "  5. miss\n";

int main(int argc, char **argv) {
  SDL_Init(SDL_INIT_VIDEO);

  result = 0;
  outputBaseName = NULL;

  const char *format = "%s-%d.png";

  // read cmdline args
  int opt;
  while ((opt = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
    switch (opt) {
    case 'h': {
      fprintf(stdout, helpString, argv[0]);
      return EXIT_SUCCESS;
    }
    case 't': {
      char *end;
      result = strtol(optarg, &end, 0);
      if (*end != '\0' || result < 0 || result >= 5) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid explosion type: %s",
                     optarg);
        exit(EXIT_FAILURE);
      }
      break;
    }
    case 'o': {
      outputBaseName = optarg;
      break;
    }
    case '2': {
      format = "%s-%d@2x.png";
      break;
    }
    }
  }

  if (argc - optind < 1) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "no input texture");
    return EXIT_FAILURE;
  }
  inputPath = argv[optind];

  if (outputBaseName == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "no output template (specify with -o)");
    return EXIT_FAILURE;
  }

  // process
  if (init() != 0) {
    cleanup();
    return EXIT_FAILURE;
  }

  for (int i = 0; i < results[result].frames; ++i) {
    process(i);

    int outputNameSize = snprintf(NULL, 0, format, outputBaseName, i) + 1;
    char *outputName = SDL_malloc(outputNameSize);
    snprintf(outputName, outputNameSize, format, outputBaseName, i);
    saveFrame(outputName);
    SDL_free(outputName);
  }

  cleanup();
  return EXIT_SUCCESS;
}
