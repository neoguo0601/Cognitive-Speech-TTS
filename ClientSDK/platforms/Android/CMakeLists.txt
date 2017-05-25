# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

include_directories("${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/3rdparty/curl/include" "${PROJECT_SOURCE_DIR}/3rdparty/silk/include")

add_library(TTSClientSDK SHARED ${PROJECT_SOURCE_DIR}/source/TTSClientSDK.c ${PROJECT_SOURCE_DIR}/source/native-lib.c)

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin/Android/armeabi-v7a/${CMAKE_BUILD_TYPE}) 

find_library(log-lib log)

target_link_libraries(TTSClientSDK ${log-lib})
target_link_libraries(TTSClientSDK z)
target_link_libraries(TTSClientSDK ${PROJECT_SOURCE_DIR}/3rdparty/curl/lib/Android/android-16/armeabi-v7a/libcurl.a)
target_link_libraries(TTSClientSDK ${PROJECT_SOURCE_DIR}/3rdparty/silk/lib/Android/android-21/arm-linux-androideabi/libsilk.a)
