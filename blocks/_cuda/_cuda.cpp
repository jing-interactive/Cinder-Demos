#include "_cuda.h"

#define NOMINMAX
#include <windows.h>
#include <stdio.h>

#define ENTRY(func) decltype(::func)  *_##func = NULL;
#include "_cuda.def"
#undef ENTRY(func)

bool load_cuda()
{
	static HINSTANCE hDLLhandle = NULL;
	if (hDLLhandle) return true;

	const char* dllFilenames[] =
	{
		"C:\\Program Files\\NVIDIA GPU Computing Toolkit\\CUDA\\v10.1\\bin\\cudart64_101.dll",
	};
	for (auto file : dllFilenames)
	{
		hDLLhandle = LoadLibraryA(file);
		if (hDLLhandle) break;
	}

	// if the DLL can not be found, exit
	if (NULL == hDLLhandle)
	{
		printf("NVML DLL is not installed or not found at the default path.\r\n");
		return false;
	}

#define ENTRY(func) _##func = (decltype(func)*)GetProcAddress(hDLLhandle, #func);
#include "_cuda.def"
#undef ENTRY(func)

	return true;
}
