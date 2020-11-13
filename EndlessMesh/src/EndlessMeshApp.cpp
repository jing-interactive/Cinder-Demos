#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/Log.h"

#include "AssetManager.h"
#include "MiniConfigImgui.h"

#include "melo.h"
#include "cigltf.h"

using namespace ci;
using namespace ci::app;
using namespace std;

struct EndlessMeshApp : public App
{
    void setup() override
    {
        log::makeLogger<log::LoggerFileRotating>(fs::path(), "app.%Y.%m.%d.log");

        mCam.setNearClip(1);
        mCam.setFarClip(100000);
        mCamUi = CameraUi(&mCam, getWindow(), -1);

        createConfigImgui();
        gl::enableDepth();

        getWindow()->getSignalResize().connect([&] {
            APP_WIDTH = getWindowWidth();
            APP_HEIGHT = getWindowHeight();
            mCam.setAspectRatio(getWindowAspectRatio());
        });

        getSignalCleanup().connect([&] { writeConfig(); });

        getWindow()->getSignalKeyUp().connect([&](KeyEvent &event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE)
                quit();
        });

        {
            mRoot = melo::createRootNode();
            auto grid = melo::createGridNode();
            //mRoot->addChild(grid);

            ModelGLTF::Option option = {};
            option.loadTextures = false;
            auto gltf = ModelGLTF::create(getAssetPath(MESH_NAME), option);
            mRoot->addChild(gltf);
        }

        getSignalUpdate().connect([&] {
            mCam.setEyePoint({ CAM_POS_X, CAM_POS_Y, CAM_POS_Z });
            mRoot->setScale({ SCALE, SCALE, SCALE });
            mRoot->treeUpdate();
        });
        mGlsl = am::glslProg("endless.vert", "passthrough.frag");
        //gl::enableWireframe();

        getWindow()->getSignalDraw().connect([&] {
            gl::setMatrices(mCam);
            gl::clear();
            gl::setWireframeEnabled(WIREFRAME);

            mGlsl->uniform("uPlayerPos", vec3{ CAM_POS_X, CAM_POS_Y, CAM_POS_Z });
            mGlsl->uniform("uRollStrength", ROLL_STRENGTH);
            gl::ScopedGlslProg scpGlsl(mGlsl);

            mRoot->treeDraw();
        });
    }

    gl::GlslProgRef mGlsl;
    CameraPersp mCam;
    CameraUi mCamUi;
    melo::NodeRef mRoot;
    ModelGLTFRef mGltf;
};

auto gfxOption = RendererGl::Options().msaa(4);
CINDER_APP(EndlessMeshApp, RendererGl(gfxOption), [](App::Settings *settings) {
    readConfig();
    //settings->setConsoleWindowEnabled();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
})
