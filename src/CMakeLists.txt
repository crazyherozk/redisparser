# if(WIN32)
#     set(CMAKE_EXTRA_INCLUDE_FILES winsock2.h ws2tcpip.h)
#     set(CMAKE_REQUIRED_LIBRARIES  ws2_32.lib)
#     set(CMAKE_REQUIRED_DEFINITIONS -FIwinsock2.h -FIws2tcpip.h)
# endif()

set(HDR_REDISPROTOLIB
    ./RedisProto.h
    )

 set(SRC_REDISPROTOLIB
    ./RedisProto.c
    )

    
set(LIB_PLATFORM)

source_group("Headers"  FILES ${HDR_REDISPROTOLIB})
source_group("Source"   FILES ${SRC_REDISPROTOLIB})



if("${LIB_BUILD_TYPE}" STREQUAL "SHARED")
    message(STATUS "build shared library")
    add_library(RedisProto SHARED ${HDR_REDISPROTOLIB} ${SRC_REDISPROTOLIB})
    if(APPLE)
        message(STATUS "set INSTALL_RPATH for shared library on macOS")
        set_target_properties(RedisProto PROPERTIES INSTALL_RPATH "@loader_path/../lib")
    endif()

    add_framework(CoreFoundation RedisProto)
    target_link_libraries(RedisProto ${LIB_PLATFORM})
else()
    message(STATUS "build static library")
    add_library(RedisProto STATIC ${HDR_REDISPROTOLIB} ${SRC_REDISPROTOLIB})
endif()


