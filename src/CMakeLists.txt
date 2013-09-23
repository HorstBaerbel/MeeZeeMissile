cmake_minimum_required(VERSION 2.6)

project(meezee)

#-------------------------------------------------------------------------------
#add local find scripts to CMAKE path
LIST(APPEND CMAKE_MODULE_PATH
#    ${CMAKE_CURRENT_SOURCE_DIR}/CMake/Utils
    ${CMAKE_CURRENT_SOURCE_DIR}/CMake/Packages
)

find_package(OpenCV REQUIRED)
find_package(libusb-1.0 REQUIRED)

#-------------------------------------------------------------------------------
#check if we're running on Raspberry Pi
MESSAGE("Looking for bcm_host.h")
if(EXISTS "/opt/vc/include/bcm_host.h")
    MESSAGE("bcm_host.h found")
    set(BCMHOST found)
else()
    MESSAGE("bcm_host.h not found")
endif()

#-------------------------------------------------------------------------------
#set up compiler flags and excutable names
if(DEFINED BCMHOST)
    add_definitions(-DBCMHOST)
endif()

if(MSVC)
    set(CMAKE_DEBUG_POSTFIX "d")
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP") #multi-processor compilation
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP") #multi-processor compilation
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    #check for G++ 4.7+
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE G++_VERSION)
    if (G++_VERSION VERSION_LESS 4.7)
            message(SEND_ERROR "You need at least G++ 4.7 to compile!")
    endif()
    #set up compiler flags for GCC
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2") #support C++11 for std::, optimize
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")  #strip binary
endif()

#-------------------------------------------------------------------------------
#add include directories
if(DEFINED BCMHOST)
    LIST(APPEND EXTRA_INCLUDE_DIRS
        "/opt/vc/include"
        "/opt/vc/include/interface/vcos"
        "/opt/vc/include/interface/vmcs_host/linux"
        "/opt/vc/include/interface/vcos/pthreads"
    )
endif()

LIST(APPEND EXTRA_INCLUDE_DIRS
    ${OPENCV_INCLUDE_DIR}
    ${LIBUSB_1_INCLUDE_DIRS}/libusb-1.0
)

#-------------------------------------------------------------------------------
#define basic sources and headers
set(TARGET_HEADERS
    consolestyle.h
    framebuffer.h
    keyboard.h
    missilecontrol.h
    motiondetector.h
)
set(TARGET_SOURCES
    consolestyle.cpp
    framebuffer.cpp
    keyboard.cpp
    missilecontrol.cpp
    motiondetector.cpp
    main.cpp
)

#-------------------------------------------------------------------------------
#define libraries and directories

LIST(APPEND TARGET_LINK_DIRECTORIES
    ${OPENCV_LIBS}
)

if(DEFINED BCMHOST)
    LIST(APPEND TARGET_LINK_DIRECTORIES
        "/opt/vc/lib"
    )
#    LIST(APPEND TARGET_LIBRARIES
#        bcm_host
#    )
endif()

LIST(APPEND TARGET_LIBRARIES
    opencv_core
    opencv_imgproc
    opencv_video
    opencv_highgui
    ${LIBUSB_1_LIBRARIES}
)

#-------------------------------------------------------------------------------
#set up build directories
set(dir ${CMAKE_CURRENT_SOURCE_DIR}/../build)
set(EXECUTABLE_OUTPUT_PATH ${dir} CACHE PATH "Build directory" FORCE)
set(LIBRARY_OUTPUT_PATH ${dir} CACHE PATH "Build directory" FORCE)

#-------------------------------------------------------------------------------
#define target
include_directories(${EXTRA_INCLUDE_DIRS})
add_executable(meezee ${TARGET_SOURCES} ${TARGET_HEADERS})
link_directories(${TARGET_LINK_DIRECTORIES})
target_link_libraries(meezee ${TARGET_LIBRARIES})

#special properties for windows builds
if(MSVC)
    #show console in debug builds, but not in proper release builds
    #Note that up to CMake 2.8.11.2 this feature is broken: http://public.kitware.com/Bug/view.php?id=12566
    set_target_properties(meezee PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
    set_target_properties(meezee PROPERTIES COMPILE_DEFINITIONS_DEBUG "_CONSOLE")
    set_target_properties(meezee PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:CONSOLE")
    set_target_properties(meezee PROPERTIES COMPILE_DEFINITIONS_RELWITHDEBINFO "_CONSOLE")
    set_target_properties(meezee PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
    set_target_properties(meezee PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:WINDOWS")
endif()

