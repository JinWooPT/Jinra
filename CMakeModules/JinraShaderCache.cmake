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

    # Approved v5.6 host-only revisions. These changes affect scheduling, diagnostics,
    # texture data, media decoding, or the host upscaler. Guest SPIR-V and serialized shader metadata
    # remain compatible. A later edit must be audited before it can reuse an alias.
    foreach(revision IN ITEMS
        "common/settings_enums.h|3cdda3cdb062fd83baca1700ba8cbe78bbde06e4e26466ccfdd2e3428c926547|a40c27b266c8b8f563930ed6ba38bc2702d3be4429d76ca3732b74ce19cd318b"
        "video_core/fence_manager.h|ff2ca179e145408d33af7d059a52e10488fef7d6d91414dd17ce2720e25262dc|73afee0af5bfc2e96671cb6393dff923761956b0cba2a8aa7465a481730ec715"
        "video_core/fsr.h|c5644cd85ec353a1d978bc3f4499f7dc2a412cda84f865d840722805cee5d47e|c33fe76543ceb29dcb82aef55faf23e445ed1bd7f4beeca5e50ac0e1d413a8f5"
        "video_core/ocarina_upscale.h|43f57b8834800c9a67a891abf2e561f456e94f0ebad6031ef05a9d92d9ddf4ac|host-only"
        "video_core/host1x/ffmpeg.cpp|8ae8a19a58abb4eede89a28f3ad7d0922bf12e46dc235f4d0389c4bcc0347807|3aef40a13fcd6472a01af9a178421a542235b27888506ff35bf238cf422ed6d8"
        "video_core/host1x/ffmpeg.h|cb896c4975b477837c3547da00b62d4375d05090aaca1b75b8a1d6d79f6f6f2d|73495ec4e721672344538869f4ca4cbad60fb6ecf088210b24de6b75a1249f7c"
        "video_core/host1x/nx2_video_decoder.cpp|0591ec18a685736f8393adcf369481286d26c4f37525869ec214a47ecf82a62d|42923af302c71f763f6e3ae8bac588cb15dd8098cae10c803c0b036481dca861"
        "video_core/host1x/nx2_video_decoder.h|9284c06c78441ad9806e20466a7b5483c0e4d1c966850461fe4fb8b3d412a0f7|097d53bd7533a723b8b599bffe23b47b6fecddd618225781f78d4fbb1ba6c8fe"
        "video_core/host_shaders/CMakeLists.txt|a41a1c7d1ada2e7bb8ed4e2a94a1d327b13b61bfd45f06b6ccbd339e449a057b|ca221cb87a101803bb79c4fa32b3e3699d5426806d6f795bf9f0a4fe4cb9d4f6"
        "video_core/nx2_stutter_trace.cpp|4640b167badc651219f356b1970b9f2133c06acbfc862eedbfc30b2f91d170ea|e2eed9c78cc6314985ee2b8efb3cbefc6e037414c4539f5e2732e4a17c3b811e"
        "video_core/nx2_stutter_trace.h|c425e2249608e52158a94df8b4b4f7e358071adb42a1b0fbe085182979749f0a|df4e864f896d1866748e94f7b7460ad248a8f78a4b88ca666e62a35b89c1f1a9"
        "video_core/query_cache/query_cache.h|f7efdfb1bac1423315782d95bbc3a55eab1cf154253af79c525dbba8620b3566|6aa2239e04c73cd958f8fc522f26a8e2ae1bf3e1cc79de5e4c1d63ab82f701ed"
        "video_core/query_cache/query_stream.h|11ab9efb8a1bb20cff7cbdeaf4b843e75a1d2674140a44c8dda2fd030e380133|643a6d2530bcc15898757664117e7e98b351a43866550c39d6c103d837f41101"
        "video_core/renderer_vulkan/vk_compute_pipeline.cpp|1e8fb8ce8acddd2a17a690f0f09efcdf3323b93544b33dd2ca5015aa3220c51e|8b81925264b808ef53ad8dd762a9d779d681b9722eefd385acc62599141d05cc"
        "video_core/renderer_vulkan/vk_compute_pipeline.h|09f88094a481975078f764a50b8ece8d022ec08b0f7196eb450eae5196d47081|ba480736fef98dfb574fae9021dbf85d756ce5487093e6505d0c7ede83ce1645"
        "video_core/renderer_vulkan/vk_fsr3_upscaler.cpp|316bc78d0e81de402449f112bab38b6d82b3aa98b46e7c7499851a1872c4906b|fedef10d36d216bf0b53796e2cfc7f1c350282a84b21b970c11403470cf46d3c"
        "video_core/renderer_vulkan/vk_fsr3_upscaler.h|ba63788b8991828655d55f33c69b13c37d7e7dfd778e6da01639b41d05b7c7dc|37e8d933df4b146d8ab6c45bd550823ac4aeb0f0f673058ba958e6faaf7438ab"
        "video_core/renderer_vulkan/vk_graphics_pipeline.cpp|18909af2b904f33781e6ef1e25b2d7780a13b25866e3c530b2ed224cb80b6e11|0d61198c79c9e48f1953d97acf0160463e760ccf1a2aab5075186518eb520d0b"
        "video_core/renderer_vulkan/vk_master_semaphore.cpp|46302418123cd6151995a188acc86ca9e35366df00ee7d3a7d1363ba9d75b137|fe7fd37c5dfc58a8f7aeeadadb8506ba19f1365d50bfc8cc4850e60617785d84"
        "video_core/renderer_vulkan/vk_master_semaphore.h|855fe478fc4ddcfaa58b03a805948865329bf3e323197bc601c933f5fe776550|67b9d6c03659569c53a10ff439070835c8093b0efd8495c1a30e0d82cce6e1d8"
        "video_core/renderer_vulkan/vk_ocarina_camera_motion.cpp|55c4230b23d3ccc595e81d90d9b19411198dd31cb42823169b42e7c0ebfd2fd5|b9a9bb2f513cac1ae882c4f2422137a8fef79900d2e064f448cf6599b457f3ad"
        "video_core/renderer_vulkan/vk_ocarina_camera_motion.h|59d9a1edd91642c08726ef4c10bd2d0a973f60a87f4022a181f8a93dad4cb679|bdc89bd4d011992f408787113cf2000125e1af19bd3bba9a1058f9fbfece1f68"
        "video_core/renderer_vulkan/vk_pipeline_cache.cpp|d39a0283350ecca03e121d6bc70015628b8c666cb4a595ebf13ed1c8a5b77b81|a6cbf6d6a2ac16b260eca1ceb6d7d8d937385f8aaeb6b24c5c4a4edbc650f935"
        "video_core/renderer_vulkan/vk_pipeline_cache.h|28ab108633ae2d327d6ec40ca1de35bcbddf49bb4da9ab98d47c047e9b31fbe6|99bd8ede10e7c7463d50dba66b84468a52a7dcfefa4e9d3c2c854cdf5a943144"
        "video_core/renderer_vulkan/vk_query_cache.cpp|a5cd049e0cd366e9d4057566a36eff9ec9311ba330fb1dbf6fe46ad0fb83e067|951dc1416cf6cc532181defb6f7690095d29525b8cef788f324a6bc251d361f5"
        "video_core/renderer_vulkan/vk_rasterizer.cpp|aa020eb3032e480193e0c778907809e0211c3f7df90c29f36f82fdec0c41c2cd|cf917668761d4d3aa2ed283445383b727ffb01d2ad77c55a491afb09445e34c1"
        "video_core/renderer_vulkan/vk_rasterizer.h|6928fdab295c3d862a693ba21c140ba2f40cbb866d8afd4ddd292a5b0d27c5e4|ea7d69c2b314012f18ede593723a9a9912ae24106fd602229800042ac42a5f97"
        "video_core/renderer_vulkan/vk_scheduler.cpp|1d4c895019166543c8b3dbb817bb0c080a077425717d3719bde8f3acb6a5e403|1f9674ed47c29023dfa30c95897bb070d54d1cba757b1285ea5467a376897cc7"
        "video_core/renderer_vulkan/vk_scheduler.h|189fc1292cb37fdf43d4648cc7bd31cd32359c8c230ce1ef647c1500907861e1|0bfa34aac7f9e2c1866805328fe2ef267590034bd2cb4a0f66a2ab2bedf6024b"
        "video_core/renderer_vulkan/vk_texture_cache.cpp|a8847e21494de8f5bbb52215ec0d2eb397c303d3c109716e85345e4488442f18|d1ab07480f59cdb93cd18533c4648d5fefccf44cc250b1415e9406441f5552d5"
        "video_core/renderer_vulkan/vk_texture_cache.h|cc404b1985f058c3729e110e7bfb3f613ee5ac367a8e216e88faf0dac8bf6e76|2cb80f9eef4c3a6d42ad76ed17c62133b620487785bdafd0fb8f835bc780aa62"
        "video_core/texture_cache/texture_cache_base.h|7f0e6b5c0bf20d5aa979e3051c686ebd57627c3eb46f74efce74e2b8a3802226|5fd80df8855357db728137684df0f90c64f03a9b0a829fdcf81b65db8ae9d236"
        "video_core/texture_cache/texture_cache.h|0bc60211a2e3c3979d4ba6d81d33f21ddb4385de74fe46fded89651768cb4ed9|d88c9b0e9cefd85d2f644b69fded3e9e22b0c05a923bf0ba2b4fd8ff309279b3"
        "video_core/texture_cache/util.cpp|93196060f065a2918eb7e3092938e4771fb6eac32ce21b323c0d3ca7ca67ab1c|e0f2e9f060a2fb1619a5571f21376dc250b33bc60cefadccee3c7325d8f763ad"
        "video_core/texture_cache/util.h|bea391760e175c2ca3e214f8368c39e0b9431924710de0fff04a7d06566b7b11|2c649b2e64e8399fa5735313e421de809b437dd3296a01e38da6a85f08878148"
        "video_core/textures/decoders.cpp|b03643f4f27041043bd806dd7accc61a467d96bb30b033075d3384d9f4153505|95967fe67c3f2a41da6b39091232cf596e7046445dd47858810bbe319274e83b"
    )
        string(REPLACE "|" ";" revision "${revision}")
        list(GET revision 0 relative_path)
        list(GET revision 1 revision_hash)
        list(GET revision 2 old_hash)
        set("host_revision_path_${revision_hash}" "${source_root}/${relative_path}")
        set("host_revision_base_${revision_hash}" "${old_hash}")
    endforeach()

    set(hashes)
    foreach(input IN LISTS inputs)
        # Match files across checkout line-ending policies.
        file(READ "${input}" contents)
        string(REPLACE "\r\n" "\n" contents "${contents}")
        string(SHA256 digest "${contents}")
        if(input STREQUAL "${host_revision_path_${digest}}")
            if(host_revision_base_${digest} STREQUAL "host-only")
                continue()
            endif()
            set(digest "${host_revision_base_${digest}}")
        endif()
        # These revisions change compute replacement timing and mesh feature selection only.
        # The preparation key separately records when the device forces the mesh fallback.
        if(input STREQUAL "${source_root}/video_core/renderer_vulkan/vk_pipeline_cache.cpp"
           AND digest STREQUAL "a6cbf6d6a2ac16b260eca1ceb6d7d8d937385f8aaeb6b24c5c4a4edbc650f935")
            set(digest "4f70f2b7abf0217ffe4c89d678a0455365178e0f7e6de6ecfea91be613d98e02")
        elseif(input STREQUAL "${source_root}/video_core/renderer_vulkan/vk_pipeline_cache.h"
           AND digest STREQUAL "99bd8ede10e7c7463d50dba66b84468a52a7dcfefa4e9d3c2c854cdf5a943144")
            set(digest "56aefa6cd413bdab06f8f7704d18c98d79ecdabdfaf195fa6554e009ef355c88")
        elseif(input STREQUAL "${source_root}/video_core/vulkan_common/vulkan_device.cpp"
           AND digest STREQUAL "99ea69fb8fcc32036ec9f2a6a13789f0bbd04389e0a6848d92d220eacadb2f09")
            set(digest "ce094864f4deb450e4c2d55b9881ee681ab431a410d2d87bf4db7880daf7a4eb")
        elseif(input STREQUAL "${source_root}/video_core/vulkan_common/vulkan_device.h"
           AND digest STREQUAL "14e8ebcb63f70872017e76f54ca7e78f5d6fb042f7d09c6bd78f48f897d7c451")
            set(digest "261fe5356579b6127ee1ed6542ffc72f884d2d4003fe7c6105623975b098e67a")
        endif()
        # High-refresh settings and presentation pacing do not change translated shaders.
        if(input STREQUAL "${source_root}/common/settings.cpp"
           AND digest STREQUAL "54a53edb3623edea25bd9a311c80dbc56e55dbaabbdfd9f1c484efcccda8baa2")
            set(digest "80fce58b9ccce318a3d315e6dcbdcd64937ea4a594836391b2e7948e93142430")
        elseif(input STREQUAL "${source_root}/common/settings.h"
           AND digest STREQUAL "95df9689e315805ceb2d76b2ea51b170b4cca57b18dcc2b9ae6df3f981ad057d")
            set(digest "d61a557e4ddf760bf3202e28f93f2b91eff34e38a3fb5629e7f69a30a430a4c7")
        elseif(input STREQUAL "${source_root}/common/settings_enums.h"
           AND digest STREQUAL "a40c27b266c8b8f563930ed6ba38bc2702d3be4429d76ca3732b74ce19cd318b")
            set(digest "2e1eee18484484b31a74fa445ed451ae626fc91ba6404ab37dc6fcd8b59b8fa9")
        elseif(input STREQUAL "${source_root}/video_core/renderer_vulkan/vk_scheduler.h"
           AND digest STREQUAL "0bfa34aac7f9e2c1866805328fe2ef267590034bd2cb4a0f66a2ab2bedf6024b")
            set(digest "641888fc57e8c6522a1bdaac06ee103901d9b669a451d62e8634597e233cabfd")
        elseif(input STREQUAL "${source_root}/video_core/renderer_vulkan/vk_swapchain.cpp"
           AND digest STREQUAL "64ffeb4d6c7d6990cc12ade6c23c018548b79ee075fbc05e333ebb66ff3b6819")
            set(digest "fdbcfbffcfd9d91ffeb18140426dbc38dbd71e642ecf59d87f32a5feb188ed21")
        endif()
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
