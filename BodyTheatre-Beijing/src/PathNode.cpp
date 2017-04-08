#include "cinder/app/App.h"
#include "cinder/Perlin.h"
#include "cinder/Triangulate.h"
#include <boost/foreach.hpp>
#include "cinder/Rand.h"
#include "CinderOpenCV.h"
#include "OpenCV.h"
#include "BlobTracker.h"
#include "PathNode.h"

using namespace ci::app;

namespace
{
	Perlin perlin;
}

PathNode::PathNode()
{
	_rot = 0;
	_z = 0;
}

PathNode::PathNode( const Path2d& pathW )
{
	_rot = 0;
	_z = 0;
	setup(pathW);
}

void PathNode::draw()
{
	gl::color(_clr);
	gl::pushModelView();
	gl::multModelView(transform);
	gl::draw(_mesh);
	gl::popModelView();
}

void PathNode::setup( const Path2d& pathW )
{
	Rectf box = pathW.calcBoundingBox();
	_size.set(box.getWidth(), box.getHeight());

	Vec2f centerW = box.getCenter();

	vector<Vec2f> points;
//	_path.clear();
	int n_pts = pathW.getNumPoints();
	for (int i=0;i<n_pts;i++)
	{
		const Vec2f& ptW = pathW.getPoint(i);
		Vec2f ptL = ptW - centerW;
		points.push_back(ptL);
	}
//	_path.close();

	_path = Path2d(BSpline2f(points, 3, true, true));

	//build mesh
	TriMesh2d tri = Triangulator(_path).calcMesh();
	_mesh = gl::VboMesh(tri); 
	_pos = centerW;

	static uint32_t hexColors[]={
		0xFF0F3B,0xFF5B0F,0xFFD30F,0xB3FF0F,0xFF0FB3,0xFF4D6D,0xFF8A9F,0x3BFF0F,0xD30FFF,0x8AFFE9,0x4DFFDE,0x0FFF5B,
		0x5B0FFF,0x0F3BFF,0x0FB3FF,0x0FFFD3
	};

	_clr = ColorA::hex(hexColors[rand()%_countof(hexColors)]);
	_clr.a = 0.8f;
}

void PathNode::moveTo( const Vec2f& target, float duration)
{
	timeline().apply(&_pos, target, duration, EaseOutAtan());
	timeline().apply(&_rot, Rand::randFloat(-90, 90), duration, EaseOutExpo());
}

void PathNode::update()
{
	transform.setToIdentity();
	transform.translate(Vec3f(_pos.value(), 0));
	transform.rotate( Vec3f(0.0f, 0.0f, toRadians(_rot.value())) );
}

bool PathNode::isWorldPointInside(const ci::Vec3f& posW)
{
	// apply inverse transformation to convert mouse from world space to object space
	Vec3f mouseObjectPos = transform.inverted().transformPoint( posW );

	// now you can use Rect.contains() like this:
	return _path.contains( Vec2f(mouseObjectPos.x, mouseObjectPos.y));
}

void PathNode::drawOutline()
{	
	gl::pushModelView();
	gl::multModelView(transform);
	gl::draw(_path);
	gl::popModelView();
}

float PathNode::distance( const ci::Vec2f& posScr )
{
	return _pos.value().distance(posScr);
}
