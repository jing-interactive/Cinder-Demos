// Copyright (c) 2011-2018 Sundog Software LLC. All rights reserved worldwide.
#pragma once
#include "glad/glad.h"

// A simple sky box rendering class for OpenGL 2.0 contexts
// Renders an infinitely distant sky box suitable for rendering
// as the last thing in the frame.
class SkyBox
{
public:
    SkyBox();

    // Frees up the skybox's resources
    ~SkyBox();

    // Creates the skybox's resources
    bool Create();

    // Draw the skybox using the view/projection matrix passed in
    // (which should include the camera rotation but not the camera
    // translation in order to position the skybox at the camera)
    void Draw(const GLdouble *viewProj);

    // Retrieves the underlying cube map, useful for environment mapping
    GLuint GetCubemap() const {
        return cubeMap;
    }

    bool SetCubeMap(const char* cubeMap);
private:
    bool CreateBuffers();
    bool LoadShader();

    GLuint cubeMap;
    GLuint vboID, idxID;
    GLhandleARB vertexShader, fragmentShader, shaderProgram;
    GLint viewProjLoc, cubeMapLoc;
};