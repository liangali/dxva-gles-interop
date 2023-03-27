#include <vector>
#include <iostream>
#include <fstream>

#include <stdio.h>
#include <windows.h>

#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../stb/stb_image.h"

#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stb/stb_image_write.h"

#ifdef _WIN64
#define GWL_USERDATA GWLP_USERDATA
#endif

/// esCreateWindow flag - RGB color buffer
#define ES_WINDOW_RGB           0
/// esCreateWindow flag - ALPHA color buffer
#define ES_WINDOW_ALPHA         1
/// esCreateWindow flag - depth buffer
#define ES_WINDOW_DEPTH         2
/// esCreateWindow flag - stencil buffer
#define ES_WINDOW_STENCIL       4
/// esCreateWindow flat - multi-sample buffer
#define ES_WINDOW_MULTISAMPLE   8

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
precision mediump float;                     \n\
in vec2 texcoord;\n\
out vec3 fragcolor;\n\
uniform sampler2D tex;\n\
void main()\n\
{\n\
    fragcolor = 1.0 - texture(tex, texcoord).rgb;\n\
}\n\
";

// fragcolor = 1.0 - texture(tex, texcoord).rgb;\n\

typedef struct
{
    // Handle to a program object
    GLuint programObject;
} UserData;

struct ESContext
{
    void* platformData;
    void* userData;
    GLint width;
    GLint height;
    EGLNativeDisplayType eglNativeDisplay;
    EGLNativeWindowType  eglNativeWindow;
    EGLDisplay  eglDisplay;
    EGLContext  eglContext;
    EGLSurface  eglSurface;

};

EGLint getContextRenderableType(EGLDisplay eglDisplay)
{
#ifdef EGL_KHR_create_context
    const char* extensions = eglQueryString(eglDisplay, EGL_EXTENSIONS);

    // check whether EGL_KHR_create_context is in the extension string
    if (extensions != NULL && strstr(extensions, "EGL_KHR_create_context"))
    {
        // extension is supported
        return EGL_OPENGL_ES3_BIT_KHR;
    }
#endif
    // extension is not supported
    return EGL_OPENGL_ES2_BIT;
}

LRESULT WINAPI esWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT  lRet = 1;
    return lRet;
}

GLboolean winCreate(ESContext* esContext, const char* title)
{
    WNDCLASS wndclass = { 0 };
    DWORD    wStyle = 0;
    RECT     windowRect;
    HINSTANCE hInstance = GetModuleHandle(NULL);

    wndclass.style = CS_OWNDC;
    wndclass.lpfnWndProc = (WNDPROC)esWindowProc;
    wndclass.hInstance = hInstance;
    wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndclass.lpszClassName = "opengles3.0";

    if (!RegisterClass(&wndclass))
    {
        return FALSE;
    }

    wStyle = WS_VISIBLE | WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION;

    // Adjust the window rectangle so that the client area has
    // the correct number of pixels
    windowRect.left = 0;
    windowRect.top = 0;
    windowRect.right = esContext->width;
    windowRect.bottom = esContext->height;

    AdjustWindowRect(&windowRect, wStyle, FALSE);

    esContext->eglNativeWindow = CreateWindow(
        "opengles3.0",
        title,
        wStyle,
        0,
        0,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        NULL,
        NULL,
        hInstance,
        NULL);

    // Set the ESContext* to the GWL_USERDATA so that it is available to the ESWindowProc
#ifdef _WIN64
   //In LLP64 LONG is still 32bit.
    SetWindowLongPtr(esContext->eglNativeWindow, GWL_USERDATA, (LONGLONG)(LONG_PTR)esContext);
#else
    SetWindowLongPtr(esContext->eglNativeWindow, GWL_USERDATA, (LONG)(LONG_PTR)esContext);
#endif

    if (esContext->eglNativeWindow == NULL)
    {
        return GL_FALSE;
    }

    ShowWindow(esContext->eglNativeWindow, TRUE);

    return GL_TRUE;
}

