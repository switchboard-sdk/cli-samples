#include "OpenAIExtension.hpp"
#include "SileroVADExtension.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <switchboard/Switchboard.hpp>
#include <switchboard_core/Logger.hpp>

using namespace switchboard;

// Your Switchboard app's ID and secret. Register an app at https://console.switchboard.audio/register.
static const std::string APP_ID = "";
static const std::string APP_SECRET = "";

static const std::string LIVE_NODE = "liveNode";
static const std::string MUSIC_PLAYER_NODE = "musicPlayerNode";
static const std::string MUSIC_URL =
    "https://switchboard-sdk-public.s3.dualstack.us-east-1.amazonaws.com/assets/audio/music/elevator-music-stereo.mp3";

static std::optional<std::string> readContentsOfTextFile(const std::string& filePath) {
    std::filesystem::path fileSystemPath(filePath);
    if (!std::filesystem::exists(filePath)) {
        return std::nullopt;
    }
    std::ifstream fileStream(filePath);
    std::string fileContent((std::istreambuf_iterator(fileStream)), std::istreambuf_iterator<char>());
    return fileContent;
}

// Events arrive on SDK threads, so printing is serialized.
static std::mutex outputMutex;
static std::string currentSpeaker;

static void printLine(const std::string& line) {
    std::lock_guard<std::mutex> lock(outputMutex);
    if (!currentSpeaker.empty()) {
        std::cout << std::endl;
        currentSpeaker.clear();
    }
    std::cout << line << std::endl;
}

static void printTranscript(const std::string& speaker, const std::string& delta) {
    std::lock_guard<std::mutex> lock(outputMutex);
    if (speaker != currentSpeaker) {
        if (!currentSpeaker.empty()) {
            std::cout << std::endl;
        }
        std::cout << speaker << ": ";
        currentSpeaker = speaker;
    }
    std::cout << delta << std::flush;
}

struct ToolOutput {
    bool ok;
    std::string json;
};

static ToolOutput toolError(const std::string& message) {
    return { false, SBAnyMap::mapToJson(SBAnyMap({ { "error", message } })) };
}

static ToolOutput musicResult(const Result<SBAny>& result, bool playing) {
    if (result.isError()) {
        return toolError(result.error().message);
    }
    return { true, SBAnyMap::mapToJson(SBAnyMap({ { "playing", playing } })) };
}

// Resumes the track if it's open (paused), otherwise loads it, which starts it since the player has autoPlay.
static ToolOutput playMusic() {
    const Result<SBAny> state = Switchboard::getValue(MUSIC_PLAYER_NODE, "state");
    const std::string current = state.isError() ? "" : SBAny::convert<std::string>(state.value());
    if (current == "connecting" || current == "buffering" || current == "ready") {
        return musicResult(Switchboard::callAction(MUSIC_PLAYER_NODE, "play"), true);
    }
    return musicResult(Switchboard::callAction(MUSIC_PLAYER_NODE, "load", { { "url", MUSIC_URL } }), true);
}

static ToolOutput pauseMusic() {
    return musicResult(Switchboard::callAction(MUSIC_PLAYER_NODE, "pause"), false);
}

static ToolOutput handleToolCall(const std::string& name) {
    if (name == "play_music") {
        return playMusic();
    }
    if (name == "pause_music") {
        return pauseMusic();
    }
    return toolError("Unknown tool: " + name);
}

