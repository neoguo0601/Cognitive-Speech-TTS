md TTSClientSDK_armeabi-v7a
cd TTSClientSDK_armeabi-v7a

cmake -DCMAKE_TOOLCHAIN_FILE="..\android.toolchain.cmake" -DANDROID_NDK=<ndk_path> -DCMAKE_BUILD_TYPE=DEBUG -DANDROID_ABI="armeabi-v7a" -DANDROID_NATIVE_API_LEVEL="android-16 for ARM" -DCMAKE_MAKE_PROGRAM=<make_program_path> -G"MinGW Makefiles" ../

cmake --build .