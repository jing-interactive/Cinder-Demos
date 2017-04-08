#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/Log.h"

#include "AssetManager.h"
#include "MiniConfig.h"

#include "yocto/yocto_gltf.h"
#include "nodes/Node.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace ph::nodes;

typedef shared_ptr<struct NodeGLTF> NodeGLTFRef;

struct NodeGLTF : public Node3D
{
    gl::VboMeshRef mesh;
    gl::Texture2dRef tex;
    // other PSO fields

    void draw()
    {
        if (tex && mesh && mShader)
        {
            gl::ScopedTextureBind tex0(tex);
            mShader->uniform("tex0", 0);
            gl::ScopedGlslProg glsl(mShader);
            gl::draw(mesh);
        }
    }
};

class CinderPBRApp : public App
{
  public:
    void setup() override
    {
        log::makeLogger<log::LoggerFile>();

        mRoot = make_shared<NodeGLTF>();

        {
            auto filename = getAssetPath(MESH_FILENAME);
            try
            {
                // load scene
                bool load_bin = true;
                bool load_shaders = true;
                bool load_img = true;
                bool skip_missing = false;

                auto gltf = unique_ptr<ygltf::glTF_t>(ygltf::load_gltf(filename.string(), load_bin, load_shaders, load_img, skip_missing));

                // flatten to scene
                gltfScene = unique_ptr<ygltf::fl_gltf>(ygltf::flatten_gltf(gltf.get(), gltf->scene));
            }
            catch (exception& ex)
            {
                CI_LOG_EXCEPTION("load_gltf", ex);
                STATUS = ex.what();
            }
        }

        {
            auto nodeGLTF = make_shared<NodeGLTF>();
            auto triMesh = am::triMesh("Teapot");
            auto aabb = triMesh->calcBoundingBox();
            nodeGLTF->mesh = gl::VboMesh::create(*triMesh);

            gl::Texture2d::Format mipFmt;
            mipFmt.enableMipmapping(true);
            mipFmt.setMinFilter(GL_LINEAR_MIPMAP_LINEAR);
            mipFmt.setMagFilter(GL_LINEAR);
            nodeGLTF->tex = am::texture2d("checkerboard", mipFmt);
        }

        const vec2 windowSize = toPixels( getWindowSize() );
        mCam = CameraPersp( (int32_t)windowSize.x, (int32_t)windowSize.y, 60.0f, 0.01f, 1000.0f );
        //mCam.lookAt( aabb.getMax(), aabb.getCenter() );
        mCamUi = CameraUi( &mCam, getWindow(), -1 );
        
        auto mParams = createConfigUI({500, 200});
    
        gl::enableDepth();
    }
    
    void resize() override
    {
        mCam.setAspectRatio( getWindowAspectRatio() );
    }
    
    void update() override
    {
        mRoot->treeUpdate(getElapsedSeconds());
    }
    
    void draw() override
    {
        gl::setMatrices( mCam );
        gl::clear( Color( 0, 0, 0 ) );

        mRoot->treeDraw();
    }
    
private:
    CameraPersp         mCam;
    CameraUi            mCamUi;

    unique_ptr<ygltf::fl_gltf> gltfScene;

    NodeGLTFRef         mRoot;
};

CINDER_APP( CinderPBRApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
} )
