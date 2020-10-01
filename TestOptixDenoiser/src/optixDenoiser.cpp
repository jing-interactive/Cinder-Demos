
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
#include "../../blocks/optix/optix_function_table_definition.h"
#include "../../blocks/optix/optix_stubs.h"
#include "../../blocks/_cuda/_cuda.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <Cinder/Surface.h>
#include <Cinder/Timer.h>
#include <cinder/Log.h>
#include "AssetManager.h"

using namespace ci;

#define CUDA_CHECK(hr) (hr)
#define OPTIX_CHECK(hr) (hr)
#define CUDA_SYNC_CHECK() (0)

static void context_log_cb(uint32_t level, const char* tag, const char* message, void* /*cbdata*/)
{
    if (level < 4)
        CI_LOG_I("[" << std::setw(2) << level << "][" << std::setw(12) << tag << "]: "
            << message << "\n");
}

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

void OptiXDenoiser::init(Data& data)
{
    CI_ASSERT(data.color);
    CI_ASSERT(data.output);
    CI_ASSERT(data.width);
    CI_ASSERT(data.height);
    CI_ASSERT_MSG(!data.normal || data.albedo, "Currently albedo is required if normal input is given");

    m_host_output = data.output;

    load_cuda();

    //
    // Initialize CUDA and create OptiX context
    //
    {
        // Initialize CUDA
        CUDA_CHECK(_cudaFree(0));

        CUcontext cu_ctx = 0;  // zero means take the current context
        OPTIX_CHECK(optixInit());
        OptixDeviceContextOptions options = {};
        options.logCallbackFunction = &context_log_cb;
        options.logCallbackLevel = 4;
        OPTIX_CHECK(optixDeviceContextCreate(cu_ctx, &options, &m_context));
    }

    //
    // Create denoiser
    //
    {
        OptixDenoiserOptions options = {};
        options.inputKind =
            data.normal ? OPTIX_DENOISER_INPUT_RGB_ALBEDO_NORMAL :
            data.albedo ? OPTIX_DENOISER_INPUT_RGB_ALBEDO :
            OPTIX_DENOISER_INPUT_RGB;
        OPTIX_CHECK(optixDenoiserCreate(m_context, &options, &m_denoiser));
        OPTIX_CHECK(optixDenoiserSetModel(
            m_denoiser,
            OPTIX_DENOISER_MODEL_KIND_HDR,
            nullptr, // data
            0        // size
        ));
    }


    //
    // Allocate device memory for denoiser
    //
    {
        OptixDenoiserSizes denoiser_sizes;
        OPTIX_CHECK(optixDenoiserComputeMemoryResources(
            m_denoiser,
            data.width,
            data.height,
            &denoiser_sizes
        ));

        // NOTE: if using tiled denoising, we would set scratch-size to 
        //       denoiser_sizes.withOverlapScratchSizeInBytes
        m_scratch_size = static_cast<uint32_t>(denoiser_sizes.withoutOverlapScratchSizeInBytes);

        CUDA_CHECK(_cudaMalloc(reinterpret_cast<void**>(&m_intensity), sizeof(float)));
        CUDA_CHECK(_cudaMalloc(
            reinterpret_cast<void**>(&m_scratch),
            m_scratch_size
        ));

        CUDA_CHECK(_cudaMalloc(
            reinterpret_cast<void**>(&m_state),
            denoiser_sizes.stateSizeInBytes
        ));
        m_state_size = static_cast<uint32_t>(denoiser_sizes.stateSizeInBytes);

        const uint64_t frame_byte_size = data.width * data.height * sizeof(float4);
        CUDA_CHECK(_cudaMalloc(reinterpret_cast<void**>(&m_inputs[0].data), frame_byte_size));
        CUDA_CHECK(_cudaMemcpy(
            reinterpret_cast<void*>(m_inputs[0].data),
            data.color,
            frame_byte_size,
            cudaMemcpyHostToDevice
        ));
        m_inputs[0].width = data.width;
        m_inputs[0].height = data.height;
        m_inputs[0].rowStrideInBytes = data.width * sizeof(float4);
        m_inputs[0].pixelStrideInBytes = sizeof(float4);
        m_inputs[0].format = OPTIX_PIXEL_FORMAT_FLOAT4;

        m_inputs[1].data = 0;
        if (data.albedo)
        {
            CUDA_CHECK(_cudaMalloc(reinterpret_cast<void**>(&m_inputs[1].data), frame_byte_size));
            CUDA_CHECK(_cudaMemcpy(
                reinterpret_cast<void*>(m_inputs[1].data),
                data.albedo,
                frame_byte_size,
                cudaMemcpyHostToDevice
            ));
            m_inputs[1].width = data.width;
            m_inputs[1].height = data.height;
            m_inputs[1].rowStrideInBytes = data.width * sizeof(float4);
            m_inputs[1].pixelStrideInBytes = sizeof(float4);
            m_inputs[1].format = OPTIX_PIXEL_FORMAT_FLOAT4;
        }

        m_inputs[2].data = 0;
        if (data.normal)
        {
            CUDA_CHECK(_cudaMalloc(reinterpret_cast<void**>(&m_inputs[2].data), frame_byte_size));
            CUDA_CHECK(_cudaMemcpy(
                reinterpret_cast<void*>(m_inputs[2].data),
                data.normal,
                frame_byte_size,
                cudaMemcpyHostToDevice
            ));
            m_inputs[2].width = data.width;
            m_inputs[2].height = data.height;
            m_inputs[2].rowStrideInBytes = data.width * sizeof(float4);
            m_inputs[2].pixelStrideInBytes = sizeof(float4);
            m_inputs[2].format = OPTIX_PIXEL_FORMAT_FLOAT4;
        }

        CUDA_CHECK(_cudaMalloc(reinterpret_cast<void**>(&m_output.data), frame_byte_size));
        m_output.width = data.width;
        m_output.height = data.height;
        m_output.rowStrideInBytes = data.width * sizeof(float4);
        m_output.pixelStrideInBytes = sizeof(float4);
        m_output.format = OPTIX_PIXEL_FORMAT_FLOAT4;
    }

    //
    // Setup denoiser
    //
    {
        OPTIX_CHECK(optixDenoiserSetup(
            m_denoiser,
            0,  // CUDA stream
            data.width,
            data.height,
            m_state,
            m_state_size,
            m_scratch,
            m_scratch_size
        ));


        m_params.denoiseAlpha = 0;
        m_params.hdrIntensity = m_intensity;
        m_params.blendFactor = 0.0f;
    }
}


