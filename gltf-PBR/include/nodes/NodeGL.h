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

#include "Node.h"

namespace nodes {

// Basic support for OpenGL nodes
typedef std::shared_ptr<class NodeGL> NodeGLRef;

class NodeGL : public Node {
  public:
	NodeGL( void ) {}
	virtual ~NodeGL( void ) {}

	// shader support
	template <typename T>
	void setShaderUniform( const std::string &name, const T &data )
	{
		if( mShader )
			mShader->uniform( name, data );
	}
	template <typename T>
	void setShaderUniform( const std::string &name, const T *data, int count )
	{
		if( mShader )
			mShader->uniform( name, data, count );
	}

	void setShaderUniform( const std::string &name, int data )
	{
		if( mShader )
			mShader->uniform( name, data );
	}
	void setShaderUniform( const std::string &name, float data )
	{
		if( mShader )
			mShader->uniform( name, data );
	}

	void bindShader()
	{
		if( mShader )
			mShader->bind();
	}

	// stream support
	virtual inline std::string toString() const { return "NodeGL"; }
  protected:
	ci::gl::GlslProgRef mShader;
};
} // namespace nodes