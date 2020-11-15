#include <math.h>

void BuildPerspProjMat(double *m, double fov, double aspect, double znear, double zfar)
{
    double ymax = znear * tan(fov * (3.14159265 / 360.0));
    double ymin = -ymax;
    double xmax = ymax * aspect;
    double xmin = ymin * aspect;
    double width = xmax - xmin;
    double height = ymax - ymin;
    double depth = zfar - znear;
    double q = -(zfar + znear) / depth;
    double qn = -2 * (zfar * znear) / depth;
    double w = 2 * znear / width;
    double h = 2 * znear / height;
    m[0]  = w;
    m[1]  = 0;
    m[2]  = 0;
    m[3]  = 0;
    m[4]  = 0;
    m[5]  = h;
    m[6]  = 0;
    m[7]  = 0;
    m[8]  = 0;
    m[9]  = 0;
    m[10] = q;
    m[11] = -1;
    m[12] = 0;
    m[13] = 0;
    m[14] = qn;
    m[15] = 0;
}

void BuildIdentityMat(double *m)
{
    m[0] =  1;
    m[1] = 0;
    m[2] =  0;
    m[3] =  0;
    m[4] =  0;
    m[5] = 1;
    m[6] =  0;
    m[7] =  0;
    m[8] =  0;
    m[9] = 0;
    m[10] = 1;
    m[11] = 0;
    m[12] = 0;
    m[13] = 0;
    m[14] = 0;
    m[15] = 1;
}

void Translate( double *m, double x, double y, double z )
{
    m[12] = m[0] * x + m[4] * y + m[8]  * z + m[12];
    m[13] = m[1] * x + m[5] * y + m[9]  * z + m[13];
    m[14] = m[2] * x + m[6] * y + m[10] * z + m[14];
    m[15] = m[3] * x + m[7] * y + m[11] * z + m[15];
}

void Multiply( double *mOut, double *m1, double *m2)
{
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            mOut[row * 4 + col] =
                m1[row * 4 + 0] * m2[0 * 4 + col] +
                m1[row * 4 + 1] * m2[1 * 4 + col] +
                m1[row * 4 + 2] * m2[2 * 4 + col] +
                m1[row * 4 + 3] * m2[3 * 4 + col];
        }
    }
}

void ApplyYaw( double *m, double yaw)
{
    m[0] = cos(yaw);
    m[1] = 0;
    m[2] = sin(yaw);
    m[3] = 0;
    m[4] = 0;
    m[5] = 1;
    m[6] = 0;
    m[7] = 0;
    m[8] = -sin(yaw);
    m[9] = 0;
    m[10] = cos(yaw);
    m[11] = 0;
    m[12] = 0;
    m[13] = 0;
    m[14] = 0;
    m[15] = 1;
}

void EulerMatrix( double m[16], double yaw, double pitch, double roll )
{
    double rx = roll, ry = pitch, rz = yaw;
    double (*mat)[4] = (double (*)[4]) ( m );
    // Build rotation matrix
    // the matrix looks like this:
    //  r1 - (r4 * sin(x))     r2 + (r3 * sin(x))   -cos(x) * sin(y)
    //  -cos(x) * sin(z)       cos(x) * cos(z)      sin(x)
    //  r3 + (r2 * sin(x))     r4 - (r1 * sin(x))   cos(x) * cos(y)
    //
    // where:
    //  r1 = cos(y) * cos(z)
    //  r2 = cos(y) * sin(z)
    //  r3 = sin(y) * cos(z)
    //  r4 = sin(y) * sin(z)
    double cx = cos(rx), sx = sin(rx);
    double cy = cos(ry), sy = sin(ry);
    double cz = cos(rz), sz = sin(rz);
    double r1 = cy * cz;
    double r2 = cy * sz;
    double r3 = sy * cz;
    double r4 = sy * sz;

    mat[0][0] = r1 - (r4 * sx);
    mat[0][1] = r2 + (r3 * sx);
    mat[0][2] = -cx * sy;
    mat[1][0] = -cx * sz;
    mat[1][1] = cx * cz;
    mat[1][2] = sx;
    mat[2][0] = r3 + (r2 * sx);
    mat[2][1] = r4 - (r1 * sx);
    mat[2][2] = cx * cy;

    mat[0][3] = mat[1][3] = mat[2][3] = 0.0;
    mat[3][0] = mat[3][1] = mat[3][2] = 0.0;
    mat[3][3] = 1.0;
}

