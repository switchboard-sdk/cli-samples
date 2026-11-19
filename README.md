# SwitchboardCLIDemo

This lightweight C++ project includes multiple CLI demo apps showcasing the capabilities of the Switchboard SDK, such as speech-to-text (STT), text-to-speech (TTS), voice activity detection (VAD), and a voice conversation with OpenAI GPT-Live. Built with CMake for easy cross-platform support, it runs seamlessly on Windows, Linux, and macOS—making it an ideal starting point for exploring and integrating audio features into your own applications.

## Prerequisites

Ensure you have the following installed:
- **CMake** (>= 3.28)
- **C++ compiler** (e.g., GCC, Clang, MSVC)
- **Python 3** with the build tasks' dependencies: `pip install -r requirements.txt`

## SDK version

The build downloads the Switchboard SDK and its extensions. Until 3.3.0 is released, it uses the latest 3.3.0
development build; `--clean` gets the newest one. To build against a released version instead, configure CMake with
`-DSWITCHBOARD_PACKAGE_CHANNEL=release -DSWITCHBOARD_PACKAGE_VERSION=<version>`.

## Build & Run

### macOS

```bash
inv build-macos --clean
cd out/bin
./Sine # or ./TTS ./STT ./STTtoTTS ./OpenAILive
```

### Windows

```bash
inv build-windows --clean
cd out/bin
./Sine.exe # or ./TTS.exe ./STT.exe ./STTtoTTS.exe
```

### Linux

```bash
inv build-linux --clean
cd out/bin
./Sine # or ./TTS ./STT ./STTtoTTS ./OpenAILive
```

## OpenAI Live

`OpenAILive` holds a voice conversation with OpenAI GPT-Live through the
[`OpenAI.Live` node](https://docs.switchboard.audio/extensions/openai/openai-live-node), on macOS and Linux.

```bash
cd out/bin
./OpenAILive                 # or ./OpenAILive --voice cedar
```

Use headphones: there's no echo cancellation here, so through speakers the assistant hears itself. On macOS, allow
your terminal to use the microphone when asked. Then talk; the transcript prints as you go. Press Enter to stop.

Try "speak up", "a bit quieter" or "set your volume to 80": the `set_voice_volume` tool changes the gain of a
`Switchboard.Gain` node after the voice. The app answers the tool in code: it listens for the node's `toolCall`
event and replies with `submitToolResult`, or `submitToolError` when it fails.

The voice can't change during a conversation, so pick it at start with `--voice`: alloy, ash, ballad, cedar, coral,
echo, marin (the default), sage, shimmer or verse. The graph is in `demos/openai_live/OpenAILiveExample.json`. It sets
`delegation: "responses"`, which tools need, and idle mode: after 15 s of quiet the session closes, so it isn't
billed, and a `Silero.VAD` node reopens it when you speak, with the conversation intact.

It runs on a shared Switchboard app that's built in and tightly limited. For anything beyond a first try, use your
own app:

```bash
SWITCHBOARD_APP_ID=<your app ID> SWITCHBOARD_APP_SECRET=<your app secret> ./OpenAILive
```
