#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/Log.h"

#include "AssetManager.h"
#include "MiniConfigImgui.h"
#include "TextureHelper.h"

#include "OptiXDenoiser.h"

using namespace ci;
using namespace ci::app;
using namespace std;

struct TestOptixDenoiserApp : public App
{
    OptiXDenoiser mDenoiser;
    SurfaceRef mSrc, mDst;
    gl::TextureRef mTex;

    void setup() override
    {
        log::makeLogger<log::LoggerFileRotating>(fs::path(), "app.%Y.%m.%d.log");

        {
            mSrc = am::surface(SRC_IMAGE, true);

            mDst = Surface::create(mSrc->getWidth(), mSrc->getHeight(), true);

            OptiXDenoiser::Data data;
            data.width = mSrc->getWidth();
            data.height = mSrc->getHeight();
            data.color = mSrc->getData();
            data.output = mDst->getData();

            mDenoiser.init(data);
            mDenoiser.exec();
            mDenoiser.finish();
        }
        
        createConfigImgui();
        gl::enableDepth();

        getWindow()->getSignalResize().connect([&] {
            APP_WIDTH = getWindowWidth();
            APP_HEIGHT = getWindowHeight();
        });

        getSignalCleanup().connect([&] { writeConfig(); });

        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE) quit();
        });

        getSignalUpdate().connect([&] {
        });


        getWindow()->getSignalDraw().connect([&] {
            gl::setMatricesWindow( getWindowSize() );
            gl::clear();
        
            if (SHOW_SRC)
                updateTexture(mTex, *mSrc);
            else
                updateTexture(mTex, *mDst);

            gl::draw(mTex, getWindowBounds());
        });
    }
};

CINDER_APP( TestOptixDenoiserApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
} )
