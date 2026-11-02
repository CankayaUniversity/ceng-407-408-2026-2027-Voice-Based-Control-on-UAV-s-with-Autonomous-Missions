# ============================================================================
# Audio Utilities Unit Tests
# Tests for audio output and speech synthesis
# ============================================================================

target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        AudioDeviceManagerTest.cc
        AudioDeviceManagerTest.h
        AudioOutputTest.cc
        AudioOutputTest.h
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_qgc_test(AudioDeviceManagerTest LABELS Unit Utilities)
add_qgc_test(AudioOutputTest LABELS Unit Utilities)
