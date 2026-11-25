# SwitchboardCLIDemo

This lightweight C++ project includes multiple CLI demo apps showcasing the capabilities of the Switchboard SDK, such as speech-to-text (STT), text-to-speech (TTS), voice activity detection (VAD), and a voice conversation with OpenAI GPT-Live. Built with CMake for easy cross-platform support, it runs seamlessly on Windows, Linux, and macOS—making it an ideal starting point for exploring and integrating audio features into your own applications.

## Prerequisites

Ensure you have the following installed:
- **CMake** (>= 3.28)
- **C++ compiler** (e.g., GCC, Clang, MSVC)
- **Xcode** (macOS)
- **Python 3.6 or newer**

### macOS

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r requirements.txt
```

### Windows (Command Prompt)

```bat
py -m venv .venv
.venv\Scripts\activate
py -m pip install -r requirements.txt
```

### Linux

```bash
sudo apt install python3-venv
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r requirements.txt
```

## Build & Run

### macOS

```bash
inv build-macos --clean
cd out/bin
./Sine # or ./TTS ./STT ./STTtoTTS ./OpenAILive
```

### Windows (Command Prompt)

```bat
inv build-windows --clean
cd out\bin
Sine.exe
```

### Linux

```bash
inv build-linux --clean
cd out/bin
./Sine # or ./TTS ./STT ./STTtoTTS ./OpenAILive
```

## OpenAI Live

`OpenAILive` holds a voice conversation with OpenAI GPT-Live through the
[`OpenAI.Live` node](https://docs.switchboard.audio/extensions/openai/openai-live-node).

It needs your own Switchboard app: [register one](https://console.switchboard.audio/register), then replace `"demo"`
with its `appID` and `appSecret` in `demos/openai_live/main.cpp` and build again.

```bash
cd out/bin
./OpenAILive
```

Use headphones, so the assistant doesn't hear itself. On macOS, allow your terminal to use the microphone when asked.
Then talk; the transcript prints as you go. Say "play some music" or "pause the music" to try the tools. Press Enter
to stop.

## Documentation

- [Switchboard SDK](https://docs.switchboard.audio/)
- Integration: [macOS](https://docs.switchboard.audio/integration/macos),
  [Linux](https://docs.switchboard.audio/integration/linux),
  [Windows](https://docs.switchboard.audio/integration/windows)
- [OpenAI Live credentials and billing](https://docs.switchboard.audio/extensions/openai/openai-live-node#credentials-and-billing)
