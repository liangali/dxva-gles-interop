#include <stdio.h>
#include <iostream>
#include <vector>
#include <fstream>

#include <windows.h>

#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../stb/stb_image.h"

#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stb/stb_image_write.h"

#ifdef WIN32
#define ESUTIL_API  __cdecl
#define ESCALLBACK  __cdecl
#else
#define ESUTIL_API
#define ESCALLBACK
#endif

#ifdef _WIN64
#define GWL_USERDATA GWLP_USERDATA
#endif

typedef struct
{
    // Handle to a program object
    GLuint programObject;
} UserData;


typedef struct ESContext ESContext;

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

    int imgWidth;
    int imgHeight;
    int nChannels;
    unsigned char* imgBuf;
    GLuint inputTexture;

    unsigned int VBO;
    unsigned int VAO;
    unsigned int EBO;

    /// Callbacks
    void (ESCALLBACK* drawFunc) (ESContext*);
    void (ESCALLBACK* shutdownFunc) (ESContext*);
    void (ESCALLBACK* keyFunc) (ESContext*, unsigned char, int, int);
    void (ESCALLBACK* updateFunc) (ESContext*, float deltaTime);
};

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

    switch (uMsg)
    {
    case WM_CREATE:
        break;

    case WM_PAINT:
    {
        ESContext* esContext = (ESContext*)(LONG_PTR)GetWindowLongPtr(hWnd, GWL_USERDATA);

        if (esContext && esContext->drawFunc)
        {
            esContext->drawFunc(esContext);
            eglSwapBuffers(esContext->eglDisplay, esContext->eglSurface);
        }

        if (esContext)
        {
            ValidateRect(esContext->eglNativeWindow, NULL);
        }
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_CHAR:
    {
        POINT      point;
        ESContext* esContext = (ESContext*)(LONG_PTR)GetWindowLongPtr(hWnd, GWL_USERDATA);

        GetCursorPos(&point);

        if (esContext && esContext->keyFunc)
            esContext->keyFunc(esContext, (unsigned char)wParam, (int)point.x, (int)point.y);
    }
    break;

    default:
        lRet = DefWindowProc(hWnd, uMsg, wParam, lParam);
        break;
    }

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

GLuint loadShader(GLenum type, const char* shaderSrc)
{
    GLuint shader;
    GLint compiled;

    // Create the shader object
    shader = glCreateShader(type);
    if (shader == 0)
    {
        return 0;
    }

    // Load the shader source
    glShaderSource(shader, 1, &shaderSrc, NULL);

    // Compile the shader
    glCompileShader(shader);

    // Check the compile status
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled)
    {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1)
        {
            char* infoLog = (char*)malloc(sizeof(char) * infoLen);
            glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
            printf("ERROR: failed in compiling shader: %s\n", infoLog);
            free(infoLog);
        }
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

int initShader(ESContext* esContext)
{
    UserData* userData = (UserData*)esContext->userData;
    GLuint vertexShader;
    GLuint fragmentShader;
    GLuint programObject;
    GLint linked;

    std::string vstr = readShaderFile("flip.vert");
    std::string fstr = readShaderFile("flip.frag");
    char* vShaderStr = (char*)vstr.c_str();
    char* fShaderStr = (char*)fstr.c_str();

    // Load the vertex/fragment shaders
    vertexShader = loadShader(GL_VERTEX_SHADER, vShaderStr);
    fragmentShader = loadShader(GL_FRAGMENT_SHADER, fShaderStr);

    // Create the program object
    programObject = glCreateProgram();
    if (programObject == 0)
    {
        printf("ERROR: glCreateProgram failed! \n");
        return -1;
    }

    glAttachShader(programObject, vertexShader);
    glAttachShader(programObject, fragmentShader);

    // Link the program
    glLinkProgram(programObject);

    // Check the link status
    glGetProgramiv(programObject, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLint infoLen = 0;
        glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1)
        {
            char* infoLog = (char*)malloc(sizeof(char) * infoLen);
            glGetProgramInfoLog(programObject, infoLen, NULL, infoLog);
            printf("ERROR: failed in linking program: %s\n", infoLog);
            free(infoLog);
        }
        glDeleteProgram(programObject);
        return -1;
    }

    // Store the program object
    userData->programObject = programObject;

    glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
    return 0;
}

