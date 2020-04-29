#include <stdio.h>

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
