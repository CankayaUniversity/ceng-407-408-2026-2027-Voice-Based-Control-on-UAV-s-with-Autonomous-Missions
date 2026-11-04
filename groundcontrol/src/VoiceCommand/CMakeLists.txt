# ============================================================================
# Voice Command Module
# Coordinator for audio ingestion, PTT lifecycle, STT, and command execution
# ============================================================================

target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        VoiceCommandManager.cc
        VoiceCommandManager.h
)

target_include_directories(${CMAKE_PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
