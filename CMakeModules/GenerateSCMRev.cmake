# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2019 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# generate git/build information
include(GetSCMRev)

function(get_timestamp _var)
    string(TIMESTAMP timestamp UTC)
    set(${_var} "${timestamp}" PARENT_SCOPE)
endfunction()

get_timestamp(BUILD_DATE)

get_filename_component(jinra_version_file "${CMAKE_CURRENT_LIST_DIR}/../VERSION" ABSOLUTE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${jinra_version_file}")
file(STRINGS "${jinra_version_file}" BUILD_VERSION LIMIT_COUNT 1)

if (JINRA_BUILD_TAG MATCHES "^build-[0-9]+$")
    set(IS_DEV_BUILD false)
elseif (DEFINED GIT_RELEASE)
    set(GIT_REFSPEC "${GIT_RELEASE}")
    set(IS_DEV_BUILD false)
else()
    set(IS_DEV_BUILD true)
endif()

if (NIGHTLY_BUILD)
    set(IS_NIGHTLY_BUILD true)
else()
    set(IS_NIGHTLY_BUILD false)
endif()

set(GIT_DESC ${BUILD_VERSION})

# Generate cpp with Git revision from template

set(BUILD_AUTO_UPDATE_STABLE_REPO "")
set(BUILD_AUTO_UPDATE_STABLE_API "")
set(BUILD_AUTO_UPDATE_API_PATH "")
set(BUILD_AUTO_UPDATE_WEBSITE "")
set(BUILD_AUTO_UPDATE_REPO "")
if (NIGHTLY_BUILD)
    set(REPO_NAME "Jinra Nightly")
else()
    set(REPO_NAME "Jinra")
endif()

set(BUILD_ID ${GIT_REFSPEC})
set(BUILD_FULLNAME "${REPO_NAME} ${BUILD_VERSION} ")
set(CXX_COMPILER "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

configure_file(scm_rev.cpp.in scm_rev.cpp @ONLY)
