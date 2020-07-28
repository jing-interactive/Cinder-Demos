#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/Log.h"

#include "AssetManager.h"
#include "MiniConfigImgui.h"

using namespace ci;
using namespace ci::app;
using namespace std;

struct FlyCameraRotateApp : public App
{
    void setup() override
    {
        log::makeLogger<log::LoggerFileRotating>(fs::path(), "IG.%Y.%m.%d.log");
        
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

        getSignalCleanup().connect([&] { writeConfig(); });

        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE) quit();
        });
        
        mGlslProg = am::glslProg("lambert");

        getWindow()->getSignalDraw().connect([&] {

            quat pitchYawRoll = glm::quat() * glm::angleAxis(toRadians(PITCH), vec3(1.0f, 0.0f, 0.0f))
                * glm::angleAxis(toRadians(YAW), vec3(0.0f, 1.0f, 0.0f))
                * glm::angleAxis(toRadians(ROLL), vec3(0.0f, 0.0f, 1.0f));

            gl::setMatrices( mCam );
            gl::clear();

            gl::drawCoordinateFrame(10, 1, 0.2);
        
            gl::ScopedGlslProg glsl(mGlslProg);

            gl::ScopedModelMatrix scp;
            auto mat = glm::translate(vec3{X,Y,Z}) * glm::toMat4(pitchYawRoll);

            gl::setModelMatrix(mat);
            gl::drawCoordinateFrame(2);
            gl::draw(am::vboMesh(MESH_NAME));
        });
    }
    
    CameraPersp         mCam;
    CameraUi            mCamUi;
    gl::GlslProgRef     mGlslProg;
};

CINDER_APP( FlyCameraRotateApp, RendererGl, [](App::Settings* settings) {
    readConfig();

    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
} )
