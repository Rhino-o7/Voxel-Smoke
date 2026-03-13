#include <iostream>
#include <regex>
#include <sstream>
#include <string>

#include <glm/gtc/type_ptr.hpp>

#include "gl/shader.h"
#include "util/file.h"

namespace yc::gl {

const std::string shaderFolder = "../Client-Web/resources/shaders/";

namespace {
    void ReplaceAll(std::string& text, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = text.find(from, pos)) != std::string::npos) {
            text.replace(pos, from.length(), to);
            pos += to.length();
        }
    }

    std::string AdaptForWebGL2(std::string source, GLenum shaderType) {
        (void)shaderType;

        if (source.find("#version 300 es") != std::string::npos) {
            return source;
        }

        std::stringstream input(source);
        std::string line;
        std::string rebuilt;
        bool replacedVersion = false;

        while (std::getline(input, line)) {
            if (line.find("#version") != std::string::npos) {
                line = "#version 300 es";
                replacedVersion = true;
            }
            rebuilt += line + "\n";
        }

        source = rebuilt;

		// Convert shaders to be compatible with WebGL
        if (!replacedVersion) {
            source = std::string("#version 300 es\n") + source;
        }
        
        ReplaceAll(source, "texture2D(", "texture(");
        ReplaceAll(source, "textureCube(", "texture(");
        ReplaceAll(source, "layout (binding = 0) uniform", "uniform");
        ReplaceAll(source, "layout (binding = 1) uniform", "uniform");
        ReplaceAll(source, "sprite_size * tex_coord.x", "sprite_size * float(tex_coord.x)");
        ReplaceAll(source, "sprite_size * tex_coord.y", "sprite_size * float(tex_coord.y)");
        ReplaceAll(source, "sprite_size * texcoord.x", "sprite_size * float(texcoord.x)");
        ReplaceAll(source, "sprite_size * texcoord.y", "sprite_size * float(texcoord.y)");
        ReplaceAll(source, "normals[min(vFaceIndex, 5u)]", "normals[int(min(vFaceIndex, 5u))]");
        ReplaceAll(source, "if (color.w == 0)", "if (color.w == 0.0)");
        ReplaceAll(source, "if (color.w == 1)", "if (color.w == 1.0)");
        ReplaceAll(source, "1.0 / 16", "1.0 / 16.0");
        ReplaceAll(source, "tex_coord = uvec2(tex_x, 15 - tex_y);", "tex_coord = uvec2(tex_x, 15u - tex_y);");
        ReplaceAll(source, "texcoord.y = 15 - texcoord.y;", "texcoord.y = 15u - texcoord.y;");
        ReplaceAll(source, "vec3(x, y, z)", "vec3(float(x), float(y), float(z))");
        ReplaceAll(source, "vec4(x, y, z, 1.0)", "vec4(float(x), float(y), float(z), 1.0)");
        ReplaceAll(source, "/ atlas_size", "/ float(atlas_size)");
        ReplaceAll(source, "* atlas_size", "* float(atlas_size)");
        ReplaceAll(source, "1-outline_size", "1.0-outline_size");
        ReplaceAll(source, "1 - outline_size", "1.0 - outline_size");
        ReplaceAll(source, "236.0/255", "236.0/255.0");
        ReplaceAll(source, "240.0/255", "240.0/255.0");
        ReplaceAll(source, "1.0*width/screen_width", "1.0*float(width)/float(screen_width)");
        ReplaceAll(source, "1.0*height/screen_height", "1.0*float(height)/float(screen_height)");
        ReplaceAll(source, "float b = 1 - d(z);", "float b = 1.0 - d(z);");
        ReplaceAll(source, "w *= 10;", "w *= 10.0;");
        source = std::regex_replace(source, std::regex("([0-9]+\\.[0-9]+)f"), "$1");
        source = std::regex_replace(source, std::regex("uniform\\s+([^=;\\n]+)\\s*=\\s*[^;]+;"), "uniform $1;");

        if (source.find("bitfieldExtract(") != std::string::npos) {
            const std::string helper =
                "uint WebBitfieldExtract(uint value, int offset, int bits) {\n"
                "    uint mask = (1u << uint(bits)) - 1u;\n"
                "    return (value >> uint(offset)) & mask;\n"
                "}\n";

            const size_t insertPos = source.find('\n');
            if (insertPos != std::string::npos) {
                source.insert(insertPos + 1, helper);
            } else {
                source += "\n" + helper;
            }

            ReplaceAll(source, "bitfieldExtract(", "WebBitfieldExtract(");
        }

        const std::string precisionBlock = "precision highp float;\nprecision highp int;\n";
        const size_t versionEnd = source.find('\n');
        if (versionEnd != std::string::npos) {
            source.insert(versionEnd + 1, precisionBlock);
        } else {
            source += "\n" + precisionBlock;
        }

        return source;
    }
}

