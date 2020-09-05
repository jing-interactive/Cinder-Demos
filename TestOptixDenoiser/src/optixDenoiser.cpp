
//
// Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//


#include "OptiXDenoiser.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <Cinder/Surface.h>
#include <Cinder/Timer.h>
#include "AssetManager.h"

using namespace ci;


//------------------------------------------------------------------------------
//
//  optixDenoiser -- Demonstration of the OptiX denoising API.  
//
//------------------------------------------------------------------------------

void printUsageAndExit( const std::string& argv0 )
{
    std::cout
        << "Usage  : " << argv0 << " [options] color.exr\n"
        << "Options: -n | --normal <normal.exr>\n"
        << "         -a | --albedo <albedo.exr>\n"
        << "         -o | --out    <out.exr> Defaults to 'denoised.exr'\n"
        << std::endl;
    exit(0);
}


int32_t main_( int32_t argc, char** argv )
{
    if( argc < 2 )
        printUsageAndExit( argv[0] );
    std::string color_filename = argv[argc-1];

    std::string normal_filename;
    std::string albedo_filename;
    std::string output_filename = "denoised.exr";

    for( int32_t i = 1; i < argc-1; ++i )
    {
        std::string arg( argv[i] );

        if( arg == "-n" || arg == "--normal" )
        {
            if( i == argc-2 )
                printUsageAndExit( argv[0] );
            normal_filename = argv[++i];
        }
        else if( arg == "-a" || arg == "--albedo" )
        {
            if( i == argc-2 )
                printUsageAndExit( argv[0] );
            albedo_filename = argv[++i];
        }
        else if( arg == "-o" || arg == "--out" )
        {
            if( i == argc-2 )
                printUsageAndExit( argv[0] );
            output_filename = argv[++i];
        }
        else
        {
            printUsageAndExit( argv[0] );
        }
    }

    SurfaceRef color  = {};
    SurfaceRef normal = {};
    SurfaceRef albedo = {};

    try
    {
        Timer timer(true);
        std::cout << "Loading inputs ..." << std::endl;

        color = am::surface( color_filename.c_str() );
        std::cout << "\tLoaded color image (" << color->getWidth() << "x" << color->getHeight() << ")" << std::endl;

        if( !normal_filename.empty() )
        {
            normal = am::surface( normal_filename.c_str() );
            std::cout << "\tLoaded normal image" << std::endl;
        }

        if( !albedo_filename.empty() )
        {
            albedo = am::surface( albedo_filename.c_str() );
            std::cout << "\tLoaded albedo image" << std::endl;
        }

        //const double t1 = sutil::currentTime();
        std::cout << "\tLoad inputs from disk     :" << timer.getSeconds() * 1000 << "ms" << std::endl;

        CI_ASSERT( color.pixel_format  == sutil::FLOAT4 );
        CI_ASSERT( !albedo.data || albedo.pixel_format == sutil::FLOAT4 );
        CI_ASSERT( !normal.data || normal.pixel_format == sutil::FLOAT4 );

        std::vector<float> output;
        output.resize( color->getWidth()*color->getHeight()*4 );

        OptiXDenoiser::Data data;
        data.width    = color->getWidth();
        data.height   = color->getHeight();
        data.color    = reinterpret_cast<float*>(  color.data );
        data.albedo   = reinterpret_cast<float*>( albedo.data );
        data.normal   = reinterpret_cast<float*>( normal.data );
        data.output   = output.data();

        std::cout << "Denoising ..." << std::endl;
        OptiXDenoiser denoiser;

        timer.stop();
        timer.start();
        {
            denoiser.init( data );
            std::cout << "\tAPI Initialization     :" << timer.getSeconds() * 1000 << "ms" << std::endl;
        }

        {
            denoiser.exec();
            std::cout << "\tDenoise frame     :" << timer.getSeconds() * 1000 << "ms" << std::endl;
        }

        {
            denoiser.finish();
            std::cout << "\tCleanup state/copy to host     :" << timer.getSeconds() * 1000 << "ms" << std::endl;
        }

        {
            std::cout << "Saving results to '" << output_filename << "'..." << std::endl;

            //Surface output_image;
            //output_image->getWidth()        = color->getWidth();
            //output_image->getHeight()       = color->getHeight();
            //output_image.data         = output.data();
            //output_image.pixel_format = sutil::FLOAT4;
            //sutil::saveImage( output_filename.c_str(), output_image, false );

            std::cout << "\tSave output to disk     :" << timer.getSeconds() * 1000 << "ms" << std::endl;
        }
    }
    catch( std::exception& e )
    {
        std::cerr << "ERROR: exception caught '" << e.what() << "'" << std::endl;
    }
}
