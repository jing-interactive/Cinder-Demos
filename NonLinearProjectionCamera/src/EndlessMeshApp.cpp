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

struct NonLinearApp : public App
{
    void replaceGlslInGltf(ModelGLTFRef gltf, gl::GlslProgRef newGlsl)
    {
        auto& mtrls = gltf->materials;
        for (auto& mtrl : mtrls)
        {
            mtrl->ciShader = newGlsl;
        }
    }

    void setup() override
    {
        log::makeLogger<log::LoggerFileRotating>(fs::path(), "app.%Y.%m.%d.log");
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
            mRoot->addChild(grid);

            ModelGLTF::Option option = {};
            //option.loadTextures = false;
            auto gltf = ModelGLTF::create(getAssetPath(MESH_NAME), option);
            mRoot->addChild(gltf);

            mGlsl = am::glslProg(VS_NAME, FS_NAME);

            replaceGlslInGltf(gltf, mGlsl);
        }

        getSignalUpdate().connect([&] {
            mCam.setEyePoint({ CAM_POS_X, CAM_POS_Y, CAM_POS_Z });
            mCam.setNearClip(NEAR_CLIP);
            mCam.setFarClip(FAR_CLIP);
            mCam.setFovHorizontal(FOV_H);

            mRoot->setScale({ SCALE, SCALE, SCALE });
            mRoot->treeUpdate();
        });

        getWindow()->getSignalDraw().connect([&] {
            gl::setMatrices(mCam);
            gl::clear();
            gl::setWireframeEnabled(WIREFRAME);

            vec4 cylindricalProj(0.0f);
            {
                cylindricalProj.x = toRadians(mCam.getFov());
                cylindricalProj.y = mCam.getAspectRatio();
                cylindricalProj.z = mCam.getNearClip();
                cylindricalProj.w = mCam.getFarClip();
            }
            gl::setViewMatrix(mCam.getViewMatrix());
#if 1
            gl::setProjectionMatrix(glm::perspective(cylindricalProj.x, cylindricalProj.y, cylindricalProj.z, cylindricalProj.w));
#else
            gl::setProjectionMatrix(mCam.getProjectionMatrix());
#endif
            mGlsl->uniform("U_CylindricalProj", cylindricalProj);
            mGlsl->uniform("U_CamProj", vec2(CAMERA_FORM, FOV_PORTRAIT));

            mRoot->treeDraw();
        });
    }

    gl::GlslProgRef mGlsl;
    CameraPersp mCam;
    CameraUi mCamUi;
    melo::NodeRef mRoot;
    ModelGLTFRef mGltf;
};

auto gfxOption = RendererGl::Options().msaa(4);// .debug().debugLog(GL_DEBUG_SEVERITY_HIGH);

CINDER_APP(NonLinearApp, RendererGl(gfxOption), [](App::Settings *settings) {
    readConfig();
    //settings->setConsoleWindowEnabled();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
})
