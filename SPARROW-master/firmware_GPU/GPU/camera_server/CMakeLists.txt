cmake_minimum_required(VERSION 3.10)
project(camera_server_GPU LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CUDAToolkit 11.8 REQUIRED)

# 添加源文件
set(SOURCES
    camera_server_new.cpp
    camera_galaxy.cpp
	"critical zone.cpp"
	easylogging++.cc
	encode.cu
	filter_module.cu
	lightcrafter3010.cpp
	LookupTableFunction.cpp
	management.cu
	merge_hdr.cu
	merge_repetition.cu
	minsw.cu
	reconstruct.cu
	scan3d.cpp
	set_calib_looktable.cpp
	socket_tcp.cpp
	system_config_settings.cpp
	triangulation.cpp
)

# 生成可执行文件
add_executable(camera_server_GPU ${SOURCES})


# 添加头文件目录
target_include_directories(camera_server_GPU
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/Galaxy/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/opencv/include
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
target_link_libraries(camera_server_GPU PRIVATE
	${THIRD_PARTY_LIBS}
)



