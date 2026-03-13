#pragma once

#include <GLES3/gl3.h>

typedef void* (*GLADloadproc)(const char*);

static inline int gladLoadGLLoader(GLADloadproc) {
    return 1;
}