int initResources(ESContext* ctx, const char* imgFile)
{
    float vertices[] = {
        // positions          // colors           // texture coords
         1.0f,  1.0f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f, // top right
         1.0f, -1.0f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f, // bottom right
        -1.0f, -1.0f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f, // bottom left
        -1.0f,  1.0f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f  // top left 
    };

    unsigned int indices[] = {
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
    };

    glGenVertexArrays(1, &ctx->VAO);
    glGenBuffers(1, &ctx->VBO);
    glGenBuffers(1, &ctx->EBO);

    glBindVertexArray(ctx->VAO);

    glBindBuffer(GL_ARRAY_BUFFER, ctx->VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // texture coord attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glGenTextures(1, &ctx->inputTexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ctx->inputTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, ctx->imgWidth, ctx->imgHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, ctx->imgBuf);
    stbi_image_free(ctx->imgBuf);
    glBindTexture(GL_TEXTURE_2D, 0);

    ctx->width = ctx->imgWidth;
    ctx->height = ctx->imgHeight;

    return 0;
}

void draw(ESContext* ctx)
{
    UserData* userData = (UserData*)ctx->userData;

    // set the viewport
    glViewport(0, 0, ctx->width, ctx->height);

    // clear frame buffer
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // bind Texture
    glBindTexture(GL_TEXTURE_2D, ctx->inputTexture);

    // use the program object
    glUseProgram(userData->programObject);

    // render
    glBindVertexArray(ctx->VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glFlush();
}

void shutdown(ESContext* esContext)
{
    UserData* userData = (UserData*)esContext->userData;

    glDeleteVertexArrays(1, &esContext->VAO);
    glDeleteBuffers(1, &esContext->VBO);
    glDeleteBuffers(1, &esContext->EBO);

    glDeleteProgram(userData->programObject);
}

void winLoop(ESContext* esContext)
{
    MSG msg = { 0 };
    int done = 0;
    DWORD lastTime = GetTickCount();

    while (!done)
    {
        int gotMsg = (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != 0);
        DWORD curTime = GetTickCount();
        float deltaTime = (float)(curTime - lastTime) / 1000.0f;
        lastTime = curTime;

        if (gotMsg)
        {
            if (msg.message == WM_QUIT)
            {
                done = 1;
            }
            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            SendMessage(esContext->eglNativeWindow, WM_PAINT, 0, 0);
        }

        // Call update function if registered
        if (esContext->updateFunc != NULL)
        {
            esContext->updateFunc(esContext, deltaTime);
        }
    }
}

int main(int argc, char** argv)
{
    const char* imgPath = (argc == 2) ? argv[1] : "dog.jpg";
    ESContext esContextData = {};
    ESContext* esContext = &esContextData;

    // load texture image
    esContextData.imgBuf = stbi_load(imgPath, &esContextData.imgWidth, &esContextData.imgHeight, &esContextData.nChannels, 0);
    if (!esContextData.imgBuf) {
        printf("ERROR: stbi_load image failed! \n");
        return -1;
    }
    printf("image width: %d, height: %d\n", esContextData.imgWidth, esContextData.imgHeight);

    esContext->width = esContextData.imgWidth;
    esContext->height = esContextData.imgHeight;
    esContext->userData = malloc(sizeof(UserData));

    if (initEGL(esContext) != 0)
    {
        printf("ERROR: InitEGL failed! \n");
        return -1;
    }

    if (initShader(esContext) != 0)
    {
        printf("ERROR: InitShader failed! \n");
        return -1;
    }

    if (initResources(esContext, imgPath) != 0)
    {
        printf("ERROR: initResources failed! \n");
        return -1;
    }

    esContext->drawFunc = draw;
    esContext->shutdownFunc = shutdown;

    winLoop(esContext);

    if (esContext->shutdownFunc != NULL)
    {
        esContext->shutdownFunc(esContext);
    }

    if (esContext->userData != NULL)
    {
        free(esContext->userData);
    }

    printf("done\n");
    return 0;
}
