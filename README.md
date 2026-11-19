# SwitchboardCLIDemo

This lightweight C++ project includes multiple CLI demo apps showcasing the capabilities of the Switchboard SDK, such as speech-to-text (STT), text-to-speech (TTS), voice activity detection (VAD), and a voice conversation with OpenAI GPT-Live. Built with CMake for easy cross-platform support, it runs seamlessly on Windows, Linux, and macOS—making it an ideal starting point for exploring and integrating audio features into your own applications.

## Prerequisites

Ensure you have the following installed:
- **CMake** (>= 3.28)
- **C++ compiler** (e.g., GCC, Clang, MSVC)
- **Xcode** (macOS)
- **Python 3**

## Build & Run

### macOS

```bash
pip install -r requirements.txt
inv build-macos --clean
cd out/bin
./Sine # or ./TTS ./STT ./STTtoTTS ./OpenAILive
```

### Windows

```bash
pip install -r requirements.txt
inv build-windows --clean
cd out/bin
./Sine.exe # or ./TTS.exe ./STT.exe ./STTtoTTS.exe
```

### Linux

```bash
pip install -r requirements.txt
inv build-linux --clean
cd out/bin
./Sine # or ./TTS ./STT ./STTtoTTS ./OpenAILive
```

## OpenAI Live

`OpenAILive` holds a voice conversation with OpenAI GPT-Live through the
[`OpenAI.Live` node](https://docs.switchboard.audio/extensions/openai/openai-live-node).

```bash
cd out/bin
./OpenAILive
```

Use headphones, so the assistant doesn't hear itself. On macOS, allow your terminal to use the microphone when asked.
Then talk; the transcript prints as you go. Say "play some music" or "pause the music" to try the tools. Press Enter
to stop.

It runs on a shared Switchboard app that's tightly limited. To use your own:

```bash
SWITCHBOARD_APP_ID=<your app ID> SWITCHBOARD_APP_SECRET=<your app secret> ./OpenAILive
```
