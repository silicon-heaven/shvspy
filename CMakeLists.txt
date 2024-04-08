cmake_minimum_required(VERSION 3.18.4)
project(shvspy LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SHVSPY_USE_QT6 ON CACHE BOOL "Qt6 build")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(SHV_WARNING_FLAGS "-Wall -Wextra -pedantic -Wcast-align -Wunused -Wpedantic -Wmisleading-indentation -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough")

    # We switched to qsizetype in Qt6, so let's get rid of this warning in Qt5 for now.
    # TODO: Remove this after dropping support for Qt5
    if(SHVSPY_USE_QT6)
        set(SHV_WARNING_FLAGS "${SHV_WARNING_FLAGS} -Wconversion")
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            set(SHV_WARNING_FLAGS_CXX "-Wuseless-cast ${SHV_WARNING_FLAGS_CXX}")
        endif()
    endif()

    set(SHV_WARNING_FLAGS "${SHV_WARNING_FLAGS} -Wno-sign-conversion")
    set(SHV_WARNING_FLAGS_CXX "-Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual")

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(SHV_WARNING_FLAGS "-Wduplicated-cond -Wduplicated-branches -Wlogical-op ${SHV_WARNING_FLAGS}")
    endif()

    set(SHV_WARNING_FLAGS_CXX "${SHV_WARNING_FLAGS} ${SHV_WARNING_FLAGS_CXX}")

    set(CMAKE_C_FLAGS "${SHV_WARNING_FLAGS} ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${SHV_WARNING_FLAGS_CXX} ${CMAKE_CXX_FLAGS}")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_WIN32_EXECUTABLE ON) # Don't open a console window for Windows apps on Release mode.
endif()

if(EMSCRIPTEN)
    add_link_options("-sASYNCIFY")
    add_link_options("-Os")
endif()

if(SHVSPY_USE_LOCAL_NECROLOG)
    find_package(necrolog REQUIRED)
else()
    add_subdirectory(3rdparty/necrolog)
endif()

if(SHVSPY_USE_LOCAL_LIBSHV)
    find_package(libshv REQUIRED)
else()
    set(WITH_BROKER OFF CACHE BOOL "")
    set(LIBSHV_USE_QT6 "${SHVSPY_USE_QT6}" CACHE BOOL "")
    add_subdirectory(3rdparty/libshv)
endif()

if(NOT TARGET libshv::libshvvisu)
    message(FATAL_ERROR "libshvvisu not found. Including libshv might've failed due to missing Qt dependencies. Please check above for errors.")
endif()

if(SHVSPY_USE_QT6)
    if(EMSCRIPTEN)
        find_package(Qt6 COMPONENTS Core Widgets Gui Network WebSockets)
    else()
        find_package(Qt6 COMPONENTS Core Widgets Gui Network NetworkAuth SerialPort WebSockets)
    endif()
    set(Qt_FOUND ${Qt6_FOUND})
else()
    find_package(Qt5 QUIET COMPONENTS Core Widgets Gui Network SerialPort)
    set(Qt_FOUND ${Qt5_FOUND})

    # Qt5 doesn't have add qt_add_executable so this is a polyfill for that, since we already support Qt6.
    function(qt_add_executable)
        add_executable(${ARGV})
    endfunction()
endif()


if(Qt_FOUND)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)

    add_subdirectory(shvspy)
else()
    message(FATAL_ERROR "Qt${QT_VERSION_MAJOR} not found")
endif()
