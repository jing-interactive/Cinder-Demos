#include <cinder/app/AppBasic.h>
#include <cinder/Capture.h>
#include <cinder/Surface.h>
#include <cinder/Matrix44.h>
#include <cinder/Thread.h>

#include <cinder/gl/gl.h>
#include <cinder/gl/Texture.h>
#include <cinder/gl/Vbo.h>
#include "State/State.h"

#define NEW_CONTENT_SYSTEM

using namespace ci;
using namespace ci::app;
using namespace std;

namespace cinder{namespace params{
	class InterfaceGl;
}}
namespace ARContent{
	class ContentManager;
	class Content;
}

class BookAR : public AppBasic, public StateMachine<BookAR>
{
public:
	enum
	{
		APP_W = 400,
		APP_H = 750,
		CAM_W = 320,
		CAM_H = 240,
	};
  public:
	void setup();
	void shutdown();

	void mouseDown( MouseEvent event );
	void mouseMove( MouseEvent event );
	void mouseUp( MouseEvent event );
//	void buttonDown( ButtonEvent event);

	void update();
	void draw();
	void keyDown(KeyEvent event);

	void prepareSettings( Settings *settings );

public: //camera device
	Capture _capture;
	bool _capture_visible;
	int _device_id;
	Area _area_capture;

public:
	gl::Texture _tex_bg;

public: //AR
	shared_ptr<class ARTracker> _ar_tracker;
	std::mutex _mtx_ar;

public: //book rendering
	float _cube_scale;
	bool _2dbook_visible;
	bool _3dbook_visible;
	gl::VboMesh _mesh_book;
	void updateData(const ci::Surface32f& image, gl::VboMesh& mesh, float max_height);
	ci::Vec3f _mesh_translate;

public: //InterfaceGl
	shared_ptr<params::InterfaceGl>	mParams;
	bool _using_sdar;
	ci::Vec3f _light_dir;
	ci::ColorA _cube_clr;

public: //UI
	vector<shared_ptr<class UIElement>> _buttons;
	vector<shared_ptr<class UIElement>> _thumbs;

public:	//Content
	shared_ptr<ARContent::ContentManager> _content_mgr;

public: //GUI
	gl::Texture _tex_iphone4;

public: //States
	void setupStates();
	shared_ptr<State<BookAR>> _state_tracking;
	shared_ptr<State<BookAR>> _state_creating;
	shared_ptr<State<BookAR>> _state_sharing;

private:
	bool readAppConfig(const string& appXml);
};