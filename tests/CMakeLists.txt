# Copyright © 2026 Kyle J. Cardoza.
# SPDX-FileCopyrightText: 2026 Kyle J. Cardoza
# SPDX-License-Identifier: GPL-3.0-or-later

# libagora-iconservices' own suite.
#
# THE FIXTURES ARE BUILT AT RUN TIME, NOT COMMITTED: the test writes an `Icons.icns` holding two PNG
# representations into a private copy of the fixture bundle, and a small icon theme of its own — an `index.theme` and
# three flat-coloured PNGs — so the `.icns` reader and the theme lookup are both measured against containers the test
# composed. Nothing here needs an installed theme, and the component ships no art to measure.

add_executable(iconservices iconservices.c)
target_compile_definitions(iconservices PRIVATE
    FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    # The checkout's SYMBOL theme: the composed theme in the test links it as its sibling, so the rule that asks
    # `Tabler` for a name the theme in charge does not carry is measured without an install.
    THEMES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../themes")
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
    "LD_LIBRARY_PATH=${_agora_ld_path}")
