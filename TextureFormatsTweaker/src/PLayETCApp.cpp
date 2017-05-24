#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/params/Params.h"
#include "cinder/ip/checkerboard.h"

#include "AssetManager.h"
#include "MiniConfig.h"

using namespace ci;
using namespace ci::app;
using namespace std;


vector<string> textureFormatNames = {
    "RGB8",
    "ETC2",
    "DXT1",
    "DXT3",
    "DXT5",
};
vector<GLenum> textureFormats = {
    GL_RGB8,
    GL_COMPRESSED_RGB8_ETC2,
    GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
    GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
    GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
};

class PLayETCApp : public App
{
  public:
    void setup() override
    {
        log::makeLogger<log::LoggerFile>();
        auto param = createConfigUI({300, 200});
        param->setPosition({ getWindowWidth() - 300, 10 });
 
        ADD_ENUM_TO_INT(param, TEXTURE_FORMAT, textureFormatNames);

        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE) quit();
        });
        
        getWindow()->getSignalDraw().connect([&] {
            gl::clear();

            if (mTextureFormat != TEXTURE_FORMAT)
            {
                auto surface = ip::checkerboard(512, 512);
                mTextureFormat = TEXTURE_FORMAT;
                auto format = gl::Texture2d::Format().internalFormat(textureFormats[TEXTURE_FORMAT]);
                mTexture = gl::Texture2d::create(surface, format);
            }
            if (mTexture) gl::draw(mTexture);
        });
    }
    
private:
    gl::Texture2dRef mTexture;
    int mTextureFormat = -1;
};

CINDER_APP( PLayETCApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(_APP_WIDTH, _APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
} )
