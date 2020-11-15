// Triton OpenGL Sample Project
// Illustrates integration of Triton with an OpenGL-based application using free//glut

// Copyright (c) 2011-2016 Sundog Software LLC. All rights reserved worldwide.

//#define UNDERWATER

// The Triton.h header includes everything you need
#include "Licenses.h"
#include  "../../../Triton SDK/Public Headers/Triton.h"
#include "glad/glad.h"

// Various platform-specific headers:
#if defined(__APPLE__)
# include <//glut///glut.h>
# include <sys/timeb.h>
#elif defined(_WIN32)
# include <windows.h>
# ifndef WGL_EXT_swap_control
typedef int (APIENTRY * PFNWGLSWAPINTERVALEXTPROC)(int);
# endif
#else
# include "GL/free//glut.h"
# include <GL/glx.h>
# include <sys/timeb.h>
# ifndef GLX_SGI_swap_control
typedef int ( * PFNGLXSWAPINTERVALSGIPROC) (int interval);
# endif
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <cassert>
#include <sstream>
#include "skybox.h"

// The three main Triton objects you need:
static Triton::ResourceLoader *resourceLoader = 0;
static Triton::Environment *environment = 0;
static Triton::Ocean *ocean = 0;

// Our simple skybox class that exposes an environment map, which
// Triton can use for water reflections.
static SkyBox *skyBox = 0;

// Camera properties:
#ifdef UNDERWATER
static double yaw = 2 * TRITON_PI, pitch = 0, roll = -0.1;
static double x = 0, y = -15, z = 15;
#else
static double yaw = 0, pitch = 0, roll = 0.1;
static double x = 0, y = 15, z = 15;
#endif
static unsigned long startTime = 0, frameCount = 0;

// Function prototypes from matrixutils.cpp:
extern void BuildPerspProjMat(double *m, double fov, double aspect, double znear, double zfar);
extern void BuildIdentityMat(double *m);
extern void Translate( GLdouble *m, GLdouble x, GLdouble y, GLdouble z );
extern void Multiply( GLdouble *mOut, GLdouble *m1, GLdouble *m2);
extern void ApplyYaw( GLdouble *m, GLdouble yaw);
extern void EulerMatrix( double m[16], double yaw, double pitch, double roll );

// Forward declarations
unsigned long getMilliseconds();
void SetVSync(int interval);

// Create the Triton objects at startup, once we have a valid GL context in place
bool InitTriton()
{
    // We use the default resource loader that just loads files from disk. You'll need
    // to redistribute the resources folder if using this. You can also extend the
    // ResourceLoader class to hook into your own resource manager if you wish.
    resourceLoader = new Triton::ResourceLoader("../../../Triton SDK/Resources");
    // Create an environment for the water, with a flat-Earth coordinate system with Y
    // pointing up and using an OpenGL 2.0 capable context.
    environment = new Triton::Environment();
    Triton::EnvironmentError err = environment->Initialize(Triton::FLAT_YUP,
                                   Triton::OPENGL_2_0, resourceLoader);

    if (err != Triton::SUCCEEDED) {
#ifdef _WIN32
        ::MessageBoxA(NULL, "Failed to initialize Triton - is the resource path passed in to "
                      "the ResouceLoader constructor valid?", "Triton error", MB_OK | MB_ICONEXCLAMATION);
#else
        printf("Failed to initialize Triton.\n");
#endif
        return false;
    }

    // Substitute your own license name and code, otherwise the app will terminate after
    // 5 minutes. Visit www.sundog-soft.com to purchase a license if you're so inclined.
    environment->SetLicenseCode(TRITON_LICENSE_NAME, TRITON_LICENSE_CODE);

    // Set up wind of 15 m/s blowing roughly from the North
    Triton::WindFetch wf;
    wf.SetWind(15.0, 0.3);
    environment->AddWindFetch(wf);

    // Finally, create the Ocean object using the environment we've created.
    // If NULL is returned, something's wrong - enable the enable-debug-messages option
    // in resources/triton.config to get more details on the problem.
    ocean = Triton::Ocean::Create(environment, Triton::JONSWAP, false, false, Triton::BEST);

    if (ocean) {
        printf("Using FFT Technique: %s\n", ocean->GetFFTName());

        //Disabling spray will improve performance, if you don't need it.
        //ocean->EnableSpray(false);
    }

    return (ocean != NULL);
}

