cmake_minimum_required(VERSION 3.4.1)

set(MY_ROOT_DIR ../../..)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -std=c++11 -Wno-inconsistent-missing-override")


set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")

#---------- fix for missing <asm/io.h> ----------
include_directories(src/main/jni/ndk_headers_wrapper)

#---------- Raknet ---------------

set(MY_RAKNET_DIR ${MY_ROOT_DIR}/third-party/RakNet/Source)

file(GLOB MY_RAKNET_SRC_FILES ${MY_RAKNET_DIR}/*.cpp)

add_library(RakNet

            STATIC

            ${MY_RAKNET_SRC_FILES}
            )

set_target_properties(RakNet PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} -DRAKNET_ENABLE_SOCKET_ADDR_PORT_REUSE")

#---------- miniupnpc -------------

set(MY_MINIUPNP_DIR ${MY_ROOT_DIR}/third-party/miniupnp/miniupnpc)


set(MY_MINIUPNP_INCLUDES src/main/jni ${MY_MINIUPNP_DIR})

set(MY_MINIUPNP_CFLAGS "-DMINIUPNPC_SET_SOCKET_TIMEOUT \
                        -DMINIUPNPC_GET_SRC_ADDR \
                        -D_BSD_SOURCE \
                        -D_DEFAULT_SOURCE \
                        -D_POSIX_C_SOURCE=200112L \
                        -D_DARWIN_C_SOURCE \
                        -D_XOPEN_SOURCE=600 \
                        -DMINIUPNP_STATICLIB"
    )

set(MY_MINIUPNP_SRC_FILES ${MY_MINIUPNP_DIR}/igd_desc_parse.c
                          ${MY_MINIUPNP_DIR}/miniupnpc.c
                          ${MY_MINIUPNP_DIR}/minissdpc.c
                          ${MY_MINIUPNP_DIR}/minixml.c
                          ${MY_MINIUPNP_DIR}/minisoap.c
                          ${MY_MINIUPNP_DIR}/miniwget.c
                          ${MY_MINIUPNP_DIR}/upnpcommands.c
                          ${MY_MINIUPNP_DIR}/upnpreplyparse.c
                          ${MY_MINIUPNP_DIR}/upnperrors.c
                          ${MY_MINIUPNP_DIR}/connecthostport.c
                          ${MY_MINIUPNP_DIR}/portlistingparse.c
                          ${MY_MINIUPNP_DIR}/receivedata.c
                          ${MY_MINIUPNP_DIR}/upnpdev.c
                          ${MY_MINIUPNP_DIR}/listdevices.c
     )

add_library(miniupnpc

            STATIC

            ${MY_MINIUPNP_SRC_FILES}
            )

set_target_properties(miniupnpc PROPERTIES COMPILE_FLAGS "${CMAKE_C_FLAGS} ${MY_MINIUPNP_CFLAGS}")

target_include_directories(miniupnpc PRIVATE ${MY_MINIUPNP_INCLUDES})

#---------- RemoteController -----------------

set(MY_RCTL_SRC_DIR  ${MY_ROOT_DIR}/third-party/RemoteController/android)
set(MY_RCTL_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/RemoteController )
file(MAKE_DIRECTORY ${MY_RCTL_BUILD_DIR})

add_subdirectory( ${MY_RCTL_SRC_DIR}

                  ${MY_RCTL_BUILD_DIR} )

#---------- ImagesLoader ---------

set(MY_IMGLOADER_SRC_DIR  ${MY_ROOT_DIR}/third-party/HQFramework/ImagesLoader)
set(MY_IMGLOADER_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/ImagesLoader )
file(MAKE_DIRECTORY ${MY_IMGLOADER_BUILD_DIR})

add_subdirectory( ${MY_IMGLOADER_SRC_DIR}

                  ${MY_IMGLOADER_BUILD_DIR} )


#---------- emucore ----------------

set(MY_CORE_SRC_DIR  ${MY_ROOT_DIR}/source/core)
set(MY_CORE_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/emucore )
file(MAKE_DIRECTORY ${MY_CORE_BUILD_DIR})

add_subdirectory( ${MY_CORE_SRC_DIR}

                  ${MY_CORE_BUILD_DIR} )

#---------- Main module ---------

set(MY_INCLUDES ${MY_ROOT_DIR}/source
                ${MY_ROOT_DIR}/third-party
                ${MY_ROOT_DIR}/third-party/HQFramework
                ${MY_ROOT_DIR}/third-party/HQFramework/ImagesLoader
                ${MY_ROOT_DIR}/third-party/RakNet/Source)

set(MY_SRC_FILES    ${MY_ROOT_DIR}/source/mobile/MutableTextureGLES.cpp
                    ${MY_ROOT_DIR}/source/mobile/TextureGLES.cpp
                    ${MY_ROOT_DIR}/source/mobile/InputGLES.cpp
                    ${MY_ROOT_DIR}/source/mobile/RendererGLES.cpp
                    ${MY_ROOT_DIR}/source/mobile/GLESUtils.cpp
                    ${MY_ROOT_DIR}/source/mobile/Input.cpp
                    ${MY_ROOT_DIR}/source/mobile/Renderer.cpp
                    ${MY_ROOT_DIR}/source/mobile/Common.cpp
                    ${MY_ROOT_DIR}/source/mobile/NesSystemWrapper.cpp
                    ${MY_ROOT_DIR}/source/mobile/android/Game_jni.cpp
                    ${MY_ROOT_DIR}/source/mobile/android/JniUtils.cpp
                    ${MY_ROOT_DIR}/source/mobile/android/AudioDriver.cpp
                    ${MY_ROOT_DIR}/source/remote_control/ConnectionHandlerRakNet.cpp
     )

add_library(nes

            SHARED

            ${MY_SRC_FILES}
            )


target_include_directories(nes PRIVATE ${MY_INCLUDES})

target_link_libraries(nes

                      z log GLESv2 android OpenSLES

                      emucore ImagesLoader RakNet miniupnpc RemoteController)