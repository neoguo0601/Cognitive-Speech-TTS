md TTSClientSDK_armeable-v7a
cd TTSClientSDK_armeable-v7a

cmake -DCMAKE_TOOLCHAIN_FILE="..\android.toolchain.cmake" -DANDROID_NDK="C:\Users\v-guoya\AppData\Local\Android\sdk\ndk-bundle" -DCMAKE_BUILD_TYPE=DEBUG -DANDROID_ABI="armeabi-v7a" -DANDROID_NATIVE_API_LEVEL="android-16 for ARM" -DCMAKE_MAKE_PROGRAM="C:\Users\v-guoya\AppData\Local\Android\sdk\ndk-bundle\prebuilt\windows-x86_64\bin\make.exe" -G"MinGW Makefiles" ../

cmake --build .