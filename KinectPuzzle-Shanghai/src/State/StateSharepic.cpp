#include "States.h"
#include "PuzzleApp.h"
#include "cinder/Utilities.h"
#include <cinder/ImageIo.h>
#include "Sprite.h"
#include "Hand.h"

namespace
{
	Vec2f pos;
	Color8u clr(255,255,255);
	const int n_countdown = 60;
	string welcome = toUtf8(L"��Ƭ����");
	string icon_names[]={"/UI/sina-weibo.png", "/UI/mobile-icon.png"};
	const int ICON_W = 128;
	const int ICON_H = 128;
	Vec2i poses[2]={Vec2i(140,100), Vec2i(140,300)}; 
	shared_ptr<Sprite> icons[2];
	gl::Texture tex_bg;
	Anim<float> alpha;
}

void StateSharepic::enter()
{
	if (_app._img_sharing)
		tex_bg = _app._img_sharing;
	else
		tex_bg = _app._tex_selected;
	resetTimer();
	timeline().apply(&alpha, 1.0f, 0.0f, 4);
	pos.set(_app.getWindowSize()/2);
	for (int i=0;i<2;i++)
	{
		Sprite* spr = Sprite::createFromImage(loadImage(_app.getAppPath()/icon_names[i]), 
			poses[i].x,poses[i].y, ICON_W, ICON_H);
		icons[i] = shared_ptr<Sprite>(spr);
	}
}

void StateSharepic::update()
{
	if (getElapsedSeconds() > n_countdown)
		_app.changeToState(_app._state_idle);
}

void StateSharepic::draw()
{
	gl::color(1,1,1);
	gl::draw(tex_bg, _app.getWindowBounds());
	gl::drawStringCentered(welcome, pos, ColorA(1,1,1,alpha), _app.fnt_big);

	for (int i=0;i<2;i++)
	{		
		if (icons[i]->isPointInside(_app._hands[RIGHT]->pos))
		{
			gl::color(0.5,.5,0.7);
		}
		else
		{
			gl::color(1,1,1);
		}
		icons[i]->draw();
	}
	
}

void StateSharepic::exit()
{

}
