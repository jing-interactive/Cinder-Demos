#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/CameraUi.h"

#include "AssetManager.h"
#include "MiniConfig.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class CameraFovTestApp : public App
{
  public:
    void setup() override
    {
        log::makeLogger<log::LoggerFileRotating>(fs::path(), "IG.%Y.%m.%d.log");
        createConfigUI({200, 200});
        mCamUi = CameraUi(&mCam, getWindow(), -1);
        gl::enableDepth();

        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE) quit();
        });

        getSignalCleanup().connect([&] { writeConfig(); });
        
        getWindow()->getSignalDraw().connect([&] {
            mCam.setFov(FOV);
            gl::setMatrices(mCam);
            gl::clear();
            am::glslProg("lambert")->bind();

            for (int x = 0; x < 10; x++)
                for (int z = 0; z < 10; z++)
                {
                    mat4 mat = glm::translate(vec3(x, 0, z));
                    gl::setModelMatrix(mat);
                    gl::draw(am::vboMesh("Teapot"));
                }
        });
    }
    
private:
    CameraPersp         mCam;
    CameraUi            mCamUi;
};

CINDER_APP( CameraFovTestApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
} )
