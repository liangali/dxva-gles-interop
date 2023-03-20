#include <stdio.h>
#include <iostream>

#include <windows.h>

#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

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

int main()
{
    return 0;
}

