#include <stdio.h>
#include <math.h>
#include <memory.h>
#include <float.h>

// Our unfortunate matrix storage:
//
//   0   4   8  12
//   1   5   9  13
//   2   6  10  14
//   3   7  11  15

void mat4_multiply(float *result, float *a, float *b)
{
        result[ 0] = a[ 0] * b[ 0] + a[ 4] * b[ 1] + a[ 8] * b[ 2] + a[12] * b[ 3];
        result[ 1] = a[ 1] * b[ 0] + a[ 5] * b[ 1] + a[ 9] * b[ 2] + a[13] * b[ 3];
        result[ 2] = a[ 2] * b[ 0] + a[ 6] * b[ 1] + a[10] * b[ 2] + a[14] * b[ 3];
        result[ 3] = a[ 3] * b[ 0] + a[ 7] * b[ 1] + a[11] * b[ 2] + a[15] * b[ 3];

        result[ 4] = a[ 0] * b[ 4] + a[ 4] * b[ 5] + a[ 8] * b[ 6] + a[12] * b[ 7];
        result[ 5] = a[ 1] * b[ 4] + a[ 5] * b[ 5] + a[ 9] * b[ 6] + a[13] * b[ 7];
        result[ 6] = a[ 2] * b[ 4] + a[ 6] * b[ 5] + a[10] * b[ 6] + a[14] * b[ 7];
        result[ 7] = a[ 3] * b[ 4] + a[ 7] * b[ 5] + a[11] * b[ 6] + a[15] * b[ 7];

        result[ 8] = a[ 0] * b[ 8] + a[ 4] * b[ 9] + a[ 8] * b[10] + a[12] * b[11];
        result[ 9] = a[ 1] * b[ 8] + a[ 5] * b[ 9] + a[ 9] * b[10] + a[13] * b[11];
        result[10] = a[ 2] * b[ 8] + a[ 6] * b[ 9] + a[10] * b[10] + a[14] * b[11];
        result[11] = a[ 3] * b[ 8] + a[ 7] * b[ 9] + a[11] * b[10] + a[15] * b[11];

        result[12] = a[ 0] * b[12] + a[ 4] * b[13] + a[ 8] * b[14] + a[12] * b[15];
        result[13] = a[ 1] * b[12] + a[ 5] * b[13] + a[ 9] * b[14] + a[13] * b[15];
        result[14] = a[ 2] * b[12] + a[ 6] * b[13] + a[10] * b[14] + a[14] * b[15];
        result[15] = a[ 3] * b[12] + a[ 7] * b[13] + a[11] * b[14] + a[15] * b[15];
}

void mat4_f3_multiply(float *result, float *m, float x, float y, float z)
{
        result[ 0] = m[ 0] * x + m[ 4] * y + m[ 8] * z + m[12] * 1.f;
        result[ 1] = m[ 1] * x + m[ 5] * y + m[ 9] * z + m[13] * 1.f;
        result[ 2] = m[ 2] * x + m[ 6] * y + m[10] * z + m[14] * 1.f;
        result[ 3] = m[ 3] * x + m[ 7] * y + m[11] * z + m[15] * 1.f;
}

void mat4_print(float *a)
{
        printf("%4.4f %4.4f %4.4f %4.4f\n", a[ 0], a[ 4], a[ 8], a[12]);
        printf("%4.4f %4.4f %4.4f %4.4f\n", a[ 1], a[ 5], a[ 9], a[13]);
        printf("%4.4f %4.4f %4.4f %4.4f\n", a[ 2], a[ 6], a[10], a[14]);
        printf("%4.4f %4.4f %4.4f %4.4f\n", a[ 3], a[ 7], a[11], a[15]);
}

#define NO_PITCH FLT_MAX

// create a view matrix by point+angle or point to point
void lookit(float *out_matrix, float *f, float eye0, float eye1, float eye2,
                float pitch, float yaw)
{
        if (pitch == NO_PITCH)
        {
                f[0] -= eye0;
                f[1] -= eye1;
                f[2] -= eye2;
                float dist = sqrtf(f[0]*f[0] + f[1]*f[1] + f[2]*f[2]);
                f[0] /= dist;
                f[1] /= dist;
                f[2] /= dist;
        }
        else
        {
                f[0] = cosf(pitch) * sinf(yaw);
                f[1] = sinf(pitch);
                f[2] = cosf(pitch) * cosf(yaw);
        }
        float wing0, wing1, wing2;
        wing0 = -cosf(yaw);
        wing1 = 0;
        wing2 = sinf(yaw);
        float up0, up1, up2;
        up0 = f[1]*wing2 - f[2]*wing1;
        up1 = f[2]*wing0 - f[0]*wing2;
        up2 = f[0]*wing1 - f[1]*wing0;
        float upm = sqrtf(up0*up0 + up1*up1 + up2*up2);
        up0 /= upm;
        up1 /= upm;
        up2 /= upm;
        float s0, s1, s2;
        s0 = f[1]*up2 - f[2]*up1;
        s1 = f[2]*up0 - f[0]*up2;
        s2 = f[0]*up1 - f[1]*up0;
        float sm = sqrtf(s0*s0 + s1*s1 + s2*s2);
        float zz0, zz1, zz2;
        zz0 = s0/sm;
        zz1 = s1/sm;
        zz2 = s2/sm;
        float u0, u1, u2;
        u0 = zz1*f[2] - zz2*f[1];
        u1 = zz2*f[0] - zz0*f[2];
        u2 = zz0*f[1] - zz1*f[0];
        float buf[] = {
                s0, u0, -f[0], 0,
                s1, u1, -f[1], 0,
                s2, u2, -f[2], 0,
                 0,  0,     0, 1
        };
        memcpy(out_matrix, buf, sizeof buf);
}

void translate(float *mat, float x, float y, float z)
{
        mat[12] = (mat[0] * x) + (mat[4] * y) + (mat[ 8] * z);
        mat[13] = (mat[1] * x) + (mat[5] * y) + (mat[ 9] * z);
        mat[14] = (mat[2] * x) + (mat[6] * y) + (mat[10] * z);
}

float distance3d(float x, float y, float z, float a, float b, float c)
{
        return sqrtf((x-a) * (x-a) + (y-b) * (y-b) + (z-c) * (z-c));
}
