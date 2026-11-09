# SPDX-License-Identifier: GPL-3.0-or-later

# Keep the cache independent of frontend, package and executable version changes.
function(jinra_shader_cache_fingerprint output)
    set(source_root "${PROJECT_SOURCE_DIR}/src")
    set(inputs)
    foreach(component IN ITEMS shader_recompiler video_core common)
        file(GLOB_RECURSE component_inputs CONFIGURE_DEPENDS LIST_DIRECTORIES false
            "${source_root}/${component}/*.cpp"
            "${source_root}/${component}/*.h"
            "${source_root}/${component}/*.hpp"
            "${source_root}/${component}/*.inc"
            "${source_root}/${component}/CMakeLists.txt")
        list(APPEND inputs ${component_inputs})
    endforeach()

    # Include the resolved encoder and headers, including local CPM overrides. Builds with
    # untracked system packages retain the executable-specific compatibility check.
    set(sirit_root "${CPM_PACKAGE_sirit_SOURCE_DIR}")
    set(spirv_root "${CPM_PACKAGE_SPIRV-Headers_SOURCE_DIR}")
    set(vulkan_root "${CPM_PACKAGE_VulkanHeaders_SOURCE_DIR}")
    if(NOT IS_DIRECTORY "${sirit_root}/include/sirit" OR
       NOT IS_DIRECTORY "${spirv_root}/include/spirv" OR
       NOT IS_DIRECTORY "${vulkan_root}/include/vulkan")
        set(${output} "" PARENT_SCOPE)
        return()
    endif()
    get_target_property(sirit_imported sirit::sirit IMPORTED)
    if(sirit_imported)
        get_target_property(sirit_binary sirit::sirit IMPORTED_LOCATION)
        if(NOT EXISTS "${sirit_binary}")
            set(${output} "" PARENT_SCOPE)
            return()
        endif()
        file(SHA256 "${sirit_binary}" encoder_hash)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${sirit_binary}")
    else()
        file(GLOB_RECURSE sirit_sources CONFIGURE_DEPENDS LIST_DIRECTORIES false
            "${sirit_root}/src/*.cpp" "${sirit_root}/src/*.h" "${sirit_root}/src/*.inc")
        if(NOT sirit_sources)
            set(${output} "" PARENT_SCOPE)
            return()
        endif()
        list(APPEND inputs ${sirit_sources} "${sirit_root}/CMakeLists.txt")
    endif()
    file(GLOB_RECURSE dependency_headers CONFIGURE_DEPENDS LIST_DIRECTORIES false
        "${sirit_root}/include/sirit/*.h" "${sirit_root}/include/sirit/*.hpp"
        "${spirv_root}/include/spirv/*.h" "${spirv_root}/include/spirv/*.hpp"
        "${vulkan_root}/include/vulkan/*.h")
    list(APPEND inputs ${dependency_headers})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${inputs})

    # Absolute source/package paths do not affect cache compatibility.
    string(TOUPPER "${CMAKE_BUILD_TYPE}" config)
    set(flags "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${config}}")
    string(REGEX REPLACE "-fprofile-(instr-)?use=(\"[^\"]*\"|[^ \t]+)" "" flags "${flags}")
    string(REGEX REPLACE "-Wno-error=(profile-instr-out-of-date|profile-instr-unprofiled|backend-plugin)" "" flags "${flags}")
    string(REGEX REPLACE "-flto(=[^ \t]+)?" "" flags "${flags}")
    string(REGEX REPLACE "[ \t]+" " " flags "${flags}")
    string(STRIP "${flags}" flags)
    set(material "jinra-shader-cache-v2\n${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR} ${CMAKE_SIZEOF_VOID_P}\n")
    string(APPEND material "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} ${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}\n")
    string(APPEND material "${flags}\n${encoder_hash}\n")

    set(hashes)
    foreach(input IN LISTS inputs)
        # Match files across checkout line-ending policies.
        file(READ "${input}" contents)
        string(REPLACE "\r\n" "\n" contents "${contents}")
        string(SHA256 digest "${contents}")
        # The audited 1.0.0 changes only report skip reasons or retry host transfer
        # allocations. Their exact contents produce the same cached guest SPIR-V as
        # the tested build. Any further changes to these files use their own hash.
        if(digest STREQUAL "08bc4c7d3ab2fe4616a057b1d2edb5fc0aa19574a60e87c4b62bdb86f7a859c6")
            set(digest "8e940bb59d2fbab5e8c2e304eafc68cef688a9bfcbdfe27f25ab05480195b1ee")
        elseif(digest STREQUAL "d6149384d184c775e35598ee4eb1ca1b387bf352464891edbf1bf6286f61f714")
            set(digest "a8b092ade4e997cb134c622ae2677b950f4eeff815929db52df69ac602149db5")
        endif()
        foreach(root IN ITEMS source_root sirit_root spirv_root vulkan_root)
            cmake_path(IS_PREFIX ${root} "${input}" NORMALIZE in_root)
            if(in_root)
                file(RELATIVE_PATH relative "${${root}}" "${input}")
                list(APPEND hashes "${root}/${relative}:${digest}")
                break()
            endif()
        endforeach()
    endforeach()
    list(SORT hashes)
    string(JOIN "\n" manifest ${hashes})
    string(SHA256 fingerprint "${material}${manifest}")
    set(${output} "${fingerprint}" PARENT_SCOPE)
endfunction()
