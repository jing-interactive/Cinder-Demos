-- https://github.com/premake/premake-core/wiki

local action = _ACTION or ""

solution "UEViewer"
    location (action)
    configurations { "Debug", "Release" }
    platforms {"x64"}
    language "C++"
    cppdialect "C++17"
    characterset "ASCII"
    warnings "off"

    filter "system:windows"
        defines { 
            "_CRT_SECURE_NO_WARNINGS",
            "WIN32",
            "FBXSDK_SHARED"
        }

    configuration "Debug"
        targetdir "Debug"
        debugdir "Debug"
        defines { "DEBUG" }
        symbols "On"
        targetsuffix "-d"
        libdirs {
            -- "third_party/FBX/lib/debug"
        }

    configuration "Release"
        targetdir "Release"
        debugdir "Release"
        defines { "NDEBUG" }
        flags { "No64BitChecks" }
        editandcontinue "Off"
        optimize "Speed"
        optimize "On"
        editandcontinue "Off"
        libdirs {
        --  "third_party/FBX/lib/release"
        }

    project "UEViewer"
        kind "ConsoleApp"
        includedirs {
            "../blocks/SDL2/include",
            "../../UEViewer",
            "../../UEViewer/Core",
            "../../UEViewer/Unreal",
            "../../UEViewer/UmodelTool",
            "../../UEViewer/libs",
            "../../UEViewer/libs/libpng",
            "../../UEViewer/libs/include",
            "../../UEViewer/libs/includewin32",
            "../../UEViewer/libs/nvtt",
            "../../UEViewer/libs/PowerVR",
            "../../UEViewer/libs/detex",
            "../../UEViewer/libs/zlib",
        }
        files { 
            "../../UEViewer/Exporters/**",
            "../../UEViewer/Unreal/**",
            "../../UEViewer/Viewers/**",
            "../../UEViewer/MeshInstance/**",
            "../../UEViewer/Core/**",
            -- "../../UEViewer/UmodelTool/res/umodel.rc",
            "../../UEViewer/UmodelTool/*",
            "../../UEViewer/UI/**",

            "../../UEViewer/libs/lz4/**",
            "../../UEViewer/libs/lzo/**",
            "../../UEViewer/libs/PowerVR/**",
            "../../UEViewer/libs/nvtt/**",
            "../../UEViewer/libs/libpng/**",
            "../../UEViewer/libs/astc/**",
            "../../UEViewer/libs/detex/**",
            "../../UEViewer/libs/zlib/**",
            "../../UEViewer/libs/mspack/**",
            "../../UEViewer/libs/rijndael/**",
        }
         links {
            "../blocks/SDL2/lib/x64/SDL2.lib",
         }