#include <vector>
#include <iostream>

#include <stdio.h>
#include <windows.h>

#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

const char *vert_src = "\
#version 300 es\n\
layout (location = 0) in vec3 in_position;\n\
layout (location = 1) in vec2 in_texcoord;\n\
out vec2 texcoord;\n\
void main()\n\
{\n\
    texcoord = in_texcoord;\n\
    gl_Position = vec4(in_position, 1.0);\n\
}\n\
";

const char *frag_src = "\
#version 300 es\n\
in vec2 texcoord;\n\
out vec3 fragcolor;\n\
uniform sampler2D tex;\n\
void main()\n\
{\n\
    fragcolor = 0.5;\n\
}\n\
";

// fragcolor = 1.0 - texture(tex, texcoord).rgb;\n\

int main(int argc, char *argv[])
{
    //========== shader program
    GLuint vert_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_shader, 1, &vert_src, NULL);
    glCompileShader(vert_shader);

    int success;
    char infoLog[512];
    glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vert_shader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_shader, 1, &frag_src, NULL);
    glCompileShader(frag_shader);

    glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(frag_shader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vert_shader);
    glAttachShader(program, frag_shader);
    glLinkProgram(program);
    glUseProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    //========== VAO VBO
    float vertices[] = {
        // positions            // texture_coords
        -1.0f, -1.0f,  0.0f,    0.0f, 0.0f,
        -1.0f,  1.0f,  0.0f,    0.0f, 1.0f,
         1.0f, -1.0f,  0.0f,    1.0f, 0.0f,
         1.0f,  1.0f,  0.0f,    1.0f, 1.0f
    };

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // positions attribute
    glVertexAttribPointer(glGetAttribLocation(program, "in_position"), 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    // texture_coords attribute
    glVertexAttribPointer(glGetAttribLocation(program, "in_texcoord"), 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    //========== read image data
    unsigned int width = 320, height = 240;
    std::vector<uint8_t> img(width*height*3, 128);
    //auto error = lodepng::decode(img, width, height, argv[1], LCT_RGB);
    //if (error) {
    //    std::cerr << "decoder error " << error << ": " << lodepng_error_text(error) << std::endl;
    //    std::exit(1);
    //}
    printf("image width: %d, height: %d\n", width, height);

    std::vector<uint8_t> img_out(width * height * 3);

    //========== upload image to input texture
    GLuint texture_in;
    glGenTextures(1, &texture_in);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_in);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, img.data());

    //========== output texture
    GLuint texture_out;
    glGenTextures(1, &texture_out);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texture_out);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

    //========== output framebuffer
    GLuint framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_out, 0);
    glViewport(0, 0, width, height);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
        printf("glCheckFramebufferStatus success.\n");
    }
    else {
        printf("glCheckFramebufferStatus error. %d\n", glCheckFramebufferStatus(GL_FRAMEBUFFER));
    }

    //========== render
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    //========== read output image
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, (GLvoid *)img_out.data());

    //error = lodepng::encode("results/img_out.png", img_out, width, height, LCT_RGB);
    //if (error) std::cout << "encoder error " << error << ": " << lodepng_error_text(error) << std::endl;

    return 0;
}
