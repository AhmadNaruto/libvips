cmake_minimum_required(VERSION 3.22.1)

project("vips_jni")

# Path variables for headers and prebuilt library binaries.
# By default, these point to src/main/include and src/main/jniLibs.
# Developers can customize these paths when integrating into their build systems.
set(VIPS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../include" CACHE PATH "Path to libvips header directories")
set(VIPS_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}" CACHE PATH "Path to libvips prebuilt shared libraries")

# Import prebuilt shared libraries of libvips and its dependencies
add_library(vips SHARED IMPORTED)
set_target_properties(vips PROPERTIES IMPORTED_LOCATION "${VIPS_LIB_DIR}/libvips.so")

add_library(glib-2.0 SHARED IMPORTED)
set_target_properties(glib-2.0 PROPERTIES IMPORTED_LOCATION "${VIPS_LIB_DIR}/libglib-2.0.so")

add_library(gobject-2.0 SHARED IMPORTED)
set_target_properties(gobject-2.0 PROPERTIES IMPORTED_LOCATION "${VIPS_LIB_DIR}/libgobject-2.0.so")

add_library(gio-2.0 SHARED IMPORTED)
set_target_properties(gio-2.0 PROPERTIES IMPORTED_LOCATION "${VIPS_LIB_DIR}/libgio-2.0.so")

# Compile our JNI bridge code into a shared library named libvips_jni.so
add_library(vips_jni SHARED
    vips_jni.cpp
)

# Header directories for compiling the JNI code
target_include_directories(vips_jni PRIVATE
    ${VIPS_INCLUDE_DIR}
    ${VIPS_INCLUDE_DIR}/glib-2.0
    ${VIPS_INCLUDE_DIR}/glib-2.0/include
)

# Link dependencies:
# - log: for Android logging
# - jnigraphics: for Android Bitmap pixel manipulation (AndroidBitmap_*)
# - vips, glib-2.0, gobject-2.0, gio-2.0: libvips runtime dependencies
target_link_libraries(vips_jni
    log
    jnigraphics
    vips
    glib-2.0
    gobject-2.0
    gio-2.0
)

# Compiler optimization options
target_compile_options(vips_jni PRIVATE
    -O3
    -ffast-math
    -fno-exceptions
    -fno-rtti
)

# Hide non-JNI symbols to minimize binary size and dynamic loading overhead
set_target_properties(vips_jni PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Enable Interprocedural Optimization / Link-Time Optimization (LTO) if supported
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported)
    set_target_properties(vips_jni PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

