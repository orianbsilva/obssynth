/**
 * Minimal OpenGL 2.0+ Function Loader
 * Loads GL extension functions needed by NanoVG.
 * Cross-platform: Windows (wglGetProcAddress) + Linux (glXGetProcAddress)
 *
 * Public domain / zlib license
 */
#ifndef GLAD_H
#define GLAD_H

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>
#else
#include <GL/gl.h>
#include <GL/glx.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    /* ========== GL type definitions ========== */
    typedef char GLchar;
    typedef ptrdiff_t GLsizeiptr;
    typedef ptrdiff_t GLintptr;

/* ========== GL constants ========== */
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_ARRAY_BUFFER 0x8889
#define GL_STREAM_DRAW 0x88E0
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_TEXTURE0 0x84C0
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_GENERATE_MIPMAP 0x8191
#define GL_UNIFORM_BUFFER 0x8A11
#define GL_INCR_WRAP 0x8507
#define GL_DECR_WRAP 0x8508
#define GL_FUNC_ADD 0x8006
#define GL_FUNC_REVERSE_SUBTRACT 0x800B
#define GL_SRC_ALPHA_SATURATE 0x0308
#endif

#ifndef GL_STENCIL_ATTACHMENT
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_STENCIL_ATTACHMENT 0x8D20
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_STENCIL_INDEX8 0x8D48
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif

    /* ========== Function pointer typedefs ========== */
    typedef GLuint(APIENTRY *PFNGLCREATESHADERPROC)(GLenum type);
    typedef void(APIENTRY *PFNGLDELETESHADERPROC)(GLuint shader);
    typedef void(APIENTRY *PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
    typedef void(APIENTRY *PFNGLCOMPILESHADERPROC)(GLuint shader);
    typedef void(APIENTRY *PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint *params);
    typedef void(APIENTRY *PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
    typedef GLuint(APIENTRY *PFNGLCREATEPROGRAMPROC)(void);
    typedef void(APIENTRY *PFNGLDELETEPROGRAMPROC)(GLuint program);
    typedef void(APIENTRY *PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
    typedef void(APIENTRY *PFNGLLINKPROGRAMPROC)(GLuint program);
    typedef void(APIENTRY *PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint *params);
    typedef void(APIENTRY *PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
    typedef void(APIENTRY *PFNGLUSEPROGRAMPROC)(GLuint program);
    typedef GLint(APIENTRY *PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar *name);
    typedef void(APIENTRY *PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
    typedef void(APIENTRY *PFNGLUNIFORM2FVPROC)(GLint location, GLsizei count, const GLfloat *value);
    typedef void(APIENTRY *PFNGLUNIFORM4FVPROC)(GLint location, GLsizei count, const GLfloat *value);
    typedef GLint(APIENTRY *PFNGLGETATTRIBLOCATIONPROC)(GLuint program, const GLchar *name);
    typedef void(APIENTRY *PFNGLBINDATTRIBLOCATIONPROC)(GLuint program, GLuint index, const GLchar *name);

    typedef void(APIENTRY *PFNGLGENBUFFERSPROC)(GLsizei n, GLuint *buffers);
    typedef void(APIENTRY *PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint *buffers);
    typedef void(APIENTRY *PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
    typedef void(APIENTRY *PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);

    typedef void(APIENTRY *PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
    typedef void(APIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
    typedef void(APIENTRY *PFNGLDISABLEVERTEXATTRIBARRAYPROC)(GLuint index);

    typedef void(APIENTRY *PFNGLACTIVETEXTUREPROC)(GLenum texture);
    typedef void(APIENTRY *PFNGLGENERATEMIPMAPPROC)(GLenum target);

    typedef void(APIENTRY *PFNGLSTENCILFUNCSEPARATEPROC)(GLenum face, GLenum func, GLint ref, GLuint mask);
    typedef void(APIENTRY *PFNGLSTENCILOPSEPARATEPROC)(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
    typedef void(APIENTRY *PFNGLSTENCILMASKSEPARATEPROC)(GLenum face, GLuint mask);
    typedef void(APIENTRY *PFNGLBLENDFUNCSEPARATEPROC)(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
    typedef void(APIENTRY *PFNGLBLENDEQUATIONPROC)(GLenum mode);

    typedef void(APIENTRY *PFNGLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint *framebuffers);
    typedef void(APIENTRY *PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei n, const GLuint *framebuffers);
    typedef void(APIENTRY *PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
    typedef GLenum(APIENTRY *PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum target);
    typedef void(APIENTRY *PFNGLFRAMEBUFFERRENDERBUFFERPROC)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
    typedef void(APIENTRY *PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);

    typedef void(APIENTRY *PFNGLGENRENDERBUFFERSPROC)(GLsizei n, GLuint *renderbuffers);
    typedef void(APIENTRY *PFNGLDELETERENDERBUFFERSPROC)(GLsizei n, const GLuint *renderbuffers);
    typedef void(APIENTRY *PFNGLBINDRENDERBUFFERPROC)(GLenum target, GLuint renderbuffer);
    typedef void(APIENTRY *PFNGLRENDERBUFFERSTORAGEPROC)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);

    typedef void(APIENTRY *PFNGLBINDBUFFERRANGEPROC)(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
    typedef GLuint(APIENTRY *PFNGLGETUNIFORMBLOCKINDEXPROC)(GLuint program, const GLchar *uniformBlockName);
    typedef void(APIENTRY *PFNGLUNIFORMBLOCKBINDINGPROC)(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);

    /* ========== Function pointer declarations ========== */
    extern PFNGLCREATESHADERPROC glCreateShader;
    extern PFNGLDELETESHADERPROC glDeleteShader;
    extern PFNGLSHADERSOURCEPROC glShaderSource;
    extern PFNGLCOMPILESHADERPROC glCompileShader;
    extern PFNGLGETSHADERIVPROC glGetShaderiv;
    extern PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
    extern PFNGLCREATEPROGRAMPROC glCreateProgram;
    extern PFNGLDELETEPROGRAMPROC glDeleteProgram;
    extern PFNGLATTACHSHADERPROC glAttachShader;
    extern PFNGLLINKPROGRAMPROC glLinkProgram;
    extern PFNGLGETPROGRAMIVPROC glGetProgramiv;
    extern PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
    extern PFNGLUSEPROGRAMPROC glUseProgram;
    extern PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
    extern PFNGLUNIFORM1IPROC glUniform1i;
    extern PFNGLUNIFORM2FVPROC glUniform2fv;
    extern PFNGLUNIFORM4FVPROC glUniform4fv;
    extern PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
    extern PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation;

    extern PFNGLGENBUFFERSPROC glGenBuffers;
    extern PFNGLDELETEBUFFERSPROC glDeleteBuffers;
    extern PFNGLBINDBUFFERPROC glBindBuffer;
    extern PFNGLBUFFERDATAPROC glBufferData;

    extern PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
    extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
    extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;

    extern PFNGLACTIVETEXTUREPROC glActiveTexture;
    extern PFNGLGENERATEMIPMAPPROC glGenerateMipmap;

    extern PFNGLSTENCILFUNCSEPARATEPROC glStencilFuncSeparate;
    extern PFNGLSTENCILOPSEPARATEPROC glStencilOpSeparate;
    extern PFNGLSTENCILMASKSEPARATEPROC glStencilMaskSeparate;
    extern PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
    extern PFNGLBLENDEQUATIONPROC glBlendEquation;

    extern PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
    extern PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
    extern PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
    extern PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
    extern PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;
    extern PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;

    extern PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers;
    extern PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers;
    extern PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer;
    extern PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage;

    extern PFNGLBINDBUFFERRANGEPROC glBindBufferRange;
    extern PFNGLGETUNIFORMBLOCKINDEXPROC glGetUniformBlockIndex;
    extern PFNGLUNIFORMBLOCKBINDINGPROC glUniformBlockBinding;

    /* VAO (Vertex Array Objects) */
    typedef void(APIENTRY *PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint *arrays);
    typedef void(APIENTRY *PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint *arrays);
    typedef void(APIENTRY *PFNGLBINDVERTEXARRAYPROC)(GLuint array);

    extern PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
    extern PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
    extern PFNGLBINDVERTEXARRAYPROC glBindVertexArray;

    /* ========== Loader function ========== */
    /* Call this AFTER creating an OpenGL context */
    int gladLoadGL(void);

#ifdef __cplusplus
}
#endif

#endif /* GLAD_H */
