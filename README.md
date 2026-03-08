# RF-Telegram-Gateway

A hybrid Analog-to-Digital Gateway designed to monitor analog RF transmissions (VHF/UHF) via a Voice-Operated Exchange (VOX) and broadcast the results to Telegram.

> [!CAUTION]
> **⚠️ LEGAL DISCLAIMER & RESPONSIBILITY**
>
> This project is for **educational and academic purposes only**. The user is solely responsible for complying with local telecommunications regulations (such as ANATEL in Brazil or the FCC in the USA). The developer is not responsible for any misuse, illegal transmissions, or interference caused by this software. Use it responsibly and within the law.

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

## Safe Testing & Frequencies

**Hardware Safety**: NEVER transmit without a proper antenna or a **50 Ω Dummy Load**. Operating without a matched load can cause permanent damage to your radio hardware.

### Recommended Unlicensed Frequencies
For testing purposes, stick to unlicensed bands:

| Service | Frequency | Notes |
| :--- | :--- | :--- |
| **FRS** (Channel 1) | 462.5625 MHz | Family Radio Service |
| **LPD** | 433.075 MHz | Low Power Device |

> [!WARNING]
> **Stay away** from Airband (108-137 MHz) and any Emergency or Commercial frequencies. Interference with these services is a serious legal offense.

## The "Hysteresis" Engine

The system handles the physical quirks of analog radio using a dual-threshold approach:
- **HIGH (Trigger)**: Level required to *start* recording (1.3x noise peak).
- **LOW (Anchor)**: Level required to *sustain* recording (1.1x noise peak).
- **Persistence**: Signal must remain high for at least **200ms** to trigger.
- **Silence Tolerance**: A **6.0s limit** prevent cuts during speech pauses.

## Broadcasting Setup

Configure your target destinations in the `.env` file. You can broadcast to multiple IDs simultaneously by using a comma-separated list:

```env
TELEGRAM_TOKEN=123456:ABC-DEF
CHAT_ID=1234567, -100987654321, 55443322
```
*Note: Support includes Private Chats (positive IDs) and Groups/Channels (negative IDs starting with -100).*

## Compilation & Execution

```bash
# Compile
g++ audio_monitor.cpp -o audio_monitor -lportaudio -lsndfile -lasound -lpthread -lm

# Run
./audio_monitor
```

## Hardware Calibration

- **Initial/Periodic Calibration**: Upon startup and after 60s of idle time, the system samples the background noise for **2 seconds** to set optimal thresholds.
- **Squelch Tail Mitigation**: An automatic **1.5s delay** after each transmission prevents radio static from being sampled as noise.

## Security
The `.env` file containing your credentials is excluded via `.gitignore`. Never share this file or commit it to a public repository.