// Compute the matrices for the camera and for rendering our sky box, and
// pass them into Triton so it can properly position and orient the water.
// We compute the projection and view matrices "by hand" to make this sample
// more useful for OpenGL 3.2+ developers.
void GrabMatrices(double *skyBoxMatrix)
{
    double mv[16], proj[16];
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    double aspect = (double)vp[2] / (double)vp[3];

    // Compute a perspective projection matrix
    BuildPerspProjMat(proj, 30.0, aspect, 10.0, 100000.0);

    // Spin the camera to face the sun in our skybox texture
    BuildIdentityMat(mv);
    // ApplyYaw(mv, yaw);
    EulerMatrix( mv, yaw, pitch, roll );

    // Capture the viewproj matrix before we apply translation, for use
    // in positioning our sky box:
    Multiply(skyBoxMatrix, mv, proj);

    // Apply the camera translation
    Translate(mv, -x, -y, -z);

    // Pass the final view and projection matrices into Triton.
    if (environment) {
        environment->SetCameraMatrix(mv);
        environment->SetProjectionMatrix(proj);
    }
}

static int cubeMapIndex = 0;
void nextCubeMap(void)
{
    ++cubeMapIndex;
    if (cubeMapIndex > 9) cubeMapIndex = 1;

    std::stringstream ss;

    ss << "0" << cubeMapIndex;

#ifdef _WIN32
    std::string strDir = "..\\media\\skyboxes\\" + ss.str() + "\\";
#else
    std::string strDir = "../media/skyboxes/" + ss.str() + "/";
#endif
    skyBox->SetCubeMap(strDir.c_str());
    environment->SetEnvironmentMap((Triton::TextureHandle)(skyBox->GetCubemap()));
}

