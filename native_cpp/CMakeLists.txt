cmake_minimum_required(VERSION 3.15)

project(TransFlux LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含当前路径
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# 定义基本源文件集
set(PROJECT_SOURCES
    MediaConverter.h
    MediaConverter.cpp
    ImageConverter.h
    ImageConverter.cpp
    MediaStreamer.h
    MediaStreamer.cpp
    Tool.h
    Tool.cpp

    common/MediaQueue.h
    common/SystemClock.h
    common/Logger.h
    common/Logger.cpp
    common/Utils.h
    common/Utils.cpp
)

# 定义颜色变量
string(ASCII 27 Esc)
set(CYAN "${Esc}[36m")
set(GREEN "${Esc}[32m")
set(YELLOW "${Esc}[33m")
set(RED "${Esc}[31m")
set(RESET "${Esc}[0m")

# 是否开启Pybind11
option(ENABLE_PYBIND "enable pybind11 in order to create pyd files." OFF)
#set(ENABLE_PYBIND OFF)

# 条件编译
if(ENABLE_PYBIND)
    message(STATUS "${GREEN}Pybind11 Enabled.${RESET}")

    # PyBind11模块
    set(pybind11_DIR ${PROJECT_SOURCE_DIR}/pybind11/share/cmake/pybind11)
    find_package(pybind11 REQUIRED)

    pybind11_add_module(transflux
        ${PROJECT_SOURCES}
        BindClass.cpp
    )
    # 为代码添加ENABLE_PYBIND宏定义
    target_compile_definitions(transflux PRIVATE ENABLE_PYBIND)

    # 设置 pyd 文件的输出路径
    set_target_properties(transflux PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build"
    )

else()
    message(STATUS "${GREEN}Pybind11 Disabled.${RESET}")

    # QT文件工具
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)

    # 查找QT库
    find_package(Qt5 COMPONENTS Widgets Multimedia REQUIRED)

    # 创建可执行文件
    add_executable(${PROJECT_NAME}
        ${PROJECT_SOURCES}
        main.cpp
    )

    # 指定输出目录为项目的根目录
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../libs"
    )
endif()

include_directories(${PROJECT_SOURCE_DIR}/common)

# 区分平台添加依赖库
if(WIN32)
    # Windows平台的库目录
    include_directories(
        ${PROJECT_SOURCE_DIR}/FFmpeg/include
    )

    link_directories(
        ${PROJECT_SOURCE_DIR}/FFmpeg/lib
    )

    # 添加库
    set(ALL_LIBS
        ${PROJECT_SOURCE_DIR}/FFmpeg/lib/avformat.lib
        ${PROJECT_SOURCE_DIR}/FFmpeg/lib/avcodec.lib
        ${PROJECT_SOURCE_DIR}/FFmpeg/lib/avutil.lib
        ${PROJECT_SOURCE_DIR}/FFmpeg/lib/avdevice.lib
        ${PROJECT_SOURCE_DIR}/FFmpeg/lib/avfilter.lib
        ${PROJECT_SOURCE_DIR}/FFmpeg/lib/swscale.lib
        ${PROJECT_SOURCE_DIR}/FFmpeg/lib/swresample.lib
        ${PROJECT_SOURCE_DIR}/FFmpeg/lib/postproc.lib
        ws2_32
    )

else()

    # Linux平台的库
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale libswresample libavfilter)
    find_package(SDL2 REQUIRED)

    include_directories(${PROJECT_SOURCE_DIR}/SoundTouch/include)

    # 添加库
    set(ALL_LIBS
        PkgConfig::FFMPEG
    )
endif()

# 添加库
if(ENABLE_PYBIND)
    include_directories(${PROJECT_SOURCE_DIR}/pybind11/include)
    target_link_libraries(transflux PRIVATE ${ALL_LIBS})
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${ALL_LIBS}
        Qt5::Core
        Qt5::Widgets
        Qt5::Multimedia
    )
endif()

# Cuda加速
find_package(CUDA)
if(CUDA_FOUND)
    message(STATUS "${GREEN}CUDA successfully found!${RESET}")
    include_directories(${CUDA_INCLUDE_DIRS})
    # 为代码添加CUDA_ISAVAILABLE宏定义
    add_compile_definitions(CUDA_ISAVAILABLE)
else()
    message(WARNING "${YELLOW}CUDA not found, proceeding without CUDA support.${RESET}")
endif()