Shader::Shader() {
}

void Shader::use() {
    glUseProgram(this->id);
}

void Shader::loadFromFile(const std::string& vertexShaderPath,
    const std::string& fragmentShaderPath) {

    std::string vertexShaderSrc = yc::util::readTextFromFile(shaderFolder + vertexShaderPath);
    std::string fragmentShaderSrc = yc::util::readTextFromFile(shaderFolder + fragmentShaderPath);

    vertexShaderSrc = AdaptForWebGL2(vertexShaderSrc, GL_VERTEX_SHADER);
    fragmentShaderSrc = AdaptForWebGL2(fragmentShaderSrc, GL_FRAGMENT_SHADER);

    const char* vertexShaderSrcC = vertexShaderSrc.c_str();
    const char* fragmentShaderSrcC = fragmentShaderSrc.c_str();

    GLint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vertexShader, 1, &vertexShaderSrcC, NULL);
    glShaderSource(fragmentShader, 1, &fragmentShaderSrcC, NULL);

    GL_CHECK(glCompileShader(vertexShader));
    GL_CHECK(glCompileShader(fragmentShader));

    checkCompileErrors(vertexShader);
    checkCompileErrors(fragmentShader);

    this->id = glCreateProgram();
    glAttachShader(this->id, vertexShader);
    glAttachShader(this->id, fragmentShader);
    glLinkProgram(this->id);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    std::cout << "Loaded shaders " << vertexShaderPath << " and " << fragmentShaderPath << '\n';
}

void Shader::checkCompileErrors(unsigned int shader) {
    int success;
    char infoLog[1024];

    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 1024, NULL, infoLog);
        std::cout << "OpenGL shader error: " << "\n" << infoLog << "\n";
    }
}

GLuint Shader::getUniform(const std::string& name) {
    return glGetUniformLocation(this->id, name.c_str());
}

void Shader::setInt(const std::string& name, int value) {
    glUniform1i(getUniform(name), value);
}

void Shader::setFloat(const std::string& name, float value) {
    glUniform1f(getUniform(name), value);
}

void Shader::setVec2(const std::string& name, glm::vec2 value) {
    glUniform2f(getUniform(name), value.x, value.y);
}

void Shader::setVec3(const std::string& name, glm::vec3 value) {
    glUniform3f(getUniform(name), value.x, value.y, value.z);
}

void Shader::setMat4(const std::string& name, glm::mat4 value) {
    glUniformMatrix4fv(getUniform(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setUArray(const std::string& name, const std::vector<uint32_t>& value) {
    glUniform1uiv(getUniform(name), value.size(), value.data());
}

void Shader::setVec4(const std::string& name, glm::vec4 value) {
    glUniform4f(getUniform(name), value.x, value.y, value.z, value.w);
}

void Shader::setVec4Array(const std::string& name, const std::vector<glm::vec4>& value) {
    if (value.empty()) return;
    glUniform4fv(getUniform(name), static_cast<GLsizei>(value.size()), glm::value_ptr(value[0]));
}

}
