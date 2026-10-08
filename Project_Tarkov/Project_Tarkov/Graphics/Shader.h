#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <filesystem>

class Shader
{
private:
    unsigned int program = 0;

    std::string vertexPath;
    std::string fragmentPath;

    std::filesystem::file_time_type vsTime;
    std::filesystem::file_time_type fsTime;

private:
    std::string ReadFile(const std::string& path)
    {
        std::ifstream file(path);

        if (!file.is_open())
        {
            std::cout << "File Open Failed : "
                << path << std::endl;
            return "";
        }

        std::stringstream ss;
        ss << file.rdbuf();

        return ss.str();
    }

    bool CheckShader(unsigned int shader)
    {
        int success;
        char info[1024];

        glGetShaderiv(
            shader,
            GL_COMPILE_STATUS,
            &success);

        if (!success)
        {
            glGetShaderInfoLog(
                shader,
                1024,
                nullptr,
                info);

            std::cout << "\n[Shader Compile Error]\n"
                << info << std::endl;

            return false;
        }

        return true;
    }

    bool CheckProgram(unsigned int prog)
    {
        int success;
        char info[1024];

        glGetProgramiv(
            prog,
            GL_LINK_STATUS,
            &success);

        if (!success)
        {
            glGetProgramInfoLog(
                prog,
                1024,
                nullptr,
                info);

            std::cout << "\n[Program Link Error]\n"
                << info << std::endl;

            return false;
        }

        return true;
    }

    bool Compile(
        const char* vsSrc,
        const char* fsSrc)
    {
        unsigned int vs =
            glCreateShader(GL_VERTEX_SHADER);

        glShaderSource(vs, 1, &vsSrc, nullptr);
        glCompileShader(vs);

        if (!CheckShader(vs))
        {
            glDeleteShader(vs);
            return false;
        }

        unsigned int fs =
            glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(fs, 1, &fsSrc, nullptr);
        glCompileShader(fs);

        if (!CheckShader(fs))
        {
            glDeleteShader(vs);
            glDeleteShader(fs);
            return false;
        }

        unsigned int newProgram =
            glCreateProgram();

        glAttachShader(newProgram, vs);
        glAttachShader(newProgram, fs);
        glLinkProgram(newProgram);

        if (!CheckProgram(newProgram))
        {
            glDeleteShader(vs);
            glDeleteShader(fs);
            glDeleteProgram(newProgram);
            return false;
        }

        glDeleteShader(vs);
        glDeleteShader(fs);

        if (program != 0)
            glDeleteProgram(program);

        program = newProgram;

        return true;
    }

public:
    Shader() = default;
    ~Shader() { Reset(); }
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    void Reset()
    {
        if (program) glDeleteProgram(program);
        program = 0;
        vertexPath.clear();
        fragmentPath.clear();
    }
    bool LoadFromFile(
        const std::string& vs,
        const std::string& fs)
    {
        vertexPath = vs;
        fragmentPath = fs;

        std::error_code vsError, fsError;
        const auto newVs = std::filesystem::last_write_time(vertexPath, vsError);
        const auto newFs = std::filesystem::last_write_time(fragmentPath, fsError);
        if (vsError || fsError) return false;
        // Record attempts so a broken edit is not recompiled every frame.
        vsTime = newVs;
        fsTime = newFs;
        std::string vsCode =
            ReadFile(vertexPath);

        std::string fsCode =
            ReadFile(fragmentPath);

        if (vsCode.empty() ||
            fsCode.empty())
            return false;

        if (!Compile(
            vsCode.c_str(),
            fsCode.c_str()))
            return false;

        std::cout
            << "[Shader Loaded] "
            << vertexPath
            << " / "
            << fragmentPath
            << std::endl;

        return true;
    }

    void HotReload()
    {
        if (vertexPath.empty() || fragmentPath.empty()) return;
        std::error_code vsError, fsError;
        const auto newVs = std::filesystem::last_write_time(vertexPath, vsError);
        const auto newFs = std::filesystem::last_write_time(fragmentPath, fsError);
        if (vsError || fsError) return;
        if (newVs != vsTime ||
            newFs != fsTime)
        {
            std::cout
                << "[Shader Changed] Reloading...\n";

            LoadFromFile(
                vertexPath,
                fragmentPath);
        }
    }

    void Bind()
    {
        glUseProgram(program);
    }

    void SetVec3(
        const char* name,
        const glm::vec3& v)
    {
        glUniform3fv(
            glGetUniformLocation(program, name),
            1,
            glm::value_ptr(v));
    }

    void SetMat4(
        const char* name,
        const glm::mat4& mat)
    {
        glUniformMatrix4fv(
            glGetUniformLocation(
                program,
                name),
            1,
            GL_FALSE,
            glm::value_ptr(mat));
    }
};
