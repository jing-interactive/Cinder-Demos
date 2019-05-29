#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/Log.h"

#include "AssetManager.h"
#include "MiniConfig.h"

using namespace ci;
using namespace ci::app;
using namespace std;

Surface createSolid(int width, int height, Color8u c)
{
    Surface result(width, height, false);
    Surface::Iter it = result.getIter();
    while (it.line()) {
        while (it.pixel()) {
            it.r() = c.r;
            it.g() = c.g;
            it.b() = c.b;
        }
    }

    return result;
}

class VolumeRenderApp : public App
{
  public:
    void setup() override
    {
        log::makeLogger<log::LoggerFile>();
        
        auto aabb = am::triMesh(MESH_NAME)->calcBoundingBox();
        mCam.lookAt(aabb.getMax() * 2.0f, aabb.getCenter());
        mCamUi = CameraUi( &mCam, getWindow(), -1 );
        
        createConfigUI({200, 200});
        gl::enableDepth();

        getWindow()->getSignalResize().connect([&] {
            APP_WIDTH = getWindowWidth();
            APP_HEIGHT = getWindowHeight();
            mCam.setAspectRatio( getWindowAspectRatio() );
        });

        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE) quit();
        });
        
        mGlslProg = am::glslProg(VS_NAME, FS_NAME);
        mGlslProg->uniform("uTex0", 0);
        mGlslProg->uniform("uTex1", 1);

        {
            // GL_TEXTURE_3D
            mTex3d = gl::Texture3d::create(256, 256, 3);
            mTex3d->update(createSolid(mTex3d->getWidth(), mTex3d->getHeight(), Color8u(255, 0, 0)), 0);
            mTex3d->update(createSolid(mTex3d->getWidth(), mTex3d->getHeight(), Color8u(0, 255, 0)), 1);
            mTex3d->update(createSolid(mTex3d->getWidth(), mTex3d->getHeight(), Color8u(0, 0, 255)), 2);
            mTex3d->setWrapR(GL_REPEAT);

            mShader3d = gl::GlslProg::create(loadAsset("shader.vert"), loadAsset("shader_3d.frag"));
            mShader3d->uniform("uTex0", 0);

            // setup batches
            mTex3dBatch = gl::Batch::create(geom::Rect(Rectf(100, 100, 300, 300)), mShader3d);
        }

        getWindow()->getSignalDraw().connect([&] {
            gl::clear();
        
            if (false)
            {
                gl::setMatrices(mCam);
                gl::ScopedTextureBind tex0(am::texture3d(TEX0_NAME), 0);
                gl::ScopedTextureBind tex1(am::texture3d(TEX1_NAME), 0);
                gl::ScopedGlslProg glsl(mGlslProg);

                gl::draw(am::vboMesh(MESH_NAME));
            }

            mTex3d->bind();
            mTex3dBatch->getGlslProg()->uniform("uTexCoord", vec3(0.5, 0.5, getElapsedSeconds() / 2));
            mTex3dBatch->draw();
        });
    }
    
private:
    CameraPersp         mCam;
    CameraUi            mCamUi;
    gl::GlslProgRef     mGlslProg;

    gl::Texture3dRef	mTex3d;
    gl::GlslProgRef		mShader3d;
    gl::BatchRef		mTex3dBatch;
};

CINDER_APP( VolumeRenderApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
} )
