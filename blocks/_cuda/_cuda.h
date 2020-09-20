#pragma once

//#include <cuda_gl_interop.h>
#include <cuda_runtime_api.h>
#include <cuda_profiler_api.h>

#define ENTRY(func) extern decltype(::func)  *_##func;
#include "_cuda.def"
#undef ENTRY(func)

bool load_cuda();