cmake_minimum_required(VERSION 3.16)

project(motioncam-fs VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network) # Keep for compatibility if needed

set(DEPENDENCIES_PATH ${CMAKE_CURRENT_SOURCE_DIR}/deps) # Use CMAKE_CURRENT_SOURCE_DIR
# set(Boost_DEBUG 1) # Usually not needed unless debugging Boost's find mechanism

set(PROJECT_SOURCES
        src/main.cpp
        src/mainwindow.cpp
        src/VirtualFileSystemImpl_MCRAW.cpp
        src/CameraMetadata.cpp
        src/CameraFrameMetadata.cpp
        src/AudioWriter.cpp
        src/Utils.cpp
        src/SettingsDialog.cpp

        include/mainwindow.h
        include/Types.h
        include/IVirtualFileSystem.h
        include/IFuseFileSystem.h
        include/VirtualFileSystemImpl_MCRAW.h
        include/LRUCache.h
        include/AudioWriter.h
        include/Measure.h
        include/SingleApplication.h
        include/CameraMetadata.h
        include/CameraFrameMetadata.h
        include/Utils.h
        include/SettingsDialog.h
        include/MatrixProfile.h

        ui/mainwindow.ui
        ui/SettingsDialog.ui
)

qt_add_resources(PROJECT_SOURCES resources.qrc)

set(platform-specific "") # Initialize

if(WIN32)
    list(APPEND PROJECT_SOURCES
        resources/app.rc

        src/win/fusefilesystemimpl_win.cpp
        src/win/virtualizationInstance.cpp
        src/win/dirInfo.cpp

        include/win/dirInfo.h
        include/win/fusefilesystemimpl_win.h
        include/win/virtualizationInstance.h)

    # Explicitly search for ProjectedFSLib.lib in the known SDK path
    find_library(PROJECTED_FS_LIB # Changed variable name for clarity from 'projected-fs' to 'PROJECTED_FS_LIB'
                 NAMES ProjectedFSLib
                 PATHS "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64"
                       # You can add other potential SDK paths here as fallbacks if needed:
                       # "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/um/x64"
                       # "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.19041.0/um/x64"
                 # DOC "Path to ProjectedFSLib.lib" # Optional documentation string
                )

    if(NOT PROJECTED_FS_LIB)
        message(FATAL_ERROR "ProjectedFSLib.lib not found even with explicit path. \n"
                            "The file was expected in a subdirectory of 'C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64'. \n"
                            "Please verify: \n"
                            "1. The 'Projected File System' component is installed via Visual Studio Installer (Individual Components).\n"
                            "2. The path provided in CMakeLists.txt for find_library (PROJECTED_FS_LIB) is correct for your system.\n"
                            "Current value of PROJECTED_FS_LIB was: ${PROJECTED_FS_LIB}")
    else()
        message(STATUS "Found ProjectedFSLib: ${PROJECTED_FS_LIB}")
        set(platform-specific ${PROJECTED_FS_LIB})
    endif()
endif()

set(CMAKE_AUTOUIC_SEARCH_PATHS ui)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(motioncam-fs
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(motioncam-fs SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(motioncam-fs
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_include_directories(motioncam-fs PRIVATE include)

# Add motioncam decoder as static library
add_library(motioncam-decoder STATIC
    ${DEPENDENCIES_PATH}/motioncam-decoder/lib/Decoder.cpp
    ${DEPENDENCIES_PATH}/motioncam-decoder/lib/RawData.cpp
    ${DEPENDENCIES_PATH}/motioncam-decoder/lib/RawData_Legacy.cpp)

target_include_directories(motioncam-decoder
    PUBLIC
    ${DEPENDENCIES_PATH}/bw64/include
    ${DEPENDENCIES_PATH}/motioncam-decoder/thirdparty
    ${DEPENDENCIES_PATH}/motioncam-decoder/lib/include
)

# Find the packages using vcpkg
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

# Add boost
# Modern CMake with vcpkg usually doesn't require these Boost_USE_* variables
# set(Boost_USE_STATIC_LIBS        ON)
# set(Boost_USE_DEBUG_LIBS        OFF)
# set(Boost_USE_RELEASE_LIBS       ON)
# set(Boost_USE_MULTITHREADED      ON)
# set(Boost_USE_STATIC_RUNTIME    OFF)

# Ubuntu packages currently provide Boost 1.83, so require that version or
# newer. The algorithm headers are header only and don't need a component.
find_package(Boost 1.83 REQUIRED COMPONENTS filesystem locale)
# if(Boost_FOUND) # find_package with REQUIRED will FATAL_ERROR if not found
#   include_directories(${Boost_INCLUDE_DIRS}) # Usually not needed with imported targets
# endif()

target_link_libraries(motioncam-fs PRIVATE
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::Network
  # Use modern Boost imported targets
  Boost::filesystem
  Boost::locale
  spdlog::spdlog
  fmt::fmt
  motioncam-decoder
  ${platform-specific} # This will link ProjectedFSLib.lib
)

if(${QT_VERSION} VERSION_LESS 6.1.0 AND (APPLE OR MACOSX_BUNDLE_GUI_IDENTIFIER)) # Added condition for MACOSX_BUNDLE_GUI_IDENTIFIER
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.motioncam.motioncam-fs)
endif()
set_target_properties(motioncam-fs PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS motioncam-fs
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(motioncam-fs)
endif()
