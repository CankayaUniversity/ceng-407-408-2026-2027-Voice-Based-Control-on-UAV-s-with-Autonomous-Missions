# ============================================================================
# Voice Command Module
# Coordinator for audio ingestion, PTT lifecycle, STT, and command execution
# ============================================================================

target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        SpeechToTextInterface.h
        SttEngineFactory.cc
        SttEngineFactory.h
        SttModelManager.cc
        SttModelManager.h
        SttModelResolver.cc
        SttModelResolver.h
        VoiceCommandManager.cc
        VoiceCommandManager.h
        WhisperEngine.cc
        WhisperEngine.h
)

target_include_directories(${CMAKE_PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/models/stt")
target_compile_definitions(${CMAKE_PROJECT_NAME}
    PRIVATE
        QGC_STT_SOURCE_MODEL_DIR="${CMAKE_SOURCE_DIR}/models/stt"
)

message(STATUS "QGC: Building whisper.cpp STT engine")

set(_qgc_saved_build_shared_libs "${BUILD_SHARED_LIBS}")
set(BUILD_SHARED_LIBS OFF)

set(WHISPER_BUILD_TESTS OFF CACHE BOOL "Disable whisper.cpp tests" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "Disable whisper.cpp examples" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "Disable whisper.cpp server" FORCE)
set(WHISPER_ALL_WARNINGS OFF CACHE BOOL "Disable whisper.cpp warnings" FORCE)
set(WHISPER_FATAL_WARNINGS OFF CACHE BOOL "Disable whisper.cpp -Werror" FORCE)
set(WHISPER_CURL OFF CACHE BOOL "Disable whisper.cpp libcurl" FORCE)
set(WHISPER_SDL2 OFF CACHE BOOL "Disable whisper.cpp SDL2" FORCE)
set(GGML_ALL_WARNINGS OFF CACHE BOOL "Disable ggml warnings" FORCE)
set(GGML_FATAL_WARNINGS OFF CACHE BOOL "Disable ggml -Werror" FORCE)
set(GGML_OPENMP OFF CACHE BOOL "Disable ggml OpenMP dependency" FORCE)
set(GGML_CCACHE ${QGC_USE_CACHE} CACHE BOOL "Use QGC ccache setting for ggml" FORCE)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/whisper.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/whisper.cpp"
    EXCLUDE_FROM_ALL
)

set(BUILD_SHARED_LIBS "${_qgc_saved_build_shared_libs}")

foreach(_whisper_target IN ITEMS whisper ggml ggml-base ggml-cpu)
    if(TARGET ${_whisper_target})
        qgc_disable_dependency_warnings(${_whisper_target})
    endif()
endforeach()

target_include_directories(${CMAKE_PROJECT_NAME}
    SYSTEM PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/whisper.cpp/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/whisper.cpp/ggml/include"
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    PRIVATE
        whisper
)
