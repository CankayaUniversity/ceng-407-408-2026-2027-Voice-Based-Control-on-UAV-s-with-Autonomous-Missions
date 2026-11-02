# ============================================================================
# Audio Utilities Unit Tests
# Tests for audio output and speech synthesis
# ============================================================================

target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        AudioCaptureTest.cc
        AudioCaptureTest.h
        AudioDeviceManagerTest.cc
        AudioDeviceManagerTest.h
        AudioLevelMeterTest.cc
        AudioLevelMeterTest.h
        AudioOutputTest.cc
        AudioOutputTest.h
        NoiseFilterTest.cc
        NoiseFilterTest.h
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_qgc_test(AudioCaptureTest LABELS Unit Utilities)
add_qgc_test(AudioDeviceManagerTest LABELS Unit Utilities)
add_qgc_test(AudioLevelMeterTest LABELS Unit Utilities)
add_qgc_test(AudioOutputTest LABELS Unit Utilities)
add_qgc_test(NoiseFilterTest LABELS Unit Utilities)
