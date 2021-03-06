
include ("${CMAKE_CURRENT_LIST_DIR}/../Core/CMakeLists.txt")

add_executable (FBX-glTF-conv "${CMAKE_CURRENT_LIST_DIR}/Cli.cpp" "${CMAKE_CURRENT_LIST_DIR}/ReadCliArgs.cpp")

set_target_properties (FBX-glTF-conv PROPERTIES CXX_STANDARD 20)

target_include_directories (FBX-glTF-conv PRIVATE ${BeeCoreIncludeDirectories})

target_include_directories (FBX-glTF-conv PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../Polyfills/std.filesystem")
if (POLYFILLS_STD_FILESYSTEM)
    message (STATUS "We're using polyfill <filesystem>")
    target_compile_definitions (FBX-glTF-conv PRIVATE BEE_POLYFILLS_STD_FILESYSTEM)
endif ()

target_link_libraries (FBX-glTF-conv PRIVATE BeeCore)

find_package(clipp CONFIG REQUIRED)
target_link_libraries(FBX-glTF-conv PRIVATE clipp::clipp)

if (APPLE)
    # Set @rpath so executable can find .dylib
    # See https://gitlab.kitware.com/cmake/community/-/wikis/doc/cmake/RPATH-handling#recommendations
    # and also https://stackoverflow.com/a/61020959
    # May be we should use "$ORIGIN"(escape the dollar sign?) on other Unix-like OS?
    set_target_properties (FBX-glTF-conv PROPERTIES INSTALL_RPATH "@executable_path")
endif ()

add_custom_command(TARGET FBX-glTF-conv POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FbxSdkDynLibraries}
        $<TARGET_FILE_DIR:FBX-glTF-conv>)

install (TARGETS FBX-glTF-conv DESTINATION "bin")
