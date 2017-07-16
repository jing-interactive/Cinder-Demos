#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"

#include "AssetManager.h"
#include "MiniConfig.h"

#define FLYTHROUGH_CAMERA_IMPLEMENTATION
#include "flythrough_camera/flythrough_camera.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class FlightThroughCameraApp : public App
{
public:
    void setup() override
    {
        log::makeLogger<log::LoggerFile>();
        
        auto aabb = am::triMesh(MESH_NAME)->calcBoundingBox();
        mCam.lookAt(aabb.getMax() * 2.0f, aabb.getCenter());
        
        createConfigUI({200, 200});
        gl::enableDepth();
        
        getWindow()->getSignalResize().connect([&] {
            mCam.setAspectRatio( getWindowAspectRatio() );
        });

        getWindow()->getSignalKeyDown().connect([&](KeyEvent& event) {
            mIsKeyPressed[event.getCode()] = true;
        });
        
        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE) quit();
            
            mIsKeyPressed[event.getCode()] = false;
        });
        
        getWindow()->getSignalMouseDown().connect([&](MouseEvent& ev) {
            mIsRMousePressed = ev.isRight();
        });
        
        getWindow()->getSignalMouseUp().connect([&](MouseEvent& ev) {
            mIsRMousePressed = false;
        });
        
        getWindow()->getSignalMouseMove().connect([&](MouseEvent& ev) {
            mIsRMousePressed = ev.isRight();
            mMousePos = ev.getPos();
        });

        getSignalUpdate().connect([&] {
            float delta_time_sec = getElapsedSeconds() - mElapsedSeconds;
            mElapsedSeconds = getElapsedSeconds();

            float activated = true;
            
            flythrough_camera_update(
                                     pos, look, up, &mViewMatrix[0][0],
                                     delta_time_sec,
                                     1.0f * (mIsKeyPressed[KeyEvent::KEY_LSHIFT] ? 2.0f : 1.0f) * activated,
                                     0.2f,
                                     80.0f,
                                     mMousePos.x - mPrevMousePos.x, mMousePos.y - mPrevMousePos.y,
                                     mIsKeyPressed[KeyEvent::KEY_w], mIsKeyPressed[KeyEvent::KEY_a],mIsKeyPressed[KeyEvent::KEY_s],mIsKeyPressed[KeyEvent::KEY_d],
                                     mIsKeyPressed[KeyEvent::KEY_SPACE], mIsKeyPressed[KeyEvent::KEY_LCTRL],
                                     0);
            
            if (activated)
            {
                float* view = &mViewMatrix[0][0];
                printf("\n");
                printf("pos: %f, %f, %f\n", pos[0], pos[1], pos[2]);
                printf("look: %f, %f, %f\n", look[0], look[1], look[2]);
                printf("view: %f %f %f %f\n"
                       "      %f %f %f %f\n"
                       "      %f %f %f %f\n"
                       "      %f %f %f %f\n",
                       view[0],  view[1],  view[2],  view[3],
                       view[4],  view[5],  view[6],  view[7],
                       view[8],  view[9], view[10], view[11],
                       view[12], view[13], view[14], view[15]);
            }
            
            mPrevMousePos = mMousePos;
        });
        
        mGlslProg = am::glslProg(VS_NAME, FS_NAME);
        mGlslProg->uniform("uTex0", 0);
        mGlslProg->uniform("uTex1", 1);
        mGlslProg->uniform("uTex2", 2);
        mGlslProg->uniform("uTex3", 3);
        
        getWindow()->getSignalDraw().connect([&] {
            gl::setViewMatrix(mViewMatrix);
            gl::setProjectionMatrix( mCam.getProjectionMatrix() );
            gl::clear();
            
            gl::ScopedTextureBind tex0(am::texture2d(TEX0_NAME), 0);
            gl::ScopedTextureBind tex1(am::texture2d(TEX1_NAME), 1);
            gl::ScopedTextureBind tex2(am::texture2d(TEX2_NAME), 2);
            gl::ScopedTextureBind tex3(am::texture2d(TEX3_NAME), 3);
            gl::ScopedGlslProg glsl(mGlslProg);
            
            gl::draw(am::vboMesh(MESH_NAME));
        });
        
        mElapsedSeconds = getElapsedSeconds();
        
        flythrough_camera_look_to(pos, look, up, &mViewMatrix[0][0], 0);
    }
    
private:
    CameraPersp         mCam;
    gl::GlslProgRef     mGlslProg;
    double              mElapsedSeconds;
    ivec2               mMousePos, mPrevMousePos;
    
    bool                mIsKeyPressed[KeyEvent::KEY_LAST] = {false};
    bool                mIsRMousePressed = false;

    mat4                mViewMatrix;
    float pos[3] = { 0.0f, 0.0f, 0.0f };
    float look[3] = { 0.0f, 0.0f, 1.0f };
    const float up[3] = { 0.0f, 1.0f, 0.0f };
};

CINDER_APP( FlightThroughCameraApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
} )
