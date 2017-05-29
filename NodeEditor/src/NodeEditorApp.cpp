#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"

#include "AssetManager.h"
#include "MiniConfigImgui.h"

#include "imgui_node_graph_test.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class NodeEditorApp : public App
{
  public:
    void setup() override
    {
        log::makeLogger<log::LoggerFile>();
        createConfigImgui();
    
        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE) quit();
        });
        
        getWindow()->getSignalDraw().connect([&] {
            gl::clear();

            static bool isOpened = false;
            ShowExampleAppCustomNodeGraph(&isOpened);
        });
    }
    
private:
};

CINDER_APP( NodeEditorApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
} )
