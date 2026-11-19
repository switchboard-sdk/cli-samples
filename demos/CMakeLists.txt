add_subdirectory(sine)
add_subdirectory(silerovad)
add_subdirectory(tts)
if(NOT WIN32)
    add_subdirectory(stt)
    add_subdirectory(stt_to_tts)
    # No OpenAI webrtc transport on Windows yet.
    add_subdirectory(openai_live)
endif()