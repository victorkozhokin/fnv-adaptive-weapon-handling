cmake_minimum_required(VERSION 3.21)
project(AdaptiveWeaponHandling CXX)

# 32-bit only: every address in the plugin is a x86 FalloutNV.exe address.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "configure with a 32-bit generator, e.g. -A Win32")
endif()

add_library(AdaptiveWeaponHandling SHARED
    src/main.cpp
    src/config.cpp
    src/curve.cpp
    src/util/crt.cpp
    src/util/log.cpp
    src/util/paths.cpp
    src/util/patch.cpp
)

target_include_directories(AdaptiveWeaponHandling PRIVATE src)
target_compile_features(AdaptiveWeaponHandling PRIVATE cxx_std_20)
set_target_properties(AdaptiveWeaponHandling PROPERTIES PREFIX "" OUTPUT_NAME "AdaptiveWeaponHandling")

# clang only. The sources use clang's MS inline asm together with GNU builtins,
# and the plugin links without a C runtime, so cl.exe is not a supported
# compiler here -- scripts/build.sh is the reference build.
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR
        "only clang is supported; run scripts/build.sh, or configure with "
        "-DCMAKE_CXX_COMPILER=clang-cl")
endif()

target_compile_options(AdaptiveWeaponHandling PRIVATE
    -O2 -fno-exceptions -fno-rtti -fno-stack-protector -mno-stack-arg-probe
    -ffreestanding -fno-builtin -fms-extensions -fms-compatibility
    -Wall -Wno-unused-parameter)
