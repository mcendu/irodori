/*
 * Copyright (c) 2024 McEndu.
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

#include "defringe.h"

#include <png.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *argv0;

int main(int argc, char **argv) {
  argv0 = argv[0];

  if (argc != 3) {
    error(ERROR_ERROR, "program requires an input and output argument");
    exit(EXIT_FAILURE);
  }

  const char *inpath = argv[1];
  const char *outpath = argv[2];

  // open input
  png_image image;
  image.version = PNG_IMAGE_VERSION;
  image.opaque = NULL;
  image.warning_or_error = 0;

  // load image
  if (png_image_begin_read_from_file(&image, inpath) == 0) {
    error(ERROR_ERROR, "while reading %s: %s", inpath, image.message);
    png_image_free(&image);
    exit(EXIT_FAILURE);
  } else if (image.warning_or_error) {
    error(ERROR_WARNING, "while reading %s: %s", inpath, image.message);
  }
  image.warning_or_error = 0;

  image.format = PNG_FORMAT_RGBA;
  unsigned int width = image.width;
  unsigned int height = image.height;
  unsigned char(*data)[4] = malloc(PNG_IMAGE_SIZE(image));

  if (!data) {
    error(ERROR_ERROR, "cannot allocate image data buffer");
    png_image_free(&image);
    exit(EXIT_FAILURE);
  };

  if (png_image_finish_read(&image, NULL, data, 0, NULL) == 0) {
    error(ERROR_ERROR, "while reading %s: %s", inpath, image.message);
    png_image_free(&image);
    exit(EXIT_FAILURE);
  } else if (image.warning_or_error) {
    error(ERROR_WARNING, "while reading %s: %s", inpath, image.message);
  }
  image.warning_or_error = 0;

  // defringe; make transparent pixels black
  for (unsigned int i = 0; i < width * height; ++i) {
    if (data[i][3] == 0) {
      memset(data[i], 0, 3);
    }
  }

  // write
  if (png_image_write_to_file(&image, outpath, 0, data, 0, NULL) == 0) {
    error(ERROR_ERROR, "while writing to %s: %s", outpath, image.message);
    free(data);
    exit(EXIT_FAILURE);
  } else if (image.warning_or_error) {
    error(ERROR_WARNING, "while writing to %s: %s", outpath, image.message);
  }

  free(data);
  exit(EXIT_SUCCESS);
}

void error(int priority, const char *format, ...) {
  va_list ap;
  static const char *priority_text[] = {
      [ERROR_INFO] = "info",
      [ERROR_WARNING] = "warning",
      [ERROR_ERROR] = "error",
  };

  if (priority > ERROR_ERROR)
    priority = ERROR_ERROR;

  fprintf(stderr, "%s: %s: ", argv0, priority_text[priority]);
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fputs("\n", stderr);
}