# ============================================================================
# Voice Command Unit Tests
# ============================================================================

target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        DeterministicMatcherTest.cc
        DeterministicMatcherTest.h
        SttEngineTest.cc
        SttEngineTest.h
        VoiceCommandInterfaceTest.cc
        VoiceCommandInterfaceTest.h
        VoiceCommandManagerTest.cc
        VoiceCommandManagerTest.h
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_qgc_test(SttEngineTest LABELS Unit Utilities)
add_qgc_test(DeterministicMatcherTest LABELS Unit Utilities)
add_qgc_test(VoiceCommandManagerTest LABELS Unit Utilities)
add_qgc_test(VoiceCommandInterfaceTest LABELS Unit Vehicle)
