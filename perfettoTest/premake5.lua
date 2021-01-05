-- https://github.com/premake/premake-core/wiki

local action = _ACTION or ""

solution "fbx2gltf"
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
            "third_party/FBX/lib/debug"
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
         "third_party/FBX/lib/release"
        }

    project "fbx2gltf"
        kind "ConsoleApp"
        includedirs {
            "third_party",
            "third_party/FBX/include",
            "third_party/CLI11",
            "third_party/json",
            "third_party/stb",
            "third_party/fifo_map",
            "third_party/fmt/include",
            "third_party/cppcodec",
            "src",
        }
        files { 
            "src/**",
            "third_party/fmt/**",
        }
         links {
               "libfbxsdk",
         }