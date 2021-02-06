-- https://github.com/premake/premake-core/wiki

local action = _ACTION or ""

solution "perfetto"
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
        }

    configuration "Debug"
        targetdir "Debug"
        debugdir "Debug"
        defines { "DEBUG" }
        symbols "On"
        targetsuffix "-d"
        libdirs {
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
        }

    project "trace_processor"
        kind "ConsoleApp"
        includedirs {
            "../../perfetto",
            "../../perfetto/include",
            "../../perfetto/include/perfetto/base/build_configs/bazel",
            "../../perfetto/bazel-perfetto/external/perfetto_dep_jsoncpp/include",
            "../../perfetto/bazel-perfetto/external/perfetto_dep_sqlite",
            "../../perfetto/bazel-perfetto/external/perfetto_dep_zlib",         
        }
        files { 
            "../../perfetto/src/base/*",
            "../../perfetto/src/ipc/*",
            "../../perfetto/src/trace_processor/*",
        }
        removefiles {
            "../../perfetto/**test.cc",
            "../../perfetto/**unix*",
            "../../perfetto/**posix*",
        }

        links {
        }