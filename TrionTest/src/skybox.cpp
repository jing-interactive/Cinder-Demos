// Copyright (c) 2011-2018 Sundog Software LLC. All rights reserved worldwide.

// This skybox class is written for OpenGL 2.0. Modifying it to OpenGL 3.2 or higher wouldn't be
// that hard; you'd need to introduce a vertex array object and make some small changes to the
// shaders.

#include "skybox.h"
#include "TGALoader.h"

#include <stdlib.h>
#include <stdio.h>

#include <string>

SkyBox::SkyBox() : vboID(0), idxID(0), vertexShader(0), fragmentShader(0), shaderProgram(0), cubeMap(0)
{
}

SkyBox::~SkyBox()
{
    if (vboID) {
        glDeleteBuffersARB(1, &vboID);
    }

    if (idxID) {
        glDeleteBuffersARB(1, &idxID);
    }

    if (cubeMap) {
        glDeleteTextures(1, &cubeMap);
    }

    if (vertexShader && shaderProgram) {
        glDetachObjectARB(shaderProgram, vertexShader);
        glDeleteObjectARB(vertexShader);
    }

    if (fragmentShader && shaderProgram) {
        glDetachObjectARB(shaderProgram, fragmentShader);
        glDeleteObjectARB(fragmentShader);
    }

    if (shaderProgram) {
        glDeleteObjectARB(shaderProgram);
    }
}

bool SkyBox::Create()
{
    return (CreateBuffers() && LoadShader());
}

// Each vertex only contains x,y,z position values, which
// double as the texture coordinates.
struct Vertex {
    float x, y, z;
};

// The 8 points of the sky box
Vertex vertices[8] = {
    { 1,  1, -1 },
    {-1,  1, -1 },
    {-1, -1, -1 },
    { 1, -1, -1 },

    { 1,  1,  1 },
    {-1,  1,  1 },
    {-1, -1,  1 },
    { 1, -1,  1 }
};

// 2 triangles each for each of the 6 faces of the
// sky box, all winding inward.
unsigned short indices[] = {
    0, 1, 3,
    1, 2, 3,
    0, 4, 1,
    1, 4, 5,
    5, 4, 7,
    5, 7, 6,
    1, 5, 6,
    1, 6, 2,
    0, 3, 7,
    0, 7, 4,
    2, 6, 7,
    2, 7, 3
};

// Create our vertex and index bufer objects.
bool SkyBox::CreateBuffers()
{
    if (GLAD_GL_ARB_vertex_buffer_object) {
        glGenBuffersARB(1, &vboID);
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, vboID);
        glBufferDataARB(GL_ARRAY_BUFFER_ARB, 8 * sizeof(Vertex),
                        (void *)vertices, GL_STATIC_DRAW_ARB);

        glGenBuffersARB(1, &idxID);
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, idxID);
        glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 12 * 3 * sizeof(GLushort),
                        (void *)indices, GL_STATIC_DRAW_ARB);

        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
    }

    return true;
}

static std::string MakeFaceFileName(const std::string& dir, int face)
{
    if (face==0)   {
        return dir + "posX.tga";
    }
    if (face == 1) {
        return dir + "negX.tga";
    }
    if (face == 2) {
        return dir + "posY.tga";
    }
    if (face == 3) {
        return dir + "negY.tga";
    }

    // PPP: flip the z's (OpenGL neg z is into screen)
    if (face == 4) {
        return dir + "negZ.tga";
    }
    if (face == 5) {
        return dir + "posZ.tga";
    }

    return "";
}

