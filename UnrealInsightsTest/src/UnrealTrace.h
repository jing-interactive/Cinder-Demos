#pragma once

// Hack to make unreal headers happy
#define WITH_UNREAL_DEVELOPER_TOOLS 0
#define WITH_PLUGIN_SUPPORT 0
#define UE_BUILD_DEVELOPMENT 1
#define WITH_EDITOR 0
#define WITH_ENGINE 0
#define IS_MONOLITHIC 0
#define IS_PROGRAM 1
#define PLATFORM_WINDOWS 1
#define PLATFORM_IS_EXTENSION 1
#define OVERRIDE_PLATFORM_HEADER_NAME Windows/Windows
#define UE_TRACE_ENABLED 1

#define TRACELOG_API
#define CORE_API

#define WITH_SERVER_CODE 0

#include "Trace/Trace.h"
#include "Trace/Trace.inl"

