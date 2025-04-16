#include "shaders.h"
#include <iostream>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

std::string FragmentShaderCode = R"(
#version 330 core
in vec3 normal;
in vec3 position;
in vec2 texcoord;

uniform sampler2D tex;
uniform vec3 sun_position;
uniform vec3 sun_color;

out vec4 color;
void main() {
    float lum = max(dot(normal, normalize(sun_position)), 0.0);
    color = texture(tex, texcoord) * vec4((0.3 + 0.7 * lum) * sun_color, 1.0);
}
)";

std::string VertexShaderCode = R"(
#version 330 core
layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_texcoord;

uniform mat4 MVP;

out vec3 normal;
out vec3 position;
out vec2 texcoord;

void main() {
    gl_Position = MVP * vec4(in_vertex, 1);
    position = gl_Position.xyz;
    normal = normalize(mat3(MVP) * in_normal);
    position = in_vertex;
    texcoord = in_texcoord;
}
)";

Shaders::Shaders() {
    GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

    GLint Result = GL_FALSE;
    int InfoLogLength;

    // Compile Vertex Shader
    const char* VertexSourcePointer = VertexShaderCode.c_str();
    glShaderSource(VertexShaderID, 1, &VertexSourcePointer, nullptr);
    glCompileShader(VertexShaderID);

    glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
    glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
    if (InfoLogLength > 0) {
        std::vector<char> VertexShaderErrorMessage(InfoLogLength + 1);
        glGetShaderInfoLog(VertexShaderID, InfoLogLength, nullptr, VertexShaderErrorMessage.data());
        std::cerr << "Vertex Shader Error:\n" << VertexShaderErrorMessage.data() << std::endl;
    }

    // Compile Fragment Shader
    const char* FragmentSourcePointer = FragmentShaderCode.c_str();
    glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer, nullptr);
    glCompileShader(FragmentShaderID);

    glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
    glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
    if (InfoLogLength > 0) {
        std::vector<char> FragmentShaderErrorMessage(InfoLogLength + 1);
        glGetShaderInfoLog(FragmentShaderID, InfoLogLength, nullptr, FragmentShaderErrorMessage.data());
        std::cerr << "Fragment Shader Error:\n" << FragmentShaderErrorMessage.data() << std::endl;
    }

    // Link the program
    std::cout << "Linking program\n";
    GLuint ProgramID = glCreateProgram();
    glAttachShader(ProgramID, VertexShaderID);
    glAttachShader(ProgramID, FragmentShaderID);
    glLinkProgram(ProgramID);

    glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
    glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
    if (InfoLogLength > 0) {
        std::vector<char> ProgramErrorMessage(InfoLogLength + 1);
        glGetProgramInfoLog(ProgramID, InfoLogLength, nullptr, ProgramErrorMessage.data());
        std::cerr << "Shader Link Error:\n" << ProgramErrorMessage.data() << std::endl;
    }

    glDetachShader(ProgramID, VertexShaderID);
    glDetachShader(ProgramID, FragmentShaderID);
    glDeleteShader(VertexShaderID);
    glDeleteShader(FragmentShaderID);

    this->pid = ProgramID;
}

Shaders::~Shaders() {
    if (pid != 0) {
        glDeleteProgram(pid);
    }
}
