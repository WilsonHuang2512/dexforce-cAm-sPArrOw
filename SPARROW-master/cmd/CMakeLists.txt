
find_package(Threads)

add_executable(sparrow_cmd
    cmd.cpp
    getopt.c
    projector_sharpness.cpp
    detect_chessboard.cpp
    ${FIRMWARE_DIR}/easylogging++.cc
    ${PROJECT_SOURCE_DIR}/gui/PrecisionTest.cpp
    ${PROJECT_SOURCE_DIR}/calibration/calibrate_function.cpp
    ${PROJECT_SOURCE_DIR}/test/reconstruct.cpp
    ${PROJECT_SOURCE_DIR}/test/AnalyseError.cpp
    ${PROJECT_SOURCE_DIR}/test/encode.cpp
    ${PROJECT_SOURCE_DIR}/test/solution.cpp
    ${PROJECT_SOURCE_DIR}/test/LookupTableFunction.cpp
    ${PROJECT_SOURCE_DIR}/test/FilterModule.cpp
    ${PROJECT_SOURCE_DIR}/test/support_function.cpp
    ${PROJECT_SOURCE_DIR}/test/triangulation.cpp
) 

target_link_libraries(sparrow_cmd PRIVATE
    ${OPENCV_3RDPARTY_LIBS}
    ${PROJECT_SOURCE_DIR}/3rdparty/GxIAPI/lib/GxIAPI.lib
    Threads::Threads
    sparrow_sdk
)

target_include_directories(sparrow_cmd PUBLIC
    ${PROJECT_SOURCE_DIR}/cmd
    ${PROJECT_SOURCE_DIR}/test
    ${PROJECT_SOURCE_DIR}/sdk
    ${PROJECT_SOURCE_DIR}/3rdparty/GxIAPI/include
    ${OPENCV_INCLUDE_DIR}
    ${FIRMWARE_DIR}
)

target_compile_definitions(sparrow_cmd PUBLIC
    NDEBUG
    _CONSOLE
    _CRT_SECURE_NO_WARNINGS
)

install(TARGETS sparrow_cmd
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
