cmake_minimum_required(VERSION 3.10)
project(camera_server_CPU LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REF_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../firmware_GPU/GPU/camera_server)

# 添加源文件
set(SOURCES
    camera_server_new.cpp
    ${REF_SOURCE_DIR}/camera_galaxy.cpp
    "${REF_SOURCE_DIR}/critical zone.cpp"
    ${REF_SOURCE_DIR}/easylogging++.cc
    encode.cpp
    FilterModule.cpp
    ${REF_SOURCE_DIR}/lightcrafter3010.cpp
    LookupTableFunction.cpp
    management.cpp
    merge_hdr.cpp
    #minsw.cpp
    reconstruct.cpp
    scan3d.cpp
    ${REF_SOURCE_DIR}/set_calib_looktable.cpp
    ${REF_SOURCE_DIR}/socket_tcp.cpp
    ${REF_SOURCE_DIR}/system_config_settings.cpp
    ${REF_SOURCE_DIR}/triangulation.cpp
)

# 生成可执行文件
add_executable(camera_server_CPU ${SOURCES})


# 添加头文件目录
target_include_directories(camera_server_CPU PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/Galaxy/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/opencv/include
    ${REF_SOURCE_DIR}
)

# 添加库目录
link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/Galaxy/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/opencv/lib
)

# 添加第三方库lin
set(GALAXY_LIB_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/Galaxy/lib)
set(OPENCV_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/opencv/lib)

set(THIRD_PARTY_LIBS
    ${GALAXY_LIB_DIR}/GxIAPI.lib
    ${OPENCV_LIB_DIR}/opencv_world480.lib
)

# 链接库
target_link_libraries(camera_server_CPU 
    PRIVATE
    ${THIRD_PARTY_LIBS}
)



