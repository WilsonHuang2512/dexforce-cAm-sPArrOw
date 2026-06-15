include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/ConfiguringIP/)

find_package(Threads)

add_library(sparrow_sdk SHARED 
    sparrow_sdk.cpp 
    ${PROJECT_SOURCE_DIR}/test/triangulation.cpp 
    socket_tcp.cpp
    utils.cpp 
    ${FIRMWARE_DIR}/easylogging++.cc
)

target_include_directories(sparrow_sdk PRIVATE         
	${FIRMWARE_DIR}
)

target_link_libraries(sparrow_sdk PRIVATE
 Threads::Threads
)

install(TARGETS sparrow_sdk
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