int initEGL(ESContext* esContext)
{
    if (!winCreate(esContext, "Triangle"))
    {
        printf("ERROR: WinCreate failed! \n");
        return -1;
    }

    esContext->eglDisplay = eglGetDisplay(esContext->eglNativeDisplay);
    if (esContext->eglDisplay == EGL_NO_DISPLAY)
    {
        printf("ERROR: eglGetDisplay failed! \n");
        return -1;
    }

    EGLConfig config = {};
    EGLint majorVersion = 0;
    EGLint minorVersion = 0;
    EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };

    // Initialize EGL
    if (!eglInitialize(esContext->eglDisplay, &majorVersion, &minorVersion))
    {
        printf("ERROR: eglInitialize failed! \n");
        return -1;
    }

    GLuint flags = ES_WINDOW_RGB;
    EGLint numConfigs = 0;
    EGLint attribList[] =
    {
       EGL_RED_SIZE,       5,
       EGL_GREEN_SIZE,     6,
       EGL_BLUE_SIZE,      5,
       EGL_ALPHA_SIZE,     (flags & ES_WINDOW_ALPHA) ? 8 : EGL_DONT_CARE,
       EGL_DEPTH_SIZE,     (flags & ES_WINDOW_DEPTH) ? 8 : EGL_DONT_CARE,
       EGL_STENCIL_SIZE,   (flags & ES_WINDOW_STENCIL) ? 8 : EGL_DONT_CARE,
       EGL_SAMPLE_BUFFERS, (flags & ES_WINDOW_MULTISAMPLE) ? 1 : 0,
       // if EGL_KHR_create_context extension is supported, then we will use
       // EGL_OPENGL_ES3_BIT_KHR instead of EGL_OPENGL_ES2_BIT in the attribute list
       EGL_RENDERABLE_TYPE, getContextRenderableType(esContext->eglDisplay),
       EGL_NONE
    };

    // Choose config
    if (!eglChooseConfig(esContext->eglDisplay, attribList, &config, 1, &numConfigs))
    {
        printf("ERROR: eglChooseConfig failed! \n");
        return -1;
    }
    if (numConfigs < 1)
    {
        printf("ERROR: egl numConfigs < 1 ! \n");
        return -1;
    }

    // Create a surface
    esContext->eglSurface = eglCreateWindowSurface(esContext->eglDisplay, config, esContext->eglNativeWindow, NULL);
    if (esContext->eglSurface == EGL_NO_SURFACE)
    {
        printf("ERROR: eglCreateWindowSurface failed! \n");
        return -1;
    }

    // Create a GL context
    esContext->eglContext = eglCreateContext(esContext->eglDisplay, config, EGL_NO_CONTEXT, contextAttribs);
    if (esContext->eglContext == EGL_NO_CONTEXT)
    {
        printf("ERROR: eglCreateContext failed! \n");
        return -1;
    }

    // Make the context current
    if (!eglMakeCurrent(esContext->eglDisplay, esContext->eglSurface, esContext->eglSurface, esContext->eglContext))
    {
        printf("ERROR: eglMakeCurrent failed! \n");
        return -1;
    }

    return 0;
}

std::string readShaderFile(const char* filename)
{
    std::string str;
    std::ifstream infile(filename);
    if (infile) {
        infile.seekg(0, infile.end);
        size_t length = infile.tellg();
        infile.seekg(0, infile.beg);

        std::vector<char> buf(length + 1, 0);
        infile.read(buf.data(), length);
        infile.close();

        if (length) {
            printf("INFO: read shader file %s, size = %lld\n", filename, length);
        }
        else {
            printf("ERROR: failed to read shader file %s\n", filename);
            exit(-1);
        }

        return std::string(buf.data());
    }
    else {
        printf("ERROR: cannot open shader file %s\n", filename);
        exit(-1);
    }
}

int main(int argc, char *argv[])
{
    std::string inputImg, shaderName;
    ESContext esContextData = {};
    ESContext* esContext = &esContextData;
    esContext->width = 320;
    esContext->height = 240;
    esContext->userData = malloc(sizeof(UserData));

    if (argc == 1)
        inputImg = "dog.jpg";
    else if (argc == 2)
        inputImg = argv[1];
    else {
        printf("ERROR: Invalid cmd line! \n");
        return -1;
    }

    if (initEGL(esContext) != 0)
    {
        printf("ERROR: InitEGL failed! \n");
        return -1;
    }

    printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
    printf("GL_VERSION: %s\n", glGetString(GL_VERSION));
    printf("GL_SHADING_LANGUAGE_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    std::string vstr = readShaderFile("quad.vert");
    std::string fstr = readShaderFile("quad.frag");
    char* vert_str = (char*)vstr.c_str();
    char* frag_str = (char*)fstr.c_str();

    //========== shader program
    GLuint vert_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_shader, 1, &vert_str, NULL);
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
    glShaderSource(frag_shader, 1, &frag_str, NULL);
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
    int width, height, nrChannels;
    unsigned char *img_data = stbi_load(inputImg.c_str(), &width, &height, &nrChannels, 0);
    if (!img_data) {
        printf("ERROR: stbi_load image failed! \n");
        return -1;
    }
    printf("image width: %d, height: %d\n", width, height);

    std::vector<uint8_t> img_out(width * height * 4);
    //========== upload image to input texture
    GLuint texture_in;
    glGenTextures(1, &texture_in);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_in);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, img_data);
    stbi_image_free(img_data);

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

    float rv[3] = { 0.9611682f, -0.05449148f, -0.01015827f };
    float gv[3] = { 0.00256727f, 1.01033f, -0.00388904f };
    float bv[3] = { 0.004813198f, 0.01739324f, 1.192613f };
    unsigned int rvec3 = glGetUniformLocation(program, "u_RVec3");
    unsigned int gvec3 = glGetUniformLocation(program, "u_GVec3");
    unsigned int bvec3 = glGetUniformLocation(program, "u_BVec3");
    glUniform3fv(rvec3, 1, rv);
    glUniform3fv(gvec3, 1, gv);
    glUniform3fv(bvec3, 1, bv);

    //========== render
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    //========== read output image
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid *)img_out.data());

    stbi_write_png("out.png", width, height, 4, img_out.data(), width * 4);

    return 0;
}
