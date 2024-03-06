# The Flutter tooling requires that developers have a version of Visual Studio
# installed that includes CMake 3.14 or later. You should not increase this
# version, as doing so will cause the plugin to fail to compile for some
# customers of the plugin.
cmake_minimum_required(VERSION 3.14)

# Project-level configuration.
set(PROJECT_NAME "trimmer")
project(${PROJECT_NAME} LANGUAGES CXX)

# Explicitly opt in to modern CMake behaviors to avoid warnings with recent
# versions of CMake.
cmake_policy(VERSION 3.14...3.25)

# This value is used when generating builds using this plugin, so it must
# not be changed
set(PLUGIN_NAME "trimmer_plugin")

# ------------------------------------------------------------------------------
# 从 video kit抄来的逻辑, 
# 1. 新建两个函数,一个下载,一个检测

# 下载+MD5检测
function(download_and_verify url md5 locationForArchive)
  # Check if the archive exists.
  if(EXISTS "${locationForArchive}")
    file(MD5 "${locationForArchive}" ARCHIVE_MD5)

    # If MD5 doesn't match, delete the archive to download again.
    if(md5 STREQUAL "")
      message(STATUS "md5 为空 跳过验证${locationForArchive}")
    elseif(NOT md5 STREQUAL ARCHIVE_MD5)
      file(REMOVE "${locationForArchive}")
      message(STATUS "MD5 mismatch. File deleted.")
    endif()
  endif()

  # Download the archive if it doesn't exist.
  if(NOT EXISTS "${locationForArchive}")
    message(STATUS "Downloading archive from ${url}...")
    file(DOWNLOAD "${url}" "${locationForArchive}")
    message(STATUS "Downloaded archive to ${locationForArchive}.")

    # Verify MD5 of the newly downloaded file.
    file(MD5 "${locationForArchive}" ARCHIVE_MD5)
    message(STATUS "${locationForArchive} 的md5是 ${ARCHIVE_MD5}")

    if(md5 STREQUAL "")
      message(STATUS "md5 为空 跳过验证${locationForArchive}")
    elseif(md5 STREQUAL ARCHIVE_MD5)
      message(STATUS "${locationForArchive} Verification successful.")
    else()
      message(FATAL_ERROR "${locationForArchive} Integrity check failed, please try to re-build project again.")
    endif()
  endif()
endfunction()
# 检查文件夹是不是存在
function(check_directory_exists_and_not_empty dir result_var)
  # Check if the directory exists
  if(EXISTS "${dir}")
    # Check if the directory is not empty
    file(GLOB dir_contents "${dir}/*")

    if(dir_contents)
      set(${result_var} TRUE PARENT_SCOPE)
    else()
      set(${result_var} FALSE PARENT_SCOPE)
      message(STATUS "Directory ${dir} exists but is empty!")
    endif()
  else()
    set(${result_var} FALSE PARENT_SCOPE)
    message(STATUS "Directory ${dir} does not exist!")
  endif()
endfunction()

# 下载包名
set(LIBFFMPEG "ffmpeg-n6.1-latest-win64-gpl-shared-6.1.zip")
set(LIBFFMPEG_UNZIP_DIR_NAME "ffmpeg-n6.1-latest-win64-gpl-shared-6.1")

# 下载地址, 以及md5
# https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2024-03-05-12-49/ffmpeg-n6.1.1-16-g33efa50fa4-win64-gpl-6.1.zip
set(LIBFFMPEG_URL "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/${LIBFFMPEG}")
set(LIBFFMPEG_MD5 "")

# 下载到哪里, CMAKE_BINARY_DIR是cmake的输出位置
set(LIBFFMPEG_ARCHIVE "${CMAKE_BINARY_DIR}/${LIBFFMPEG}")
set(LIBFFMPEG_SRC "${CMAKE_BINARY_DIR}/ffmpeg")

download_and_verify(
  ${LIBFFMPEG_URL}
  "${LIBFFMPEG_MD5}"
  ${LIBFFMPEG_ARCHIVE}
)

check_directory_exists_and_not_empty(${LIBFFMPEG_SRC} LIBFFMPEG_SRC_VALID)

