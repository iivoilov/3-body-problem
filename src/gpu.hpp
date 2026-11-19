#pragma once

#include <GL/glew.h>
#include "math.hpp"
#include <cstddef>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace orbit {
class Shader {
public:
    Shader(const char* vertexFile, const char* fragmentFile) {
        const GLuint vertex = compile(GL_VERTEX_SHADER, vertexFile);
        GLuint fragment = 0;
        try { fragment = compile(GL_FRAGMENT_SHADER, fragmentFile); }
        catch (...) { glDeleteShader(vertex); throw; }
        id_ = glCreateProgram();
        glAttachShader(id_, vertex);
        glAttachShader(id_, fragment);
        glLinkProgram(id_);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        GLint ok = 0;
        glGetProgramiv(id_, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[4096]{};
            glGetProgramInfoLog(id_, sizeof(log), nullptr, log);
            glDeleteProgram(id_);
            throw std::runtime_error(std::string("Shader link failed: ") + log);
        }
    }
    ~Shader() { glDeleteProgram(id_); }
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    void use() const { glUseProgram(id_); }
    void set(const char* name, const Mat4& m) const {
        glUniformMatrix4fv(location(name), 1, GL_FALSE, m.data);
    }
    void set(const char* name, Vec3 v) const { glUniform3f(location(name), v.x, v.y, v.z); }
    void set(const char* name, float v) const { glUniform1f(location(name), v); }
    void set(const char* name, int v) const { glUniform1i(location(name), v); }
    void viewport(float width, float height) const { glUniform2f(location("viewport"), width, height); }

private:
    GLuint id_ = 0;
    GLint location(const char* name) const { return glGetUniformLocation(id_, name); }
    static GLuint compile(GLenum type, const char* filename) {
        const std::string path = std::string(SHADER_DIR) + "/" + filename;
        std::ifstream file(path);
        if (!file) throw std::runtime_error("Cannot read shader: " + path);
        const std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        const char* text = source.c_str();
        const GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &text, nullptr);
        glCompileShader(shader);
        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[4096]{};
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            glDeleteShader(shader);
            throw std::runtime_error(path + ":\n" + log);
        }
        return shader;
    }
};

struct Vertex { Vec3 position, normal; };

// VAO describes the vertex layout; VBO holds vertices; EBO holds triangle indices.
class Mesh {
public:
    Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned>& indices, GLenum mode)
        : mode_(mode), count_(static_cast<GLsizei>(indices.empty() ? vertices.size() : indices.size())),
          indexed_(!indices.empty()) {
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
        if (indexed_) {
            glGenBuffers(1, &ebo_);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned)), indices.data(), GL_STATIC_DRAW);
        }
        glBindVertexArray(0);
    }
    ~Mesh() {
        glDeleteBuffers(1, &ebo_);
        glDeleteBuffers(1, &vbo_);
        glDeleteVertexArrays(1, &vao_);
    }
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    void update(const std::vector<Vertex>& vertices) {
        count_ = static_cast<GLsizei>(vertices.size());
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data(), GL_DYNAMIC_DRAW);
    }
    void draw() const {
        glBindVertexArray(vao_);
        if (indexed_) glDrawElements(mode_, count_, GL_UNSIGNED_INT, nullptr);
        else glDrawArrays(mode_, 0, count_);
    }
private:
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
    GLenum mode_;
    GLsizei count_;
    bool indexed_;
};
} // namespace orbit
