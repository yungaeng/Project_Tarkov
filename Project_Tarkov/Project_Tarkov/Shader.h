#pragma once

class Shader
{
private:
    unsigned int program = 0;

public:
    bool Load(const char* vertexSrc, const char* fragmentSrc)
    {
        unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vertexSrc, nullptr);
        glCompileShader(vs);

        unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fragmentSrc, nullptr);
        glCompileShader(fs);

        program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);

        glDeleteShader(vs);
        glDeleteShader(fs);

        return true;
    }

    void Bind()
    {
        glUseProgram(program);
    }

    void SetMat4(const char* name, const glm::mat4& mat)
    {
        glUniformMatrix4fv(
            glGetUniformLocation(program, name),
            1,
            GL_FALSE,
            glm::value_ptr(mat));
    }
};