void OptiXDenoiser::exec()
{
    OPTIX_CHECK(optixDenoiserComputeIntensity(
        m_denoiser,
        0, // CUDA stream
        m_inputs,
        m_intensity,
        m_scratch,
        m_scratch_size
    ));

    OPTIX_CHECK(optixDenoiserInvoke(
        m_denoiser,
        0, // CUDA stream
        &m_params,
        m_state,
        m_state_size,
        m_inputs,
        m_inputs[2].data ? 3 : m_inputs[1].data ? 2 : 1, // num input channels
        0, // input offset X
        0, // input offset y
        &m_output,
        m_scratch,
        m_scratch_size
    ));

    CUDA_SYNC_CHECK();
}


void OptiXDenoiser::finish()
{
    const uint64_t frame_byte_size = m_output.width * m_output.height * sizeof(float4);
    CUDA_CHECK(_cudaMemcpy(
        m_host_output,
        reinterpret_cast<void*>(m_output.data),
        frame_byte_size,
        cudaMemcpyDeviceToHost
    ));

    // Cleanup resources
    optixDenoiserDestroy(m_denoiser);
    optixDeviceContextDestroy(m_context);

    CUDA_CHECK(_cudaFree(reinterpret_cast<void*>(m_intensity)));
    CUDA_CHECK(_cudaFree(reinterpret_cast<void*>(m_scratch)));
    CUDA_CHECK(_cudaFree(reinterpret_cast<void*>(m_state)));
    CUDA_CHECK(_cudaFree(reinterpret_cast<void*>(m_inputs[0].data)));
    CUDA_CHECK(_cudaFree(reinterpret_cast<void*>(m_inputs[1].data)));
    CUDA_CHECK(_cudaFree(reinterpret_cast<void*>(m_inputs[2].data)));
    CUDA_CHECK(_cudaFree(reinterpret_cast<void*>(m_output.data)));
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

        //CI_ASSERT( color.pixel_format  == sutil::FLOAT4 );
        //CI_ASSERT( !albedo.data || albedo.pixel_format == sutil::FLOAT4 );
        //CI_ASSERT( !normal.data || normal.pixel_format == sutil::FLOAT4 );

        std::vector<float> output;
        output.resize( color->getWidth()*color->getHeight()*4 );

        OptiXDenoiser::Data data;
        data.width    = color->getWidth();
        data.height   = color->getHeight();
        data.color    = reinterpret_cast<float*>(  color->getData() );
        data.albedo   = reinterpret_cast<float*>( albedo->getData() );
        data.normal   = reinterpret_cast<float*>( normal->getData());
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

    return 0;
}
