#ifndef TINY_C_GAMES_UTILS_
#define TINY_C_GAMES_UTILS_

#include <stdio.h>
#include <stdlib.h>

#define GL3_PROTOTYPES 1

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define SWAP(a,b) {int *x = &(a); int *y = &(b); int t = *x; *x = *y; *y = t;}
#define CLAMP(lo,x,hi) ((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))
#define MOD(a,b) (((a) % (b) + (b)) % (b))

// lerp between values a and b, t = 0..1
float lerp(float t, float a, float b)
{
        return a * (1.f - t) + b * t;
}

// remap a value in the range fromlo..fromhi to tolo..tohi with clamping
float remap(float val, float fromlo, float fromhi, float tolo, float tohi)
{
        val = CLAMP(fromlo, val, fromhi);
        val = (val - fromlo) / (fromhi - fromlo);
        val = val * (tohi - tolo) + tolo;
        return val;
}

// remap exclusively values in the range fromlo..fromhi to tolo..tohi, leaving other values untouched
float excl_remap(float val, float fromlo, float fromhi, float tolo, float tohi)
{
        if (val < fromlo || val > fromhi)
                return val;
        val = (val - fromlo) / (fromhi - fromlo);
        val = val * (tohi - tolo) + tolo;
        return val;
}

int check_shader_errors(GLuint shader, char *name)
{
        GLint success;
        GLchar log[1024];
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (success) return 0;
        glGetShaderInfoLog(shader, 1024, NULL, log);
        fprintf(stderr, "ERROR in %s shader program: %s\n", name, log);
        exit(1);
        return 1;
}

int check_program_errors(GLuint shader, char *name)
{
        GLint success;
        GLchar log[1024];
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (success) return 0;
        glGetProgramInfoLog(shader, 1024, NULL, log);
        fprintf(stderr, "ERROR in %s shader: %s\n", name, log);
        exit(1);
        return 1;
}

// please free() the returned string
char *file2str(char *filename)
{
        FILE *f;

        #if defined(_MSC_VER) && _MSC_VER >= 1400
                if (fopen_s(&f, filename, "r"))
                        f = NULL;
        #else
                f = fopen(filename, "r");
        #endif

        if (!f) goto bad;
        fseek(f, 0, SEEK_END);
        size_t sz = ftell(f);
        rewind(f);
        char *buf = calloc(sz + 1, sizeof *buf);
        if (fread(buf, 1, sz, f) != sz) goto bad;
        fclose(f);
        return buf;

        bad:
        fprintf(stderr, "Failed to open/read %s\n", filename);
        return NULL;
}

unsigned int file2shader(unsigned int type, char *filename)
{
        char *code = file2str(filename);
        unsigned int id = glCreateShader(type);
        glShaderSource(id, 1, (const char *const *)&code, NULL);
        glCompileShader(id);
        check_shader_errors(id, filename);
        free(code);
        return id;
}

#endif
