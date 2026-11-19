set(TARGET_NAME "OpenAILive")

switchboard_add_console_app(${TARGET_NAME} EXTENSIONS
        SwitchboardOnnx
        SwitchboardSileroVAD
        SwitchboardOpenAI
)

target_sources(${TARGET_NAME} PRIVATE
        main.cpp
)

install(
        FILES OpenAILiveExample.json
        DESTINATION ${CMAKE_INSTALL_BINDIR}
)
