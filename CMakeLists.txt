cmake_minimum_required(VERSION 3.21)

# One place to rename the app. Override with -DAPP_NAME=...
set(APP_NAME "kinoz" CACHE STRING "Display name of the app")
set(APP_EXE  "kinoz" CACHE STRING "Executable base name")

project(${APP_EXE} VERSION 0.2.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(APPLE)
  # The legacy AGL OpenGL framework no longer exists in current macOS SDKs; stop Qt's OpenGL helper from linking it.
  set(__opengl_agl_fw_path "" CACHE INTERNAL "legacy AGL framework is gone")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets Network Concurrent)
if(UNIX AND NOT APPLE)
  find_package(Qt6 OPTIONAL_COMPONENTS DBus)      # media keys and the desktop now-playing card (MPRIS); the player works without it
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp" "${CMAKE_SOURCE_DIR}/src/*.h")
if(APPLE)
  enable_language(OBJCXX)
  file(GLOB MAC_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*/*.mm")      # Now Playing / media keys
  list(APPEND SOURCES ${MAC_SOURCES})
  set_source_files_properties(${MAC_SOURCES} PROPERTIES COMPILE_FLAGS "-fobjc-arc")
endif()

if(WIN32)
  # libmpv comes from the project-local toolchain (scripts/setup-toolchain.ps1)
  set(MPV_DIR "${CMAKE_SOURCE_DIR}/.toolchain/mpv")
  if(NOT EXISTS "${MPV_DIR}/mpv.lib")
    message(FATAL_ERROR "libmpv not found in ${MPV_DIR}. Run scripts/setup-toolchain.ps1")
  endif()
  add_executable(${APP_EXE} WIN32 ${SOURCES} resources/app.rc)
  target_include_directories(${APP_EXE} PRIVATE "${MPV_DIR}/include")
  target_link_libraries(${APP_EXE} PRIVATE "${MPV_DIR}/mpv.lib" dwmapi user32 shell32 ole32 shlwapi dbghelp advapi32 psapi windowsapp)
  target_compile_definitions(${APP_EXE} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
  # macOS: brew install mpv      Linux: apt install libmpv-dev      (both ship mpv.pc)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
  if(APPLE)
    # app icon (.icns) built from the PNG with macOS' own tools, and a plist that declares the file types kinoz opens
    set(KINOZ_ICNS "${CMAKE_BINARY_DIR}/kinoz.icns")
    execute_process(COMMAND sh "${CMAKE_SOURCE_DIR}/packaging/macos/make-icns.sh" "${CMAKE_SOURCE_DIR}/packaging/linux/kinoz.png" "${KINOZ_ICNS}" RESULT_VARIABLE ICNS_RESULT)
    add_executable(${APP_EXE} MACOSX_BUNDLE ${SOURCES})
    set_target_properties(${APP_EXE} PROPERTIES
      MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in"
      MACOSX_BUNDLE_BUNDLE_NAME "${APP_NAME}"
      MACOSX_BUNDLE_GUI_IDENTIFIER "com.p4inz.kinoz"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
      MACOSX_BUNDLE_COPYRIGHT "Copyright (C) 2026 Atharva Patil. GNU GPL v3.")
    if(ICNS_RESULT EQUAL 0 AND EXISTS "${KINOZ_ICNS}")
      target_sources(${APP_EXE} PRIVATE "${KINOZ_ICNS}")
      set_source_files_properties("${KINOZ_ICNS}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    else()
      message(WARNING "kinoz.icns could not be built; the app will use the default icon")
    endif()
  else()
    add_executable(${APP_EXE} ${SOURCES})
  endif()
  target_link_libraries(${APP_EXE} PRIVATE PkgConfig::MPV)
  if(APPLE)
    target_link_libraries(${APP_EXE} PRIVATE "-framework Foundation" "-framework MediaPlayer")
  endif()
  if(UNIX AND NOT APPLE AND TARGET Qt6::DBus)
    target_link_libraries(${APP_EXE} PRIVATE Qt6::DBus)
    target_compile_definitions(${APP_EXE} PRIVATE KINOZ_HAS_DBUS)
  endif()
endif()

if(APPLE)
  # Apple removed the legacy "AGL" OpenGL framework from current SDKs; some library metadata still asks for it.
  function(kinoz_strip_agl tgt prop)
    if(NOT TARGET ${tgt})
      return()
    endif()
    get_target_property(libs ${tgt} ${prop})
    if(NOT libs)
      return()
    endif()
    set(out "")
    foreach(l IN LISTS libs)
      if(l MATCHES "AGL")
        list(LENGTH out n)
        if(n GREATER 0)
          list(GET out -1 last)
          if(last STREQUAL "-framework")
            list(REMOVE_AT out -1)
          endif()
        endif()
      else()
        list(APPEND out "${l}")
      endif()
    endforeach()
    set_property(TARGET ${tgt} PROPERTY ${prop} "${out}")
  endfunction()
  foreach(t PkgConfig::MPV WrapOpenGL::WrapOpenGL Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Gui Qt6::Widgets Qt6::Core)
    kinoz_strip_agl(${t} INTERFACE_LINK_LIBRARIES)
    kinoz_strip_agl(${t} INTERFACE_LINK_OPTIONS)
  endforeach()
endif()

target_include_directories(${APP_EXE} PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(${APP_EXE} PRIVATE Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Network Qt6::Concurrent)
target_compile_definitions(${APP_EXE} PRIVATE APP_NAME="${APP_NAME}" APP_VERSION="${PROJECT_VERSION}")

if(MSVC)
  target_compile_options(${APP_EXE} PRIVATE /utf-8 /permissive- /Zc:__cplusplus /W3 /MP /GS /guard:cf)
  # Exploit mitigations: Control Flow Guard, ASLR (high entropy), DEP, CET shadow stacks where the CPU has them.
  target_link_options(${APP_EXE} PRIVATE /GUARD:CF /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /CETCOMPAT)
endif()

# Everything lands in <project>/dist/app
set_target_properties(${APP_EXE} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist/app"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/dist/app"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/dist/app")

# Fonts, keymaps and legal files next to the executable (all platforms)
add_custom_command(TARGET ${APP_EXE} POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/resources/fonts" "$<TARGET_FILE_DIR:${APP_EXE}>/fonts"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/resources/keymaps" "$<TARGET_FILE_DIR:${APP_EXE}>/keymaps"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_SOURCE_DIR}/LICENSE" "$<TARGET_FILE_DIR:${APP_EXE}>/LICENSE"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_SOURCE_DIR}/ADDITIONAL_TERMS.md" "$<TARGET_FILE_DIR:${APP_EXE}>/ADDITIONAL_TERMS.md"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md" "$<TARGET_FILE_DIR:${APP_EXE}>/THIRD_PARTY_NOTICES.md"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_SOURCE_DIR}/PRIVACY.md" "$<TARGET_FILE_DIR:${APP_EXE}>/PRIVACY.md")

if(WIN32)
  get_filename_component(QT_BIN "${Qt6_DIR}/../../../bin" ABSOLUTE)
  add_custom_command(TARGET ${APP_EXE} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${MPV_DIR}/libmpv-2.dll" "$<TARGET_FILE_DIR:${APP_EXE}>"
    COMMAND "${QT_BIN}/windeployqt.exe" --no-translations --no-system-d3d-compiler --no-opengl-sw
            --no-quick-import --skip-plugin-types qmltooling,generic,networkinformation
            "$<TARGET_FILE:${APP_EXE}>"
    COMMENT "Deploying Qt + libmpv next to the exe")
endif()
