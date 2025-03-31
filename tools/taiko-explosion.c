/*
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
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>
#include <assert.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SDL_GPUDevice *device;
SDL_GPUTexture *compositeExplosionTexture;
SDL_GPUTexture *outputTexture;
SDL_GPUSampler *sampler;
SDL_GPUGraphicsPipeline *explosionPipeline;
SDL_GPUGraphicsPipeline *textPipeline;
SDL_GPUTransferBuffer *outputDownload;
SDL_GPUBuffer *vertexBuffer;
SDL_GPUBuffer *indexBuffer;

SDL_Surface *output;

struct vertex {
  float x;
  float y;
  float z;
  float w;
  float u;
  float v;
  float reserved[2];
};

struct vertexUniforms {
  float projection[4][4];
  float transformation[4][4];
};

struct explosionUniforms {
  float colorGradientSpec[12];
  float alphaGradientSpec[12];
  float time;
  float padding[3];
};

const char *basePath;
const char *inputPath;
int result;
const char *outputBaseName;

int outWidth;
int outHeight;

#ifdef NDEBUG
#define debug false
#else
#define debug true
#endif

const float aspectTransform[][4] = {
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 4.0f / 5.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 1.0f},
};

const float alphaGradient[][4] = {
    {0.25f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.25f, 0.0f, 0.0f},
    {0.75f, 0.5f, 1.0f, 0.0f},
};

const float colorGradient300[][4] = {
    {0.25f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.25f, 0.0f, 0.0f},
    {0.75f, 0.75f, 1.0f, 0.0f},
};

const float colorGradient100[][4] = {
    {0.25f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.25f, 0.0f, 0.0f},
    {0.5f, 0.75f, 1.0f, 0.0f},
};

const SDL_FRect dimensions300 = {0.0f, 0.0f, 0.5f, 0.5f};
const SDL_FRect dimensions300g = {0.5f, 0.0f, 0.5f, 0.5f};
const SDL_FRect dimensions100 = {0.0f, 0.5f, 0.5f, 0.5f};

const SDL_FRect textureArea300 = {0.5f, 0.5f, 0.125f, 0.125f};
const SDL_FRect textureArea100 = {0.625f, 0.5f, 0.125f, 0.125f};
const SDL_FRect textureAreaMiss = {0.5f, 0.625f, 0.25f, 0.125f};
const SDL_FRect textRectHit = {-0.25f, -0.40625f, 0.5f, -0.5f};
const SDL_FRect textRectMiss = {-0.5f, -0.40625f, 1.0f, -0.5f};

const struct resultData {
  const SDL_FRect *explosionTextureArea;
  const SDL_FRect *textTextureArea;
  const SDL_FRect *textRect;
  const float (*colorGradient)[4];
} results[] = {
    {
        .explosionTextureArea = &dimensions300,
        .colorGradient = colorGradient300,
        .textTextureArea = &textureArea300,
        .textRect = &textRectHit,
    },
    {
        .explosionTextureArea = &dimensions100,
        .colorGradient = colorGradient100,
        .textTextureArea = &textureArea100,
        .textRect = &textRectHit,
    },
    {
        .explosionTextureArea = &dimensions300g,
        .colorGradient = colorGradient300,
        .textTextureArea = &textureArea300,
        .textRect = &textRectHit,
    },
    {
        .explosionTextureArea = &dimensions300,
        .colorGradient = colorGradient100,
        .textTextureArea = &textureArea100,
        .textRect = &textRectHit,
    },
    {
        .explosionTextureArea = &dimensions300,
        .colorGradient = NULL,
        .textTextureArea = &textureAreaMiss,
        .textRect = &textRectMiss,
    },
};

const char shortopts[] = "t:o:2";

const struct option longopts[] = {
    {"type", required_argument, NULL, 't'},
    {"output", required_argument, NULL, 'o'},
    {"hd", no_argument, NULL, '2'},
    {NULL, 0, NULL, 0},
};

const uint16_t indices[] = {0, 2, 1, 1, 2, 3};

SDL_GPUShader *loadShader(SDL_GPUDevice *dev, const char *name,
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
  effectiveParams.entrypoint = "main";

  SDL_GPUShader *shader = SDL_CreateGPUShader(dev, &effectiveParams);
  if (!shader) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "cannot load shader '%s'", name);
  }

  SDL_free(code);
  return shader;
}

int init() {
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
  SDL_GPUTransferBuffer *explosionVertexUpload = SDL_CreateGPUTransferBuffer(
      device, &(SDL_GPUTransferBufferCreateInfo){
                  .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                  .size = 64 * sizeof(float),
                  .props = 0,
              });
  SDL_GPUTransferBuffer *explosionIndexUpload = SDL_CreateGPUTransferBuffer(
      device, &(SDL_GPUTransferBufferCreateInfo){
                  .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                  .size = 8 * sizeof(uint16_t),
                  .props = 0,
              });
  SDL_GPUTransferBuffer *textureUpload = SDL_CreateGPUTransferBuffer(
      device, &(SDL_GPUTransferBufferCreateInfo){
                  .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                  .size = image->pitch * height,
                  .props = 0,
              });

  // shaders
  SDL_GPUShader *vertex = loadShader(device, "taiko-explosion.vert.spv",
                                     &(SDL_GPUShaderCreateInfo){
                                         .format = SDL_GPU_SHADERFORMAT_SPIRV,
                                         .stage = SDL_GPU_SHADERSTAGE_VERTEX,
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
                     .num_samplers = 1,
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
                     .num_samplers = 1,
                     .num_storage_buffers = 0,
                     .num_storage_textures = 0,
                     .num_uniform_buffers = 1,
                     .props = 0,
                 });

  if (!vertex || !explosionFragment || !textFragment) {
    goto fail;
  } else if (!explosionVertexUpload || !explosionIndexUpload ||
             !textureUpload) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "cannot create transfer buffers");
    goto fail;
  }

  // pipelines
  SDL_GPUGraphicsPipelineCreateInfo pipelineParams = {
      .vertex_shader = vertex,
      .vertex_input_state =
          {
              .vertex_buffer_descriptions =
                  (SDL_GPUVertexBufferDescription[]){
                      {
                          .slot = 0,
                          .pitch = 8 * sizeof(float),
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
                          .offset = 0,
                      },
                      {
                          .location = 1,
                          .buffer_slot = 0,
                          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                          .offset = 4 * sizeof(float),
                      },
                  },
              .num_vertex_attributes = 2,
          },
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
      .rasterizer_state = {0},
      .multisample_state = {0},
      .depth_stencil_state = {0},
      .target_info =
          {
              .color_target_descriptions =
                  (SDL_GPUColorTargetDescription[]){
                      {.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
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
                           }}},
              .num_color_targets = 1,
              .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_INVALID,
              .has_depth_stencil_target = false,
          },
      .props = 0,
  };

  pipelineParams.fragment_shader = explosionFragment;
  explosionPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineParams);

  pipelineParams.fragment_shader = textFragment;
  textPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineParams);

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

  compositeExplosionTexture = SDL_CreateGPUTexture(
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

  if (!compositeExplosionTexture || !textureUpload) {
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
  output = SDL_CreateSurface(outWidth, outHeight, SDL_PIXELFORMAT_ABGR8888);
  outputTexture = SDL_CreateGPUTexture(
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

  if (!outputTexture || !outputDownload) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "cannot set up textures");
    goto fail;
  }

  // vertex buffers
  vertexBuffer =
      SDL_CreateGPUBuffer(device, &(SDL_GPUBufferCreateInfo){
                                      .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
                                      .size = 64 * sizeof(float),
                                      .props = 0,
                                  });
  indexBuffer =
      SDL_CreateGPUBuffer(device, &(SDL_GPUBufferCreateInfo){
                                      .usage = SDL_GPU_BUFFERUSAGE_INDEX,
                                      .size = 8 * sizeof(uint16_t),
                                      .props = 0,
                                  });

  if (!vertexBuffer || !indexBuffer) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "cannot create vertex buffers");
    goto fail;
  }

  struct vertex *vertices =
      SDL_MapGPUTransferBuffer(device, explosionVertexUpload, false);

  vertices[0] = (struct vertex){-1.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0};
  vertices[1] = (struct vertex){1.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0};
  vertices[2] = (struct vertex){-1.0, -1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0};
  vertices[3] = (struct vertex){1.0, -1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0};
  vertices[4] = (struct vertex){0.0, 0.0, 0.5, 1.0, 0.0, 0.0, 0.0, 0.0};
  vertices[5] = (struct vertex){0.0, 0.0, 0.5, 1.0, 0.0, 0.0, 0.0, 0.0};
  vertices[6] = (struct vertex){0.0, 0.0, 0.5, 1.0, 0.0, 0.0, 0.0, 0.0};
  vertices[7] = (struct vertex){0.0, 0.0, 0.5, 1.0, 0.0, 0.0, 0.0, 0.0};

  vertices[0].u = vertices[2].u = explosionTextureArea->x;
  vertices[1].u = vertices[3].u =
      explosionTextureArea->x + explosionTextureArea->w;
  vertices[0].v = vertices[1].v = explosionTextureArea->y;
  vertices[2].v = vertices[3].v =
      explosionTextureArea->y + explosionTextureArea->h;

  vertices[4].x = vertices[6].x = textRect->x;
  vertices[5].x = vertices[7].x = textRect->x + textRect->w;
  vertices[4].y = vertices[5].y = textRect->y;
  vertices[6].y = vertices[7].y = textRect->y + textRect->h;
  vertices[4].u = vertices[6].u = textTextureArea->x;
  vertices[5].u = vertices[7].u = textTextureArea->x + textTextureArea->w;
  vertices[4].v = vertices[5].v = textTextureArea->y;
  vertices[6].v = vertices[7].v = textTextureArea->y + textTextureArea->h;

  SDL_UnmapGPUTransferBuffer(device, explosionVertexUpload);

  {
    uint16_t *buf =
        SDL_MapGPUTransferBuffer(device, explosionIndexUpload, false);
    memcpy(buf, indices, sizeof(indices));
    SDL_UnmapGPUTransferBuffer(device, explosionIndexUpload);
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
                             .texture = compositeExplosionTexture,
                             .layer = 0,
                             .x = 0,
                             .y = 0,
                             .z = 0,
                             .w = width,
                             .h = height,
                             .d = 1,
                         },
                         false);
  SDL_UploadToGPUBuffer(
      copy,
      &(SDL_GPUTransferBufferLocation){.transfer_buffer = explosionVertexUpload,
                                       .offset = 0},
      &(SDL_GPUBufferRegion){
          .buffer = vertexBuffer,
          .offset = 0,
          .size = 64 * sizeof(float),
      },
      false);
  SDL_UploadToGPUBuffer(
      copy,
      &(SDL_GPUTransferBufferLocation){.transfer_buffer = explosionIndexUpload,
                                       .offset = 0},
      &(SDL_GPUBufferRegion){
          .buffer = indexBuffer,
          .offset = 0,
          .size = 8 * sizeof(uint16_t),
      },
      false);

  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(cmd);

  SDL_DestroySurface(image);
  SDL_ReleaseGPUShader(device, vertex);
  SDL_ReleaseGPUShader(device, explosionFragment);
  SDL_ReleaseGPUShader(device, textFragment);
  SDL_ReleaseGPUTransferBuffer(device, textureUpload);
  SDL_ReleaseGPUTransferBuffer(device, explosionVertexUpload);
  SDL_ReleaseGPUTransferBuffer(device, explosionIndexUpload);

  return 0;

fail:
  SDL_DestroySurface(image);
  SDL_ReleaseGPUShader(device, vertex);
  SDL_ReleaseGPUShader(device, explosionFragment);
  SDL_ReleaseGPUShader(device, textFragment);
  SDL_ReleaseGPUTransferBuffer(device, textureUpload);
  SDL_ReleaseGPUTransferBuffer(device, explosionVertexUpload);
  SDL_ReleaseGPUTransferBuffer(device, explosionIndexUpload);
  return 1;
}

void renderExplosion(SDL_GPUCommandBuffer *cmd, SDL_GPURenderPass *render,
                     double time) {
  if (results[result].colorGradient == NULL || time < 0 ||
      time >= 1.0)
    return;

  struct vertexUniforms vertexUniforms = {
      .transformation =
          {
              {1.0f, 0.0f, 0.0f, 0.0f},
              {0.0f, 1.0f, 0.0f, 0.0f},
              {0.0f, 0.0f, 1.0f, 0.0f},
              {0.0f, 0.0f, 0.0f, 1.0f},
          },
  };
  memcpy(&vertexUniforms.projection, aspectTransform, sizeof(aspectTransform));

  struct explosionUniforms fragmentUniforms;
  memcpy(&fragmentUniforms.colorGradientSpec, results[result].colorGradient,
         12 * sizeof(float));
  memcpy(&fragmentUniforms.alphaGradientSpec, alphaGradient,
         12 * sizeof(float));
  fragmentUniforms.time = (time / 1.0) + (1.0f / 256.0f);

  SDL_BindGPUGraphicsPipeline(render, explosionPipeline);
  SDL_BindGPUVertexBuffers(
      render, 0,
      (SDL_GPUBufferBinding[]){{.buffer = vertexBuffer, .offset = 0}}, 1);
  SDL_BindGPUIndexBuffer(
      render, &(SDL_GPUBufferBinding){.buffer = indexBuffer, .offset = 0},
      SDL_GPU_INDEXELEMENTSIZE_16BIT);
  SDL_PushGPUVertexUniformData(cmd, 0, &vertexUniforms, sizeof(vertexUniforms));
  SDL_PushGPUFragmentUniformData(cmd, 0, &fragmentUniforms,
                                 sizeof(fragmentUniforms));
  SDL_BindGPUFragmentSamplers(
      render, 0,
      (SDL_GPUTextureSamplerBinding[]){
          {.sampler = sampler, .texture = compositeExplosionTexture}},
      1);
  SDL_DrawGPUIndexedPrimitives(render, 6, 1, 0, 0, 0);
}

void renderText(SDL_GPUCommandBuffer *cmd, SDL_GPURenderPass *render,
                double time) {
  // animate
  float alpha = 1.0;

  double y;
  if (time <= 5) {
    y = (1.0 / 50.0) * pow(time, 2) - (1.0 / 10.0) * time;
  } else {
    y = 0.0;
  }

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
          {.sampler = sampler, .texture = compositeExplosionTexture}},
      1);
  SDL_DrawGPUIndexedPrimitives(render, 6, 1, 0, 0, 0);
}

int process(double time, SDL_Surface *surface) {
  SDL_GPUCommandBuffer *renderCmds = SDL_AcquireGPUCommandBuffer(device);

  SDL_GPURenderPass *render =
      SDL_BeginGPURenderPass(renderCmds,
                             &(SDL_GPUColorTargetInfo){
                                 .texture = outputTexture,
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

  renderExplosion(renderCmds, render, (time + 0.25) / 9.0);
  renderText(renderCmds, render, time);

  SDL_EndGPURenderPass(render);
  SDL_SubmitGPUCommandBuffer(renderCmds);

  SDL_GPUCommandBuffer *copyCmds = SDL_AcquireGPUCommandBuffer(device);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(copyCmds);
  SDL_DownloadFromGPUTexture(copy,
                             &(SDL_GPUTextureRegion){
                                 .texture = outputTexture,
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
  SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(copyCmds);
  SDL_WaitForGPUFences(device, false, (SDL_GPUFence *[]){fence}, 1);
  SDL_ReleaseGPUFence(device, fence);

  const void *outputBuf =
      SDL_MapGPUTransferBuffer(device, outputDownload, false);
  memcpy(output->pixels, outputBuf, output->pitch * output->h);
  SDL_UnmapGPUTransferBuffer(device, outputDownload);
  return 0;
}

void cleanup() {
  SDL_DestroySurface(output);
  SDL_ReleaseGPUTransferBuffer(device, outputDownload);
  SDL_ReleaseGPUTexture(device, compositeExplosionTexture);
  SDL_ReleaseGPUTexture(device, outputTexture);
  SDL_ReleaseGPUSampler(device, sampler);
  SDL_ReleaseGPUGraphicsPipeline(device, explosionPipeline);
  SDL_ReleaseGPUGraphicsPipeline(device, textPipeline);
  SDL_ReleaseGPUBuffer(device, vertexBuffer);
  SDL_ReleaseGPUBuffer(device, indexBuffer);
  SDL_DestroyGPUDevice(device);
}

int main(int argc, char **argv) {
  SDL_Init(SDL_INIT_VIDEO);

  result = 0;
  outputBaseName = NULL;

  const char *format = "%s-%d.png";

  // read cmdline args
  int opt;
  while ((opt = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
    switch (opt) {
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
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No input texture");
    return EXIT_FAILURE;
  }
  inputPath = argv[optind];

  if (outputBaseName == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No output template");
    return EXIT_FAILURE;
  }

  // process
  if (init() != 0) {
    cleanup();
    return EXIT_FAILURE;
  }

#define ANIMATION_FRAMES 10

  for (int i = 0; i < ANIMATION_FRAMES; ++i) {
    process(i, output);

    int outputNameSize = snprintf(NULL, 0, format, outputBaseName, i) + 1;
    char *outputName = SDL_malloc(outputNameSize);
    snprintf(outputName, outputNameSize, format, outputBaseName, i);
    IMG_SavePNG(output, outputName);
  }

  cleanup();
  return EXIT_SUCCESS;
}
