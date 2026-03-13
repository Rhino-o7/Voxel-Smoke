#include <iostream>
#include "gl/common.h"

void yc::gl::checkOpenGLError(const char* stmt, const char* fname, int line) {
    // Helper used by GL_CHECK to report the first pending OpenGL error.
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cout << "OpenGL error %08x, at %s:%i - for %s\n", err, fname, line, stmt;
    }
}