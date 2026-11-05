# ============================================================================
# Voice Command Unit Tests
# ============================================================================

target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        VoiceCommandManagerTest.cc
        VoiceCommandManagerTest.h
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_qgc_test(VoiceCommandManagerTest LABELS Unit Utilities)
