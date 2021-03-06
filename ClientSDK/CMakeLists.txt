cmake_minimum_required(VERSION 2.8)

PROJECT(TTSClientSDK)

MESSAGE(STATUS "Project: TTSClientSDK")
MESSAGE(STATUS "Project Directory: ${PROJECT_SOURCE_DIR}")

IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    MESSAGE(STATUS "current platform: Linux ")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    MESSAGE(STATUS "current platform: Windows")
ELSE ()  
    MESSAGE(STATUS "other platform: ${CMAKE_SYSTEM_NAME}") 
ENDIF (CMAKE_SYSTEM_NAME MATCHES "Linux") 


IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Debug)
ENDIF(NOT CMAKE_BUILD_TYPE)


SET(Compiler_Flags_CPP "-std=gnu++1y -Wconversion-null ${Compiler_Flags_Common}")
SET(Compiler_Flags_C " ${Compiler_Flags_Common}")

SET(Compiler_Flags_Common_Debug "-O0 -g -fms-extensions -frtti -Wno-unused-value -Wno-comment -Wint-to-pointer-cast -fpermissive")
SET(Compiler_Flags_Common_Release "-O3 -DNDEBUG -fms-extensions -frtti -Wno-unused-value -Wno-comment -Wint-to-pointer-cast -fpermissive")

SET(CMAKE_C_FLAGS_DEBUG "${Compiler_Flags_C} ${Compile_Options_Debug}")
SET(CMAKE_C_FLAGS_RELEASE "${Compiler_Flags_C} ${Compile_Options_Release}")


IF(NOT CPU_PLATFORM)
    IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
        SET(CPU_PLATFORM "x64")
    ELSE ()  
        SET(CPU_PLATFORM "x86")
    ENDIF (CMAKE_SYSTEM_NAME MATCHES "Linux") 
ENDIF(NOT CPU_PLATFORM)


IF(CPU_PLATFORM MATCHES x86)
    MESSAGE(STATUS "CPU platform: ${CPU_PLATFORM}")
    IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")  
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")  
    ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")
    SET(CPU_PLATFORM "x86")
ELSE(CPU_PLATFORM MATCHES x64)
    MESSAGE(STATUS "CUP platform: ${CPU_PLATFORM}")
    SET(CPU_PLATFORM "x64")
ENDIF(CPU_PLATFORM MATCHES x86)  


IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/silk/include" "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/curl/include")
    add_definitions(-D_GNU_SOURCE)
    set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/${CPU_PLATFORM}/${CMAKE_BUILD_TYPE}) 
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/silk/include" "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/curl/include")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/${CPU_PLATFORM}) 
ELSE ()  
    MESSAGE(STATUS "other platform: ${CMAKE_SYSTEM_NAME}") 
ENDIF (CMAKE_SYSTEM_NAME MATCHES "Linux") 


set(TTSClientSDK_src ${CMAKE_CURRENT_SOURCE_DIR}/source/TTSClientSDK.c)


add_library(TTSSDKsample STATIC ${TTSClientSDK_src})
set_target_properties(TTSSDKsample PROPERTIES OUTPUT_NAME "libTTSClient")

