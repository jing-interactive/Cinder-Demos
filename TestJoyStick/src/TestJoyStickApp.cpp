#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/Log.h"

#include "AssetManager.h"
#include "MiniConfigImgui.h"
#include "glfw3.h"

using namespace ci;
using namespace ci::app;
using namespace std;

struct TestJoyStickApp : public App
{
    void setup() override
    {
        glfwInit();

        log::makeLogger<log::LoggerFileRotating>(fs::path(), "app.%Y.%m.%d.log");
        
        auto aabb = am::triMesh(MESH_NAME)->calcBoundingBox();
        mCam.lookAt(aabb.getMax() * 2.0f, aabb.getCenter());
        mCamUi = CameraUi( &mCam, getWindow(), -1 );

        createConfigImgui();
        gl::enableDepth();

        getWindow()->getSignalResize().connect([&] {
            APP_WIDTH = getWindowWidth();
            APP_HEIGHT = getWindowHeight();
            mCam.setAspectRatio( getWindowAspectRatio() );
        });

        getSignalCleanup().connect([&] { 
            writeConfig();
            glfwTerminate();
        });

        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE) quit();
        });

        getSignalUpdate().connect([&] {
            const int joy = 0;
            _IS_JOYSTICK = glfwJoystickPresent(joy);
            if (_IS_JOYSTICK)
            {
                _JOYSTICK_NAME = glfwGetJoystickName(joy);
                const float* axes = glfwGetJoystickAxes(joy, &_JOYSTICK_AXES);
                const unsigned char* buttons = glfwGetJoystickButtons(joy, &_JOYSTICK_BUTTONS);
                
                char label[100];
                ImGui::Begin(_JOYSTICK_NAME.c_str());
                {
                    ImGui::Text("Axes: %d", _JOYSTICK_AXES);
                    for (int i = 0; i < _JOYSTICK_AXES; i++)
                    {
                        sprintf(label, "Axe_%d", i);
                        ImGui::DragFloat(label, (float*)(axes + i), 1.0f, 0, 1, "%.2f");
                    }
                }

                {
                    vector<unsigned char> v(buttons, buttons + _JOYSTICK_BUTTONS);
                    ImGui::Text("Buttons: %d", _JOYSTICK_BUTTONS);
                    for (int i = 0; i < _JOYSTICK_BUTTONS; i++)
                    {
                        sprintf(label, "Button_%d", i);
                        ImGui::RadioButton(label, (bool)buttons[i]);
                    }
                }
                ImGui::End();
            }
        });

        mGlslProg = am::glslProg(VS_NAME, FS_NAME);
        mGlslProg->uniform("uTex0", 0);
        mGlslProg->uniform("uTex1", 1);
        mGlslProg->uniform("uTex2", 2);
        mGlslProg->uniform("uTex3", 3);

        getWindow()->getSignalDraw().connect([&] {
            gl::setMatrices( mCam );
            gl::clear();
        
            gl::ScopedTextureBind tex0(am::texture2d(TEX0_NAME), 0);
            gl::ScopedTextureBind tex1(am::texture2d(TEX1_NAME), 1);
            gl::ScopedTextureBind tex2(am::texture2d(TEX2_NAME), 2);
            gl::ScopedTextureBind tex3(am::texture2d(TEX3_NAME), 3);
            gl::ScopedGlslProg glsl(mGlslProg);

            gl::draw(am::vboMesh(MESH_NAME));
        });
    }
    
    CameraPersp         mCam;
    CameraUi            mCamUi;
    gl::GlslProgRef     mGlslProg;
};

CINDER_APP( TestJoyStickApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
} )
