/*
 Copyright (c) 2010-2012, Paul Houx - All rights reserved.
 This code is intended for use with the Cinder C++ library: http://libcinder.org

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this list of conditions and
 the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
 the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "NodeGL.h"

namespace nodes {

	// Basic support for 3D nodes
	typedef std::shared_ptr<class Node3D> Node3DRef;

	class Node3D : public NodeGL {
	public:
		Node3D(void);
		virtual ~Node3D(void);

		//! the drawMesh function only draws a mesh without binding textures and shaders
		virtual void drawWireframe() {}
		virtual void treeDrawWireframe();

		// getters and setters
		virtual ci::vec3 getPosition() const { return mPosition; }
		virtual void setPosition(float x, float y, float z)
		{
			mPosition = ci::vec3(x, y, z);
			invalidateTransform();
		}
		virtual void setPosition(const ci::vec3 &pt)
		{
			mPosition = pt;
			invalidateTransform();
		}

		virtual ci::quat getRotation() const { return mRotation; }
		virtual void setRotation(float radians)
		{
			mRotation = glm::angleAxis(radians, ci::vec3(0, 1, 0));
			invalidateTransform();
		}
		virtual void setRotation(const ci::vec3 &radians)
		{
			mRotation = glm::rotation(ci::vec3(0), radians);
			invalidateTransform();
		}
		virtual void setRotation(const ci::vec3 &axis, float radians)
		{
			mRotation = glm::angleAxis(radians, axis);
			invalidateTransform();
		}
		virtual void setRotation(const ci::quat &rot)
		{
			mRotation = rot;
			invalidateTransform();
		}

		virtual ci::vec3 getScale() const { return mScale; }
		virtual void setScale(float scale)
		{
			mScale = ci::vec3(scale, scale, scale);
			invalidateTransform();
		}
		virtual void setScale(float x, float y, float z)
		{
			mScale = ci::vec3(x, y, z);
			invalidateTransform();
		}
		virtual void setScale(const ci::vec3 &scale)
		{
			mScale = scale;
			invalidateTransform();
		}

		virtual ci::vec3 getAnchor() const { return mAnchor; }
		virtual void setAnchor(float x, float y, float z)
		{
			mAnchor = ci::vec3(x, y, z);
			invalidateTransform();
		}
		virtual void setAnchor(const ci::vec3 &pt)
		{
			mAnchor = pt;
			invalidateTransform();
		}

		// stream support
		virtual inline std::string toString() const { return "Node3D"; }
	protected:
		ci::vec3 mPosition;
		ci::quat mRotation;
		ci::vec3 mScale;
		ci::vec3 mAnchor;

		// required function (see: class Node)
		virtual void transform() const
		{
			// construct transformation matrix
			ci::mat4 transform = glm::translate(mPosition);
			transform *= glm::toMat4(mRotation);
			transform *= glm::scale(mScale);
			transform *= glm::translate(-mAnchor);

			setTransform(transform);
		}
	};
} // namespace nodes