
find_package(Threads)

add_executable(sparrow_test
    test.cpp
    reconstruct.cpp
    AnalyseError.cpp
    encode.cpp
    solution.cpp
    LookupTableFunction.cpp
    FilterModule.cpp
    support_function.cpp
    triangulation.cpp
    ${PROJECT_SOURCE_DIR}/sdk/socket_tcp.cpp
    ${PROJECT_SOURCE_DIR}/sdk/utils.cpp
    ${PROJECT_SOURCE_DIR}/cmd/getopt.c
    ${FIRMWARE_DIR}/easylogging++.cc
    ${PROJECT_SOURCE_DIR}/gui/PrecisionTest.cpp
    ${PROJECT_SOURCE_DIR}/calibration/calibrate_function.cpp
)

target_include_directories(sparrow_test PRIVATE
    ${OPENCV_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/cmd/
    ${PROJECT_SOURCE_DIR}/sdk/
    ${FIRMWARE_DIR}
    ${PROJECT_SOURCE_DIR}/gui/
    ${PROJECT_SOURCE_DIR}/calibration/
)

target_link_libraries(sparrow_test PRIVATE
    Threads::Threads
    ${OPENCV_3RDPARTY_LIBS}
    sparrow_sdk
)

install(TARGETS sparrow_test
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