int main(int argc, const char* argv[]) {
    if (APP_ID.empty() || APP_SECRET.empty()) {
        std::cerr << "Set your Switchboard app's ID and secret in main.cpp. "
                     "Register an app at https://console.switchboard.audio/register."
                  << std::endl;
        return 1;
    }

    // Load JSON
    std::string engineJSONFilePath = "OpenAILiveExample.json";
    auto engineJSON = readContentsOfTextFile(engineJSONFilePath);
    if (!engineJSON.has_value()) {
        std::cerr << "Failed to read engine JSON file: " << engineJSONFilePath << std::endl;
        return 1;
    }

    // Keep the SDK's log out of the transcript.
    Logger::setLogLevel(LogLevel::Warning);

    // Init Switchboard SDK and extensions. Silero VAD drives idle mode.
    extensions::openai::OpenAIExtension::load();
    extensions::silerovad::SileroVADExtension::load();
    SBAnyMap sdkConfig({ { "appID", APP_ID },
                         { "appSecret", APP_SECRET },
                         { "extensions", SBAnyMap({ { "OpenAI", SBAnyMap() }, { "Silero", SBAnyMap() } }) } });
    auto initializeResult = Switchboard::initialize(sdkConfig);
    if (initializeResult.isError()) {
        std::cerr << "Failed to initialize the SDK: " << initializeResult.error().message << std::endl;
        return 1;
    }

    // Create audio engine
    Result<Switchboard::ObjectID> result = Switchboard::createEngine(engineJSON.value());
    if (result.isError()) {
        std::cerr << "Failed to create engine: " << result.error().message << std::endl;
        return 1;
    }
    const std::string engineID = result.value();

    // Transcript
    Switchboard::addEventListener(LIVE_NODE, "inputTranscriptDelta", [](const Event& event) {
        const auto data = SBAny::convert<SBAnyMap>(event.data);
        printTranscript("You", SBAny::convert<std::string>(data.at("delta")));
    });
    Switchboard::addEventListener(LIVE_NODE, "outputTranscriptDelta", [](const Event& event) {
        const auto data = SBAny::convert<SBAnyMap>(event.data);
        printTranscript("Assistant", SBAny::convert<std::string>(data.at("delta")));
    });

    // Tool calls
    Switchboard::addEventListener(LIVE_NODE, "toolCall", [](const Event& event) {
        const auto data = SBAny::convert<SBAnyMap>(event.data);
        const auto callId = SBAny::convert<std::string>(data.at("callId"));
        const auto name = SBAny::convert<std::string>(data.at("name"));
        const auto argumentsJson = SBAny::convert<std::string>(data.at("argumentsJson"));

        const ToolOutput output = handleToolCall(name);
        printLine("[" + name + " " + argumentsJson + (output.ok ? " -> " : " failed: ") + output.json + "]");

        const auto answer = output.ok
            ? Switchboard::callAction(LIVE_NODE, "submitToolResult", { { "callId", callId }, { "outputJson", output.json } })
            : Switchboard::callAction(LIVE_NODE, "submitToolError", { { "callId", callId }, { "errorJson", output.json } });
        if (answer.isError()) {
            printLine("[Failed to answer " + name + ": " + answer.error().message + "]");
        }
    });

    // Session state
    Switchboard::addEventListener(LIVE_NODE, "sessionStarted", [](const Event&) {
        static bool first = true;
        printLine(first ? "[Connected. Start talking.]" : "[Connected.]");
        first = false;
    });
    Switchboard::addEventListener(LIVE_NODE, "sessionPaused", [](const Event&) {
        printLine("[Standby: paused after 15 s of quiet. Speak to resume.]");
    });
    Switchboard::addEventListener(LIVE_NODE, "sessionResumed", [](const Event&) {
        printLine("[Standby: resumed.]");
    });
    Switchboard::addEventListener(LIVE_NODE, "sessionIdle", [](const Event&) {
        printLine("[Idle: the session closed after 30 s of quiet. Speak to continue the conversation.]");
    });
    Switchboard::addEventListener(LIVE_NODE, "sessionWoken", [](const Event&) {
        printLine("[Idle: reopened.]");
    });
    Switchboard::addEventListener(LIVE_NODE, "sessionDisconnected", [](const Event&) {
        printLine("[Connection lost. Reconnecting.]");
    });
    Switchboard::addEventListener(LIVE_NODE, "error", [](const Event& event) {
        const auto data = SBAny::convert<SBAnyMap>(event.data);
        printLine(
            "[Error: " + SBAny::convert<std::string>(data.at("type")) + " "
            + SBAny::convert<std::string>(data.at("code")) + ": " + SBAny::convert<std::string>(data.at("message"))
            + "]"
        );
    });

    // Start audio engine
    auto startEngineResult = Switchboard::callAction(engineID, "start", {});
    if (startEngineResult.isError()) {
        std::cerr << "Failed to start engine: " << startEngineResult.error().message << std::endl;
        return 1;
    }

    // Wait for user to press a key
    printLine("Connecting. Use headphones, so the assistant doesn't hear itself. Press Enter to stop.");
    std::cin.get();

    // Stop and tear down audio engine
    Switchboard::callAction(engineID, "stop", {});
    Switchboard::destroyEngine(engineID);
    Switchboard::deinitialize();
    return 0;
}
