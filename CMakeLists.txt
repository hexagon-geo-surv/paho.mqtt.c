#[[
//--------------------------------------------------------//
// Copyright (c) 2015-2022 Hexagon Technology Center GmbH
// 9435 Heerbrugg, Switzerland
// http://www.hexagon.com
//
// Unauthorized use or distribution prohibited. Access to
// and use of this code is permitted only under written
// permission from Hexagon Technology Center GmbH.
// All Rights Reserved.
//--------------------------------------------------------//
]]

# Sources
file(GLOB SRCS_G "src/*.cpp" "src/*.c")
list(REMOVE_ITEM SRCS_G "${CMAKE_CURRENT_SOURCE_DIR}/src/MQTTAsync.c")
list(REMOVE_ITEM SRCS_G "${CMAKE_CURRENT_SOURCE_DIR}/src/MQTTAsyncUtils.c")
list(REMOVE_ITEM SRCS_G "${CMAKE_CURRENT_SOURCE_DIR}/src/MQTTVersion.c")

POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "include/*.h" "include/*.hpp")
POCO_HEADERS_AUTO(SRCS ${HDRS_G})

add_library(Paho ${LIB_MODE} ${SRCS})
add_library(Poco::Paho ALIAS Paho)

if (POCO_STATIC)
    target_compile_definitions(Paho PRIVATE PAHO_MQTT_STATIC)
else ()
    target_compile_definitions(Paho PRIVATE PAHO_MQTT_EXPORTS=1)
endif ()

target_link_libraries(Paho PRIVATE OpenSSL::SSL)

if (WIN32)
    target_link_libraries(Paho PRIVATE ws2_32)
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # set the OSX define, to make it working on MAC OS and iOS
    # see ...Paho/MakeFile
    target_compile_definitions(Paho PRIVATE OSX)
    target_compile_options(Paho PRIVATE -Wno-deprecated-declarations)
endif ()

target_compile_definitions(Paho PRIVATE OPENSSL)
if (ANDROID)
    if ((${CMAKE_SYSTEM_VERSION} GREATER 23) OR FORCE_PAHO_ANDROID_API_23_PLUS)
        target_compile_definitions(Paho PRIVATE POCO_NO_LINUX_IF_PACKET_H POCO_SET_NETWORKINTERFACE)
    endif ()
else ()
    target_compile_definitions(Paho PRIVATE POCO_SET_NETWORKINTERFACE)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    # PAHO: disable heap tracking
    target_compile_definitions(Paho PRIVATE HIGH_PERFORMANCE)
endif ()

# add the compile definition to disable the Paho persistence
target_compile_definitions(Paho PRIVATE NO_PERSISTENCE)

target_include_directories(Paho
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
        )

POCO_INSTALL(Paho)