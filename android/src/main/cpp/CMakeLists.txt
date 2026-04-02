cmake_minimum_required(VERSION 3.10)
project(heic_native_android)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Paths relative to this CMakeLists.txt
set(JNILIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}")
set(THIRD_PARTY_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/include")

# Import prebuilt shared libraries
add_library(libheif SHARED IMPORTED)
set_target_properties(libheif PROPERTIES
    IMPORTED_LOCATION "${JNILIBS_DIR}/libheif.so")

add_library(libde265 SHARED IMPORTED)
set_target_properties(libde265 PROPERTIES
    IMPORTED_LOCATION "${JNILIBS_DIR}/libde265.so")

add_library(libpng16 SHARED IMPORTED)
set_target_properties(libpng16 PROPERTIES
    IMPORTED_LOCATION "${JNILIBS_DIR}/libpng16.so")

# Build the plugin shared library
add_library(heic_native_android SHARED
    heic_native_core.cpp
    heic_native_jni.cpp)

target_include_directories(heic_native_android PRIVATE
    ${THIRD_PARTY_INCLUDE_DIR})

target_link_libraries(heic_native_android
    libheif
    libde265
    libpng16
    z
    log)
