#include "OpenAIExtension.hpp"
#include "SileroVADExtension.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <switchboard/Switchboard.hpp>
#include <switchboard_core/Logger.hpp>
#include <vector>

using namespace switchboard;

// Shared default app, tightly limited. Set SWITCHBOARD_APP_ID and SWITCHBOARD_APP_SECRET to use your own.
static const std::string DEFAULT_APP_ID = "6abbc7fa1fed5b257c3f0057";
static const std::string DEFAULT_APP_SECRET = "de3bd20ede43a3a708ef5f75b50de14dd0d64375";

static const std::string LIVE_NODE = "liveNode";
static const std::string VOICE_GAIN_NODE = "voiceGainNode";

// Gain at 100% volume; 50%, the default, leaves the voice unchanged.
static constexpr float MAX_VOICE_GAIN = 2.0f;

// GPT-Live voices, for --voice.
static const std::vector<std::string> VOICES = { "alloy", "ash",   "ballad", "cedar",   "coral",
                                                 "echo",  "marin", "sage",   "shimmer", "verse" };

static std::optional<std::string> readContentsOfTextFile(const std::string& filePath) {
    std::filesystem::path fileSystemPath(filePath);
    if (!std::filesystem::exists(filePath)) {
        return std::nullopt;
    }
    std::ifstream fileStream(filePath);
    std::string fileContent((std::istreambuf_iterator(fileStream)), std::istreambuf_iterator<char>());
    return fileContent;
}

static std::string environmentOr(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0' ? value : fallback;
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

// Answered here rather than with autoResult, so the model gets the volume it ended up at, or the failure.
static ToolOutput setVoiceVolume(const std::string& argumentsJson) {
    SBAnyMap arguments;
    try {
        arguments = SBAnyMap::jsonToMap(argumentsJson);
    } catch (const std::exception&) {
        return toolError("The arguments aren't a JSON object.");
    }

    const Result<SBAny> gain = Switchboard::getValue(VOICE_GAIN_NODE, "gain");
    if (gain.isError()) {
        return toolError("Couldn't read the voice volume: " + gain.error().message);
    }
    const float currentPercent = SBAny::convert<float>(gain.value()) / MAX_VOICE_GAIN * 100.0f;

    // The model may fill in both, with 0 for the one it doesn't mean.
    const float change = arguments.hasKey("change") ? SBAny::convert<float>(arguments.at("change")) : 0.0f;
    float percent = currentPercent;
    if (change != 0.0f) {
        percent = currentPercent + change;
    } else if (arguments.hasKey("volume")) {
        percent = SBAny::convert<float>(arguments.at("volume"));
    }
    percent = std::clamp(std::round(percent), 0.0f, 100.0f);

    const Result<void> result = Switchboard::setValue(VOICE_GAIN_NODE, "gain", percent / 100.0f * MAX_VOICE_GAIN);
    if (result.isError()) {
        return toolError("Couldn't set the voice volume: " + result.error().message);
    }
    return { true, SBAnyMap::mapToJson(SBAnyMap({ { "volume", static_cast<int>(percent) } })) };
}

static ToolOutput handleToolCall(const std::string& name, const std::string& argumentsJson) {
    if (name == "set_voice_volume") {
        return setVoiceVolume(argumentsJson);
    }
    return toolError("Unknown tool: " + name);
}

int main(int argc, const char* argv[]) {
    std::optional<std::string> voice;
    for (int i = 1; i < argc; i++) {
        const std::string argument = argv[i];
        if (argument == "--voice" && i + 1 < argc) {
            voice = argv[++i];
        } else {
            std::cerr << "Usage: " << argv[0] << " [--voice <name>]" << std::endl;
            return 1;
        }
    }
    if (voice.has_value() && std::find(VOICES.begin(), VOICES.end(), voice.value()) == VOICES.end()) {
        std::cerr << "Unknown voice: " << voice.value() << ". Voices:";
        for (const auto& name : VOICES) {
            std::cerr << " " << name;
        }
        std::cerr << std::endl;
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
    SBAnyMap sdkConfig({ { "appID", environmentOr("SWITCHBOARD_APP_ID", DEFAULT_APP_ID) },
                         { "appSecret", environmentOr("SWITCHBOARD_APP_SECRET", DEFAULT_APP_SECRET) },
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

    if (voice.has_value()) {
        auto voiceResult = Switchboard::setValue(LIVE_NODE, "voice", voice.value());
        if (voiceResult.isError()) {
            std::cerr << "Failed to set the voice: " << voiceResult.error().message << std::endl;
            return 1;
        }
    }

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

        const ToolOutput output = handleToolCall(name, argumentsJson);
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
    Switchboard::addEventListener(LIVE_NODE, "sessionIdle", [](const Event&) {
        printLine("[Idle: the session closed after 15 s of quiet. Speak to continue the conversation.]");
    });
    Switchboard::addEventListener(LIVE_NODE, "sessionWoken", [](const Event&) {
        printLine("[Reopened.]");
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