// Load a sky box from six TGA files named as per below
bool SkyBox::SetCubeMap(const char* cubeMapDir)
{
    TGALoader loader;

    if (cubeMap) {
        glDeleteTextures(1, &cubeMap);
        cubeMap = 0;
    }

    glGenTextures(1, &cubeMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


    for (int face = 0; face < 6; face++) {

        std::string fileName = MakeFaceFileName(cubeMapDir, face);
        if (loader.Load(fileName.c_str())) {

            GLuint cubeFace = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
            // PPP: for some reason +Y doesn't need to be flipped (maybe something
            // to do with z is up for us?)
            if (cubeFace != GL_TEXTURE_CUBE_MAP_POSITIVE_Y) {
                // PPP: tga seems flipped?
                loader.flipVertical();
                loader.flipHorizontal();
            }

            glTexImage2D(cubeFace, 0, GL_RGBA8, loader.GetWidth(), loader.GetHeight(), 0,
                         loader.GetBitsPerPixel() == 32 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, loader.GetPixels());
            loader.FreePixels();
        } else {
            printf("Could not load %s\n", fileName.c_str());
            return false;
        }
    }

    return true;
}

// The vertex shader moves the skybox down a bit to cover up the fact that the texture we use
// doesn't go all the way to the horizon, and it hacks the final position such that it
// has a depth value of 1.0 (by setting z and w to be the same.) The 3D texture coordinates
// for the cube map are derived from the vertex positions of the box.
const char *vertexShaderSource =
    "uniform mat4 viewProj; \n"

    "varying vec3 texcoord; \n"

    "void main() \n"
    "{ \n"
    "   vec4 pos = vec4(gl_Vertex.x, gl_Vertex.y, gl_Vertex.z, 0.0); \n"
    "   gl_Position = (viewProj * pos).xyww; \n"
    "   texcoord = gl_Vertex.xyz; \n"
    "} \n";

// Then, we just do a cube map lookup in the fragment shader.
const char *fragShaderSource =
    "uniform samplerCube cubeMap; \n"

    "varying vec3 texcoord;\n"

    "void main() \n"
    "{ \n"
    "   gl_FragColor = textureCube(cubeMap, texcoord); \n"
    "} \n";

// Print any compilation errors
static void PrintGLSLInfoLog(GLhandleARB obj)
{
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;

    printf("GLSL error detected!\n");

    glGetObjectParameterivARB(obj, GL_OBJECT_INFO_LOG_LENGTH_ARB,
                              &infologLength);

    if (infologLength > 0) {
        infoLog = (char *)malloc(infologLength);
        glGetInfoLogARB(obj, infologLength, &charsWritten, infoLog);
        printf("%s\n",infoLog);
        free(infoLog);
    }
}

// Loads and links our vertex and fragment shaders.
bool SkyBox::LoadShader()
{
    vertexShader = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
    glShaderSourceARB(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShaderARB(vertexShader);

    GLint ok;
    glGetObjectParameterivARB(vertexShader, GL_OBJECT_COMPILE_STATUS_ARB, &ok);
    if (!ok) {
        PrintGLSLInfoLog(vertexShader);
        return false;
    }

    fragmentShader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
    glShaderSourceARB(fragmentShader, 1, &fragShaderSource, NULL);
    glCompileShaderARB(fragmentShader);

    glGetObjectParameterivARB(fragmentShader, GL_OBJECT_COMPILE_STATUS_ARB, &ok);
    if (!ok) {
        PrintGLSLInfoLog(fragmentShader);
        return false;
    }

    shaderProgram = glCreateProgramObjectARB();

    glAttachObjectARB(shaderProgram, vertexShader);
    glAttachObjectARB(shaderProgram, fragmentShader);
    glLinkProgramARB(shaderProgram);

    viewProjLoc = glGetUniformLocationARB(shaderProgram, "viewProj");
    cubeMapLoc = glGetUniformLocationARB(shaderProgram, "cubeMap");

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        //printf("%s\n", (const char *)gluErrorString(err));
        return false;
    }

    return true;
}

// Draw the infinitely distant sky box.
void SkyBox::Draw(const GLdouble *viewProj)
{
    glUseProgramObjectARB(shaderProgram);

    GLfloat m[16];
    for (int i = 0; i < 16; i++) m[i] = (float)viewProj[i];
    glUniformMatrix4fvARB(viewProjLoc, 1, GL_FALSE, m);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap);
    glUniform1iARB(cubeMapLoc, 1);

    glBindBufferARB(GL_ARRAY_BUFFER_ARB, vboID);
    glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, idxID);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, 0);

    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glDrawElements(GL_TRIANGLES, 3 * 12, GL_UNSIGNED_SHORT, 0);

    glDisableClientState(GL_VERTEX_ARRAY);

    glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
}
