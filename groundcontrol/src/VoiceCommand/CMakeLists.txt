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
        VoiceCommandWorker.cc
        VoiceCommandWorker.h
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
set(GGML_OPENMP ON CACHE BOOL "Enable ggml OpenMP dependency" FORCE)
set(GGML_AVX_VNNI ON CACHE BOOL "Enable ggml AVX-VNNI" FORCE)
set(GGML_CCACHE ${QGC_USE_CACHE} CACHE BOOL "Use QGC ccache setting for ggml" FORCE)

find_package(Vulkan COMPONENTS glslc QUIET)
find_package(SPIRV-Headers CONFIG QUIET)
if (Vulkan_FOUND AND SPIRV-Headers_FOUND)
    message(STATUS "QGC VoiceCommand: Vulkan and SPIRV-Headers found, enabling Vulkan GPU acceleration")
    set(GGML_VULKAN ON CACHE BOOL "Enable ggml Vulkan backend" FORCE)
else()
    message(STATUS "QGC VoiceCommand: Vulkan GPU backend disabled (install spirv-headers to enable Vulkan GPU)")
    set(GGML_VULKAN OFF CACHE BOOL "Enable ggml Vulkan backend" FORCE)
endif()

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/whisper.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/whisper.cpp"
    EXCLUDE_FROM_ALL
)

set(BUILD_SHARED_LIBS "${_qgc_saved_build_shared_libs}")

foreach(_whisper_target IN ITEMS whisper ggml ggml-base ggml-cpu ggml-vulkan)
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
