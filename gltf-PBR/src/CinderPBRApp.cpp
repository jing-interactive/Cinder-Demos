#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/Log.h"

#include "AssetManager.h"
#include "MiniConfig.h"

#include "tiny_gltf_loader/tiny_gltf_loader.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class CinderPBRApp : public App
{
  public:
    void setup() override
    {
        log::makeLogger<log::LoggerFile>();

        auto triMesh = am::triMesh("Teapot");
        auto aabb = triMesh->calcBoundingBox();
        mMesh = gl::VboMesh::create(*triMesh);

        gl::Texture2d::Format mipFmt;
        mipFmt.enableMipmapping( true );
        mipFmt.setMinFilter( GL_LINEAR_MIPMAP_LINEAR );
        mipFmt.setMagFilter( GL_LINEAR );
        mTexture = am::texture2d("checkerboard", mipFmt);        
        
        const vec2 windowSize = toPixels( getWindowSize() );
        mCam = CameraPersp( (int32_t)windowSize.x, (int32_t)windowSize.y, 60.0f, 0.01f, 1000.0f );
        mCam.lookAt( aabb.getMax(), aabb.getCenter() );
        mCamUi = CameraUi( &mCam, getWindow(), -1 );
        
        auto mParams = createConfigUI({200, 200});
    
        gl::enableDepth();
    }
    
    void resize() override
    {
        mCam.setAspectRatio( getWindowAspectRatio() );
    }
    
    void update() override
    {
        
    }
    
    void draw() override
    {
        gl::setMatrices( mCam );
        gl::clear( Color( 0, 0, 0 ) );
        gl::ScopedTextureBind tex0(mTexture);
        gl::ScopedGlslProg glsl(am::glslProg("texture"));
        gl::draw(mMesh);
    }
    
private:
    gl::VboMeshRef      mMesh;
    gl::Texture2dRef    mTexture;
    CameraPersp         mCam;
    CameraUi            mCamUi;
};

CINDER_APP( CinderPBRApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
} )
