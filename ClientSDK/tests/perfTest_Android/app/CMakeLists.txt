cmake_minimum_required(VERSION 3.4.1)

include_directories("src/main/cpp/include")

add_library(TTSClientSDK SHARED src/main/cpp/native-lib.c src/main/cpp/src/TTSClientSDK.c)

find_library(log-lib log)
find_library(z-lib z)

target_link_libraries(TTSClientSDK ${log-lib})
target_link_libraries(TTSClientSDK ${z-lib})
target_link_libraries(TTSClientSDK D:/Android/TTSClientSDK/app/src/main/cpp/libs/armeabi-v7a/libcurl.a)
target_link_libraries(TTSClientSDK D:/Android/TTSClientSDK/app/src/main/cpp/libs/armeabi-v7a/libsilk.a)