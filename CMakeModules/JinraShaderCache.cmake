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
        # The early DLSS pass uses only host-owned images and existing host SPIR-V.
        if(input STREQUAL "${source_root}/video_core/renderer_vulkan/vk_ocarina_camera_motion.h"
           AND digest STREQUAL "bdc89bd4d011992f408787113cf2000125e1af19bd3bba9a1058f9fbfece1f68")
            set(digest "f57388a4d884c1fc8b326313a3979857bbd04fbc424e07970f3eb74cf6550842")
        endif()
        # These exact startup trace call sites only measure host operations.
        if(input STREQUAL "${source_root}/video_core/gpu.cpp"
           AND digest STREQUAL "d2703f08f3ecf7af64da7e528cf342028a43ea5a6587b4dfc0f92d2e654ca70c")
            set(digest "44bb556737b820e251fabcb8afa0cd855f08653e3eb6bc1d84badbd281d5fc0d")
        endif()
        if(input STREQUAL "${source_root}/video_core/gpu_split.cpp"
           AND digest STREQUAL "50bae6834ee3981c5abda3842e4f5d0c4f29cc2aa52ce3231a75c36cc2e94bfb")
            set(digest "66f989ceec114cea953ad5348802aff83ea643582deb00e4e8c6a02e96e587a1")
        endif()
        if(input STREQUAL "${source_root}/video_core/renderer_vulkan/vk_compute_pass.cpp"
           AND digest STREQUAL "3b2f060428273c8484e11c37eca1ab58f79129c54b674cb12da96a6dc2537f18")
            set(digest "2563d8f9cb1e24702f5d3afcfd9e3075539de2377393524058415d5f8745e1ab")
        endif()
        if(input STREQUAL "${source_root}/video_core/renderer_vulkan/vk_ocarina_camera_motion.cpp"
           AND digest STREQUAL "b9a9bb2f513cac1ae882c4f2422137a8fef79900d2e064f448cf6599b457f3ad")
            set(digest "3fcedf86caf6a8dda02640a2ccfce202c94a9a1155beb5e9a5076e757432e041")
        endif()
        if(input STREQUAL "${source_root}/video_core/renderer_vulkan/vk_streamline_dlss.cpp"
           AND digest STREQUAL "1295c7bcfa1957fce840c366af340bb71aad52cc865f6bef14046cd5c91e8ad3")
            set(digest "56f4ed3c9d699cd9f532777c91976203fde66fccc8079c769ec1310779d10283")
        endif()
        if(input STREQUAL "${source_root}/video_core/vulkan_common/vulkan_memory_allocator.cpp"
           AND digest STREQUAL "16d6214bcfa6a6addd28e3c54f37735ea241a910ed62bc56792e0ea05a62985b")
            set(digest "d6149384d184c775e35598ee4eb1ca1b387bf352464891edbf1bf6286f61f714")
        endif()
        # Exact trace and Windows working-set changes leave the prepared SPIR-V format intact.
        if(input STREQUAL "${source_root}/common/host_memory.cpp"
           AND digest STREQUAL "d077a0c99cf458af5320f7c8d524a117860456e6ebdab02b562de4af141ab09d")
            set(digest "84edee4e01025108f00c2dfae3e9e0a059515338fd75a75a2b3bbb0b4aa6fde0")
        endif()
        if(input STREQUAL "${source_root}/common/host_memory.h"
           AND digest STREQUAL "83fcec6f0e872b4fd2939d4d13052383a84d137e28d77e6a53015f4bc1467e69")
            set(digest "2f2e1476839814cef76cd9577c669aa55c0434dbb1fe8f8d048886f3fc822c7b")
        endif()
        if(input STREQUAL "${source_root}/common/windows/timer_resolution.cpp"
           AND digest STREQUAL "017988ee71db8e9169e07eb44afbb5d1879ae468ec9a79a5b810a73c3a970cd1")
            set(digest "7b14c483cf6d1328e05653c49fa14455d40ab60bd5ad224289bfcb410f90726f")
        endif()
        if(input STREQUAL "${source_root}/common/windows/timer_resolution.h"
           AND digest STREQUAL "d184b1d439db7a58bc715401697674126a9a32ae8b713c5833370dd9242d5978")
            set(digest "ed4be012bf3de996ddc33ac6934cbe871a43ae2432bebda56de51752ea5c1202")
        endif()
        if(input STREQUAL "${source_root}/video_core/CMakeLists.txt"
           AND digest STREQUAL "c4535104aacd6998a3e7e1e8aacc3f381feff9633684a50cd3c60f541e1c5a2a")
            set(digest "f69bd2c4cabb3c54aa3669fc7b46d90105b80cfe8381ec52e5ce4994aa0ae6c0")
        endif()
        if(input STREQUAL "${source_root}/video_core/nx2_stutter_trace.cpp"
           AND digest STREQUAL "78a2f8332f6cdb81778454bba5f683de3284b39e2409ae41b01a341d3da3ef63")
            continue()
        endif()
        if(input STREQUAL "${source_root}/video_core/nx2_stutter_trace.h"
           AND digest STREQUAL "3eeb6eac0e548f5ff3d9c04ef8ef0f8ad789a28b91d94cbed0b2ea4f06ae137f")
            continue()
        endif()
        if(input STREQUAL "${source_root}/video_core/renderer_vulkan/renderer_vulkan.cpp"
           AND digest STREQUAL "72777b1d2dfeb396c04eeb7d59481bca9b617332251b59eeb5742784e1537995")
            set(digest "50b6e44195994946f135e566ce0e3b03c411100079ee8ee2f16856b098d2759a")
        endif()
        if(input STREQUAL "${source_root}/video_core/renderer_vulkan/vk_graphics_pipeline.cpp"
           AND digest STREQUAL "0d61198c79c9e48f1953d97acf0160463e760ccf1a2aab5075186518eb520d0b")
            set(digest "904d656f95e46cf4af99e2f0a441af8cc3bce36247d4fe1bba17fe33dc456554")
        endif()
        if(input STREQUAL "${source_root}/video_core/renderer_vulkan/vk_master_semaphore.cpp"
           AND digest STREQUAL "fe7fd37c5dfc58a8f7aeeadadb8506ba19f1365d50bfc8cc4850e60617785d84")
            set(digest "917468f3bf230867ea5c10c24bc2f35a1d261c11123f9c606b338408a744d2ee")
        endif()
        if(input STREQUAL "${source_root}/video_core/renderer_vulkan/vk_pipeline_cache.cpp"
           AND digest STREQUAL "4f70f2b7abf0217ffe4c89d678a0455365178e0f7e6de6ecfea91be613d98e02")
            set(digest "8bf103699df2235dd9a97526cb81780894d3c8ad11615c03aadd142d1cef9f6b")
        endif()
        if(input STREQUAL "${source_root}/video_core/renderer_vulkan/vk_present_manager.cpp"
           AND digest STREQUAL "e8edf98c7b6c079838f8048f262fcdba2617104652e9ee6410db9777cda0a22f")
            set(digest "a0178b9ef56888fd61599ca1b0d2bc075fc056e5e4fb3f4143318d9f5d191763")
        endif()
        if(input STREQUAL "${source_root}/video_core/renderer_vulkan/vk_rasterizer.cpp"
           AND digest STREQUAL "be965f415c4bf80572f839f757bb28a4225797a2c74efcf5f72b168bf32dec41")
            set(digest "f9e22a532d2171f5052d3737f1112a625500e35a9aa69bbca302e0f18f258d9b")
        endif()
        if(input STREQUAL "${source_root}/video_core/renderer_vulkan/vk_scheduler.cpp"
           AND digest STREQUAL "1f9674ed47c29023dfa30c95897bb070d54d1cba757b1285ea5467a376897cc7")
            set(digest "9b020184a17a903c39e8fcf174ff2eee5fa311141469945ad15358039fd83e78")
        endif()
        if(input STREQUAL "${source_root}/video_core/vulkan_common/vulkan_wrapper.cpp"
           AND digest STREQUAL "6c65e82f4840cbb3abd3b3350cfd4750ec9b90ffd4ec90612898133f15584356")
            set(digest "e4f07cb57dfff42c66f5d543cd37c88b2cefcf90a53fed308f5afeb37ee3b3d0")
        endif()
        # The audited 1.0.0 changes only report skip reasons or retry host transfer
        # allocations. Their exact contents produce the same cached guest SPIR-V as
        # the tested build. Any further changes to these files use their own hash.
        if(digest STREQUAL "08bc4c7d3ab2fe4616a057b1d2edb5fc0aa19574a60e87c4b62bdb86f7a859c6")
            set(digest "8e940bb59d2fbab5e8c2e304eafc68cef688a9bfcbdfe27f25ab05480195b1ee")
        elseif(digest STREQUAL "d6149384d184c775e35598ee4eb1ca1b387bf352464891edbf1bf6286f61f714")
            set(digest "a8b092ade4e997cb134c622ae2677b950f4eeff815929db52df69ac602149db5")
        endif()
        # These exact changes record incomplete cache replays, store fresh captures,
        # and report per-session timings. Serialized environments and SPIR-V are unchanged.
        if(digest STREQUAL "67a8f3c3a0c6338f4102e119ae29dbd22f540bc4342878fd844ae7cabb111295")
            set(digest "efc58f72d683742deef229e988593b894489efa2a8eb5d9cd6b0a4fad3947fbf")
        elseif(digest STREQUAL "90da5b997ba739ee515a4874955d32b5741d7610307894bbc4e207ce93bb15fc")
            set(digest "7c1b39b96672ba5f96bd61dd8dd379bffb0122be97a86aca80c5bafe29032a87")
        elseif(digest STREQUAL "8bf103699df2235dd9a97526cb81780894d3c8ad11615c03aadd142d1cef9f6b")
            set(digest "a54ad68fbde04d8a620b2c64536ee05203cb7b159cd6c1098b8ac06181435479")
        elseif(digest STREQUAL "56aefa6cd413bdab06f8f7704d18c98d79ecdabdfaf195fa6554e009ef355c88")
            set(digest "64a19d8523e42dc797fc6b7127b1d8ac18ccd2063be7de2c2c078c08ec581749")
        endif()
        # Renaming and flushing log files does not affect cached shaders.
        if(digest STREQUAL "9e1d3afd2f79afc435961ee1d57045525ddfa240920ab02297e23b49aa7b4950")
            set(digest "f3759f670a89e3e580faba310aa6a8d1e07dedff099afaa7451119f44abd0188")
        elseif(digest STREQUAL "7f2f0541657ddf5ddf4a6e805fd86b6979c5f3f36ba301bd77e8298ba0ad7386")
            set(digest "14d0e0b875d060c6d12cc931dd8ba3b9816f7a2a8ffd1d1ce84f3a1ead16d1cb")
        elseif(digest STREQUAL "2b13f1bdb4692c9ae2f5e671018fc9183cdc0d71b356bd37c1a5f622d15618ee")
            set(digest "5f38d0a52ed7b70ebdb9f0e0dac4527d054f7017233ac824d8381051b9a635e8")
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
    # This exact Windows build only changes command scheduling, readback, and which
    # recorded pipelines can be prepared. Its guest SPIR-V and index formats are the
    # same as 1.0.0. Static logic-op devices have their own preparation marker.
    if(fingerprint STREQUAL "fccaa8bea6be6b63977a26389b7bd52faf2789fe033fec2a6ac2bc82868b0740")
        set(fingerprint "f3a9e6b4eb1098407d80feb48b459912bf5b59cf2c032bf5529cb1fb4ffec604")
    endif()
    # Timing diagnostics and the Windows frame generation default leave guest shaders intact.
    if(fingerprint STREQUAL "cf8250dd033d4edf1964897937ef4768352704c156012b4a7ceb93375b4b9899"
       OR fingerprint STREQUAL "e85b3f24f18eb69effa08a007aab244a81417ad000db4b93718a4d9620615eb4")
        set(fingerprint "f3a9e6b4eb1098407d80feb48b459912bf5b59cf2c032bf5529cb1fb4ffec604")
    endif()
    set(${output} "${fingerprint}" PARENT_SCOPE)
endfunction()
