# Copyright © 2026 Kyle J. Cardoza.
# SPDX-FileCopyrightText: 2026 Kyle J. Cardoza
# SPDX-License-Identifier: GPL-3.0-or-later
# Portions of this file may derive from GNUstep; see THIRD-PARTY.md.

# libagora-iconservices' own suite.
#
# THE APPLICATION ICON'S FIXTURES ARE BUILT AT RUN TIME, NOT COMMITTED: the test writes an `Icons.icns`
# holding two PNG representations into a private copy of the fixture bundle, so the `.icns` reader is measured
# against a container the test itself composed. The vendored Frost theme is the one binary asset the component
# ships, and `AGORA_ICON_THEME` points the suite at the checkout's copy so it is measured without an install.

add_executable(iconservices iconservices.c)
target_compile_definitions(iconservices PRIVATE
    FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    THEME_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../themes/Frost")
target_link_libraries(iconservices PRIVATE IconServices CoreFoundation::CoreFoundation AgoraDrawing::Drawing AgoraLaunchServices::LaunchServices)

add_test(NAME iconservices COMMAND iconservices)

set(_agora_runtime_dirs
    "$<TARGET_FILE_DIR:IconServices>"
    "$<TARGET_FILE_DIR:CoreFoundation::CoreFoundation>"
    "$<TARGET_FILE_DIR:AgoraDrawing::Drawing>"
    "$<TARGET_FILE_DIR:AgoraLaunchServices::LaunchServices>")
list(REMOVE_DUPLICATES _agora_runtime_dirs)
string(REPLACE ";" ":" _agora_ld_path "${_agora_runtime_dirs}")
set_tests_properties(iconservices PROPERTIES ENVIRONMENT
    "LD_LIBRARY_PATH=${_agora_ld_path};AGORA_ICON_THEME=${CMAKE_CURRENT_SOURCE_DIR}/../themes/Frost")