# 解压缩, 移动文件
if(NOT LIBFFMPEG_SRC_VALID)
  message(STATUS "Extracting ${LIBFFMPEG}...")
  make_directory("${LIBFFMPEG_SRC}")
  add_custom_target("${PROJECT_NAME}_LIBFFMPEG_EXTRACT" ALL)
  add_custom_command(
    TARGET "${PROJECT_NAME}_LIBFFMPEG_EXTRACT"
    # COMMAND "${CMAKE_COMMAND}" -E tar xzf "\"${LIBFFMPEG_ARCHIVE}\""
    COMMAND unzip "${LIBFFMPEG_ARCHIVE}" -d "${LIBFFMPEG_SRC}"
    COMMAND xcopy "\"${LIBFFMPEG_SRC}/${LIBFFMPEG_UNZIP_DIR_NAME}/include\"" "\"${LIBFFMPEG_SRC}/include\"" /E /H /C /I
    COMMAND xcopy "\"${LIBFFMPEG_SRC}/${LIBFFMPEG_UNZIP_DIR_NAME}/lib\"" "\"${LIBFFMPEG_SRC}/lib\"" /E /H /C /I
    COMMAND xcopy "\"${LIBFFMPEG_SRC}/${LIBFFMPEG_UNZIP_DIR_NAME}/bin\"" "\"${LIBFFMPEG_SRC}\"" /E /H /C /I
    COMMAND rmdir "\"${LIBFFMPEG_SRC}/${LIBFFMPEG_UNZIP_DIR_NAME}\"" /S /Q
    # COMMAND ren "\"${LIBFFMPEG_SRC}/mpv\"" "\"include\""
    WORKING_DIRECTORY "${LIBFFMPEG_SRC}"
  )
endif()

# ------------------------------------------------------------------------------






include_directories("${LIBFFMPEG_SRC}/include")
# Any new source files that you add to the plugin should be added here.
list(APPEND PLUGIN_SOURCES
  "trimmer_plugin.cpp"
  "trimmer_plugin.h"
)

# Define the plugin library target. Its name must not be changed (see comment
# on PLUGIN_NAME above).
add_library(${PLUGIN_NAME} SHARED
  "include/trimmer/trimmer_plugin_c_api.h"
  "trimmer_plugin_c_api.cpp"
  ${PLUGIN_SOURCES}
)

# Apply a standard set of build settings that are configured in the
# application-level CMakeLists.txt. This can be removed for plugins that want
# full control over build settings.
apply_standard_settings(${PLUGIN_NAME})

# Symbols are hidden by default to reduce the chance of accidental conflicts
# between plugins. This should not be removed; any symbols that should be
# exported should be explicitly exported with the FLUTTER_PLUGIN_EXPORT macro.
set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)

# Source include directories and library dependencies. Add any plugin-specific
# dependencies here.
# include_directories("${LIBFFMPEG_SRC}/include")
target_include_directories(${PLUGIN_NAME} 
  INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include"
  INTERFACE "${LIBFFMPEG_SRC}/include"
)

target_link_libraries(
  ${PLUGIN_NAME} PRIVATE 
  flutter 
  flutter_wrapper_plugin
  # ff link
  "${LIBFFMPEG_SRC}/lib/avcodec.lib"
)
link_directories(${LIBFFMPEG_SRC})
# List of absolute paths to libraries that should be bundled with the plugin.
# This list could contain prebuilt libraries, or libraries created by an
# external build triggered from this build file.
set(trimmer_bundled_libraries
  "${LIBFFMPEG_SRC}/avcodec-60.dll"
  "${LIBFFMPEG_SRC}/avdevice-60.dll"
  "${LIBFFMPEG_SRC}/avfilter-9.dll"
  "${LIBFFMPEG_SRC}/avformat-60.dll"
  "${LIBFFMPEG_SRC}/avutil-58.dll"
  "${LIBFFMPEG_SRC}/postproc-57.dll"
  "${LIBFFMPEG_SRC}/swresample-4.dll"
  "${LIBFFMPEG_SRC}/swscale-7.dll"
  PARENT_SCOPE
)

message(STATUS "${LIBFFMPEG_SRC}/avcodec-60.dll")


# === Tests ===
# These unit tests can be run from a terminal after building the example, or
# from Visual Studio after opening the generated solution file.

# Only enable test builds when building the example (which sets this variable)
# so that plugin clients aren't building the tests.
if (${include_${PROJECT_NAME}_tests})
set(TEST_RUNNER "${PROJECT_NAME}_test")
enable_testing()

# Add the Google Test dependency.
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/release-1.11.0.zip
)
# Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# Disable install commands for gtest so it doesn't end up in the bundle.
set(INSTALL_GTEST OFF CACHE BOOL "Disable installation of googletest" FORCE)
FetchContent_MakeAvailable(googletest)

# The plugin's C API is not very useful for unit testing, so build the sources
# directly into the test binary rather than using the DLL.
add_executable(${TEST_RUNNER}
  test/trimmer_plugin_test.cpp
  ${PLUGIN_SOURCES}
)
apply_standard_settings(${TEST_RUNNER})
target_include_directories(${TEST_RUNNER} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(${TEST_RUNNER} PRIVATE flutter_wrapper_plugin)
target_link_libraries(${TEST_RUNNER} PRIVATE gtest_main gmock)
# flutter_wrapper_plugin has link dependencies on the Flutter DLL.
add_custom_command(TARGET ${TEST_RUNNER} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  "${FLUTTER_LIBRARY}" $<TARGET_FILE_DIR:${TEST_RUNNER}>
)

# Enable automatic test discovery.
include(GoogleTest)
gtest_discover_tests(${TEST_RUNNER})
endif()
