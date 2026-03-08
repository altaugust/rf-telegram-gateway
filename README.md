# RF-Telegram-Gateway

A hybrid Analog-to-Digital Gateway designed to monitor analog RF transmissions (VHF/UHF) via a Voice-Operated Exchange (VOX) and broadcast the results to Telegram.

## Project Overview

This system utilizes a high-efficiency architecture:
- **C++ Engine**: Real-time Signal Processing (DSP), VOX monitoring, and transient filtering.
- **Python Gateway**: Modular multi-destination broadcasting to groups, channels, and private chats.

## Multi-Distro Installation

### Ubuntu / Debian
```bash
sudo apt install portaudio19-dev libsndfile1-dev g++ libasound2-plugins pulseaudio
```

### Fedora (Toolbox / Workstation)
```bash
sudo dnf install portaudio-devel libsndfile-devel gcc-c++ alsa-plugins-pulseaudio pulseaudio-utils
```

## The "Hysteresis" Engine

The system is designed to handle the physical quirks of analog radio:
- **Dual Thresholds**:
  - **HIGH (Trigger)**: Level required to *start* recording (1.3x noise peak).
  - **LOW (Anchor)**: Level required to *sustain* recording (1.1x noise peak).
- **Signal Persistence**: A signal must remain high for at least 200ms to be considered a transmission.
- **Silence Tolerance**: A 6.0-second silence limit prevents cutting off transmissions during natural speech pauses.
- **Noise Filtering**: Short bursts (< 1s) or weak initial signals are automatically discarded.

## Compilation & Execution

**Important**: You must be inside the project folder before proceeding.

### Compile
```bash
g++ audio_monitor.cpp -o audio_monitor -lportaudio -lsndfile -lasound -lpthread -lm

```

### Run
```bash
./audio_monitor
```

## Broadcasting Setup

Configure your target destinations in the `.env` file. You can broadcast to multiple IDs simultaneously by using a comma-separated list:

```env
TELEGRAM_TOKEN=123456:ABC-DEF
CHAT_ID=1234567, -100987654321, 55443322
```
*Note: Support includes Private Chats (positive IDs) and Groups/Channels (negative IDs starting with -100).*

## Hardware Calibration

- **Initial/Periodic Calibration**: Upon startup and after 60s of idle time, the system samples the background noise for **2 seconds** to set the optimal thresholds.
- **Squelch Tail Mitigation**: An automatic **1.5s delay** is implemented after each transmission before the next calibration starts, ensuring radio static isn't sampled as noise.

## Security
The `.env` file containing your credentials is excluded via `.gitignore`. Never share this file or commit it to a public repository.
