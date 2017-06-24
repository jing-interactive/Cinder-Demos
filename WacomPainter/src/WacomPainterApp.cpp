#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"

#define EASYTAB_IMPLEMENTATION
#include "EasyTab/easytab.h"

#include "AssetManager.h"
#include "MiniConfig.h"

using namespace ci;
using namespace ci::app;
using namespace std;

WNDPROC oldWndProc;
LRESULT CALLBACK myWndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (EasyTab_HandleEvent(hWnd, message, lParam, wParam) == EASYTAB_OK) // Event
    {
        return true; // Tablet event handled
    }
    return CallWindowProc(oldWndProc, hWnd, message, wParam, lParam);
}

class WacomPainterApp : public App
{
  public:
    void setup() override
    {
        log::makeLogger<log::LoggerFile>();
        createConfigUI({200, 200});
    
        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE) quit();
        });

        getSignalUpdate().connect([&] {
            _PRESSURE = EasyTab->Pressure;
            _BUTTONS = EasyTab->Buttons;

            mPressure = (mPressure + _PRESSURE) * 0.5f;
        });
        
        getWindow()->getSignalDraw().connect([&] {
            gl::clear();

            gl::color(Color::gray(mPressure));
            gl::drawSolidCircle({EasyTab->PosX, EasyTab->PosY}, mPressure * 100);
        });

        {
            auto hWnd = getRenderer()->getHwnd();
            EasyTabResult res = EasyTab_Load(hWnd);
            if (res != EASYTAB_OK)
            {
                CI_LOG_E("EasyTab_Load fail with: " << res);
            }

            WNDPROC proc = (WNDPROC)SetWindowLong(hWnd, GWL_WNDPROC, (LONG)myWndProcHook);
            if (oldWndProc == NULL)
                oldWndProc = proc;

            getSignalCleanup().connect([] {
                EasyTab_Unload();
            });
        }
    }
    
private:
    float mPressure = 0;
};

CINDER_APP( WacomPainterApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
} )
