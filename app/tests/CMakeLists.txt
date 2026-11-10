# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 REQUIRED COMPONENTS Test)

function(tubeclock_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE tubeclock_core littlefs Qt6::Test)
  target_compile_definitions(${name} PRIVATE
    REPO_ROOT="${REPO_ROOT}"
    TEST_FIXTURES="${REPO_ROOT}/test/fixtures")
  add_test(NAME ${name} COMMAND ${name})
  # Offscreen platform so image tests run without a display (CI, Flatpak builder).
  set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

tubeclock_test(test_clockconfig)
tubeclock_test(test_timezones)
tubeclock_test(test_imageset)
tubeclock_test(test_littlefs)
tubeclock_test(test_esptool)
tubeclock_test(test_espimage)
tubeclock_test(test_firmwarebundle)
tubeclock_test(test_clockclient)
tubeclock_test(test_serialdiscovery)

# Opens every page of the app once and fails on QML errors in our own files.
if(TARGET tubeclock)
  add_test(NAME ui_smoke COMMAND tubeclock --smoke-test)
  set_tests_properties(ui_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1;TUBECLOCK_IMAGESETS_DIR=${REPO_ROOT}/data")
endif()
