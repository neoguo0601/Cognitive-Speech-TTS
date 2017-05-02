# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

SET(PROJECT_SOURCE_DIR "../..")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/curl/include")

add_library(native-lib SHARED ${CMAKE_CURRENT_SOURCE_DIR}/source/TTSClientSDK.c)

set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin/Android/armeabi-v7a/${CMAKE_BUILD_TYPE}) 

find_library(log-lib log)

target_link_libraries(native-lib ${log-lib})
target_link_libraries(native-lib z)
target_link_libraries(native-lib ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/curl/lib/Android/android-16/armeabi-v7a/libcurl.a)
target_link_libraries(native-lib ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/silk/lib/Android/android-21/arm-linux-androideabi/libsilk.a)
