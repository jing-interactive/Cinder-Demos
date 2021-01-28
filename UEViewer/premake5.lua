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
            "../UEViewer",
            "../UEViewer/libs",
            "../UEViewer/libs/include",
            "../UEViewer/libs/includewin32",
        }
        files { 
            "../UEViewer/Exporters/**",
            "../UEViewer/Unreal/**",
            "../UEViewer/Unreal/FileSystem/**",
            "../UEViewer/Unreal/GameSpecific/**",
            "../UEViewer/Unreal/Mesh/**",
            "../UEViewer/Unreal/UnrealMaterial/**",
            "../UEViewer/Unreal/UnrealMesh/**",
            "../UEViewer/Unreal/UnrealPackage/**",
            "../UEViewer/Unreal/Wrappers/**",
            "../UEViewer/Viewers/**",
            "../UEViewer/MeshInstance/**",
            "../UEViewer/Core/**",
            "../UEViewer/UmodelTool/**",
            "../UEViewer/UI/**",
            "UmodelTool/res/umodel.rc"
        }
         links {
            "../UEViewer/libs/SDL2/x64/SDL2.lib",
         }