// Draw a frame of animation including the Triton ocean and our skybox.
void DrawFrame()
{
    double skyBoxMatrix[16];

    // Update our frame timer; output average framerate every 200 frames.
    if (startTime == 0) {
        startTime = getMilliseconds();
    }
    frameCount++;
    if (frameCount % 200 == 0) {
        long elapsed = getMilliseconds() - startTime;
        float fps = (float)frameCount / (float)elapsed * 1000.0f;
        printf("Average FPS: %.2f\n", fps);
    }

    // Compute the modelview and perspective matrices, and a pass them into Triton.
    // Compute the viewproj matrix for our skybox at the same time.
    GrabMatrices(skyBoxMatrix);

    // Clear the depth buffer
    if (ocean->IsCameraAboveWater()) {
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
    } else {
        glClearColor(0.0f, 0.1f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    // Now light and draw the ocean:
    if (environment) {
        // The sun position is roughly where it is in our skybox texture:
        Triton::Vector3 lightPosition(0, 1.0 / sqrt(2.0), 1.0 / sqrt(2.0));
        environment->SetDirectionalLight(lightPosition, Triton::Vector3(1.0, 1.0, 1.0));

        // Ambient color based on the zenith color in the cube map
        Triton::Vector3 ambientColor(67.0 / 255.0, 102.0 / 255.0, 104.0 / 255.0);
        ambientColor.Normalize();
        environment->SetAmbientLight(ambientColor);

        environment->SetBelowWaterVisibility(1000.0, Triton::Vector3(0.0, 0.1, 0.3));

        // Grab the cube map from our sky box and give it to Triton to use as an environment map
        environment->SetEnvironmentMap((Triton::TextureHandle)(skyBox->GetCubemap()));

        // Draw the ocean for the current time sample
        if (ocean) {

            unsigned long millis = getMilliseconds();

            // Ensure proper sorting of the waves against each other:
            glDepthFunc(GL_LEQUAL);

            // Actually draw the ocean:
            ocean->Draw((double)millis * 0.001);
        }
    }

    // Draw our skybox last to take advantage of early depth culling
    if (skyBox && ocean->IsCameraAboveWater()) {
        skyBox->Draw(skyBoxMatrix);
    }

    // Reveal the frame and ask for another one
    //glutSwapBuffers();
    //glutPostRedisplay();
}

// Handle a window resize
void Resize(GLint width, GLint height)
{
    glViewport(0, 0, width, height);
}

// Clean up our resources
void Destroy()
{
    if (ocean) delete ocean;
    if (environment) delete environment;
    if (resourceLoader) delete resourceLoader;
    if (skyBox) delete skyBox;
}

////////////////////////////////////////////////////////////////////////////////
void Keyboard(unsigned char key, int, int)
{
    switch(key) {
    case 27:
        exit(0);
        break;

    case 'd':
    case 'D':
        pitch += 0.01;
        break;

    case 'A':
    case 'a':
        pitch -= 0.01;
        break;

    case 'w':
    case 'W':
        roll += 0.01;
        break;

    case 's':
    case 'S':
        roll -= 0.01;
        break;

    case 'q':
    case 'Q':
        yaw += 0.01;
        break;

    case 'e':
    case 'E':
        yaw -= 0.01;
        break;

    case 'r':
    case 'R':
        y += 10.0;
        break;

    case 'f':
    case 'F':
        y -= 10.0;
        break;

    case 'c':
    case 'C':
        nextCubeMap();
        break;

    case 'i': {
        static Triton::Impact *impact = 0;
        if (!impact) {
            impact = new Triton::Impact(ocean, 0.01, 260000.0, true);
        }
        unsigned long millis = getMilliseconds();
        impact->Trigger(Triton::Vector3(0, 10, -100), Triton::Vector3(0, -1, 0), 100.0, (double)millis * 0.001);
    }
    break;

    default:
        break;
    }
    printf( "Yaw: %.2f Pitch: %.2f Roll: %.2f Altitude: %.2f\n", 180 * yaw / TRITON_PI,
            180 * pitch / TRITON_PI, 180 * roll / TRITON_PI, y );

    //glutPostRedisplay();
}
////////////////////////////////////////////////////////////////////////////////
int main_(int argc, char **argv)
{
    // Set up our window
    //glutInit(&argc, (char **)argv);
    //glutInitDisplayMode(//glut_RGBA | //glut_DEPTH | //glut_DOUBLE);
    //glutInitWindowSize(800, 600);
    //glutCreateWindow("Triton OpenGL Sample");

    // We use GLEW mainly for the benefit of our SkyBox class
    //glewInit();

    // Disable vertical sync for framerate measurements
    if (argc > 1 && strcmp(argv[1], "novsync") == 0) {
        SetVSync(0);
    }

    // Set up Triton's resources
    if (!InitTriton()) {
        return 0;
    }

    // Create our sky box to render behind Triton's ocean and provide
    // its environment map
    skyBox = new SkyBox();
    skyBox->Create();
    nextCubeMap();

    // Some free//glut callbacks
    //glutDisplayFunc(DrawFrame);
    //glutReshapeFunc(Resize);
    //glutKeyboardFunc(Keyboard);

#ifdef FREE//glut
    //glutCloseFunc(Destroy);
    //glutSetOption(//glut_ACTION_ON_WINDOW_CLOSE, //glut_ACTION_//glutMAINLOOP_RETURNS);
#endif

    // Off we go:
    //glutMainLoop();

    return 0;
}

unsigned long getMilliseconds()
{
#ifdef _WIN32
    static DWORD startMillis = 0;
    DWORD millis = timeGetTime();
    if (startMillis == 0) {
        startMillis = millis;
    }
    return millis - startMillis;
#else
    struct timeb tp;
    ftime(&tp);
    static time_t startTime = 0;
    if (startTime == 0) startTime = tp.time;
    return (((tp.time - startTime) * 1000) + tp.millitm);
#endif
}

// Enable / disable vsync for framerate measurements (use the novsync command line argument.)
void SetVSync(int interval)
{
    printf("Disabling vsync...\n");
#if defined(__APPLE__)
    return;
#elif defined(_WIN32)
    PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT =
        (PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress("wglSwapIntervalEXT");

    if (wglSwapIntervalEXT) {
        wglSwapIntervalEXT(interval);
    }
#else
    PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI =
        (PFNGLXSWAPINTERVALSGIPROC) glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalSGI");
    if (glXSwapIntervalSGI) {
        glXSwapIntervalSGI(interval);
    }
#endif
}
