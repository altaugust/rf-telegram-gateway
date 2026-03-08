#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <chrono>
#include <thread>
#include <portaudio.h>
#include <sndfile.h>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cstdio>

#define SAMPLE_RATE 44100
#define CHANNELS 1
#define FRAMES_PER_BUFFER 1024
#define SILENCE_LIMIT_SEC 6.0        // Increased tolerance for human speech pauses
#define MAX_RECORD_TIME_SEC 60.0
#define IDLE_RECALIBRATION_SEC 60.0  // Recalibrate after 60s of silence
#define POST_TX_RECOVERY_MS 1500     // Delay to avoid sampling squelch tail noise
#define TRIGGER_PERSISTENCE_SEC 0.2  // 200ms required to start trigger
#define CALIBRATION_SAMPLING_SEC 2   // Actual time spent sampling noise
#define SAFETY_ANALYSIS_SEC 0.5      // Analyze first 500ms of recording

struct AudioData {
    std::vector<short> recordBuffer;
    bool isRecording = false;
    double triggerStartTime = 0.0;
    double silenceStartTime = 0.0;
    double recordStartTime = 0.0;
    
    double highThreshold = 15000.0; // Trigger level (1.3x peak)
    double lowThreshold = 11000.0;  // Anchor level (1.1x peak)
    
    bool transmissionFinished = false;
    long recordedFrames = 0;
};

bool stopRequested = false;
void signalHandler(int signum) {
    stopRequested = true;
}

// Low-latency audio callback with Hysteresis and Clean UI
static int audioCallback(const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData) {
    AudioData* data = (AudioData*)userData;
    const short* in = (const short*)inputBuffer;

    if (in == nullptr || data->transmissionFinished) return paContinue;

    // RMS Calculation
    double sumSquares = 0;
    for (unsigned long i = 0; i < framesPerBuffer; i++) {
        double val = (double)in[i];
        sumSquares += val * val;
    }
    double rms = std::sqrt(sumSquares / framesPerBuffer);

    double currentTime = timeInfo->inputBufferAdcTime;

    if (!data->isRecording) {
        // State: LISTENING -> Use HIGH_THRESHOLD + Persistence
        if (rms > data->highThreshold) {
            if (data->triggerStartTime == 0) {
                data->triggerStartTime = currentTime;
            } else if (currentTime - data->triggerStartTime > TRIGGER_PERSISTENCE_SEC) {
                // State Transition: LISTENING -> RECORDING
                std::printf("\n[RECORDING] Signal active...                      \n");
                std::fflush(stdout);
                data->isRecording = true;
                data->recordStartTime = currentTime;
                data->silenceStartTime = 0;
                data->recordedFrames = 0;
            }
        } else {
            data->triggerStartTime = 0;
        }
    }

    if (data->isRecording) {
        // State: RECORDING -> Use LOW_THRESHOLD (Anchor)
        if (data->recordedFrames + framesPerBuffer < data->recordBuffer.size()) {
            std::memcpy(&data->recordBuffer[data->recordedFrames], in, framesPerBuffer * sizeof(short));
            data->recordedFrames += framesPerBuffer;
        }

        // Hysteresis Anchor Check
        if (rms < data->lowThreshold) {
            if (data->silenceStartTime == 0) data->silenceStartTime = currentTime;
            else if (currentTime - data->silenceStartTime > SILENCE_LIMIT_SEC) {
                data->transmissionFinished = true; // State: Finalize
            }
        } else {
            data->silenceStartTime = 0;
        }

        // Max Duration Safety
        if (currentTime - data->recordStartTime > MAX_RECORD_TIME_SEC) {
            data->transmissionFinished = true;
        }
    }

    // VU Meter Style Terminal Update (Minimalist & Real-time)
    if (int(currentTime * 10) % 2 == 0) { // Update every ~200ms
        const char* status = data->isRecording ? "RECORDING" : "LISTENING";
        std::printf("\r[%s] RMS: %.1f | Trigger: %.0f | Anchor: %.0f    ", 
                    status, rms, data->highThreshold, data->lowThreshold);
        std::fflush(stdout);
    }

    return paContinue;
}

// Peak-Detecting Calibration (2s sampling)
void calibrateNoiseFloor(PaStream* stream, unsigned long chunks, AudioData& data) {
    std::cout << "[CALIBRATING] Identifying peak noise floor (2s)..." << std::endl;
    double maxPeakRms = 0;
    for (unsigned long i = 0; i < chunks; ++i) {
        if (stopRequested) break;
        short buffer[FRAMES_PER_BUFFER];
        Pa_ReadStream(stream, buffer, FRAMES_PER_BUFFER);
        double sumSquares = 0;
        for (int j = 0; j < FRAMES_PER_BUFFER; ++j) {
            double val = (double)buffer[j];
            sumSquares += val * val;
        }
        double rms = std::sqrt(sumSquares / FRAMES_PER_BUFFER);
        if (rms > maxPeakRms) maxPeakRms = rms;
    }
    
    data.highThreshold = maxPeakRms * 1.3; // High trigger (30%)
    data.lowThreshold = maxPeakRms * 1.1;  // Low anchor (10%)
    
    std::cout << "[READY] Thresholds updated. Next check in 60s." << std::endl;
    std::cout << "[READY] Trigger at: " << (int)data.highThreshold << ", Hold at: " << (int)data.lowThreshold << std::endl;
}

void saveWav(const std::string& filename, const short* buffer, long frames) {
    SF_INFO sfinfo;
    std::memset(&sfinfo, 0, sizeof(sfinfo));
    sfinfo.channels = CHANNELS;
    sfinfo.samplerate = SAMPLE_RATE;
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE* outfile = sf_open(filename.c_str(), SFM_WRITE, &sfinfo);
    if (outfile) {
        sf_write_short(outfile, buffer, frames);
        sf_close(outfile);
    }
}

int main() {
    // Silence host-level ALSA / PortAudio errors to stderr
    std::freopen("/dev/null", "w", stderr);

    std::cout << "========================================" << std::endl;
    std::cout << "      RF ANALOG-DIGITAL GATEWAY      " << std::endl;
    std::cout << "========================================" << std::endl;

    std::signal(SIGINT, signalHandler);

    if (Pa_Initialize() != paNoError) return 1;

    AudioData data;
    data.recordBuffer.resize(SAMPLE_RATE * (MAX_RECORD_TIME_SEC + 5));

    PaStreamParameters inputParams;
    inputParams.device = Pa_GetDefaultInputDevice();
    if (inputParams.device == paNoDevice) {
        Pa_Terminate();
        return 1;
    }
    inputParams.channelCount = CHANNELS;
    inputParams.sampleFormat = paInt16;
    inputParams.suggestedLatency = Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = NULL;

    PaStream* stream = nullptr;
    PaError err;

    std::cout << "[*] System started. Listening for transmissions..." << std::endl;

    auto lastActivityOrCalibration = std::chrono::steady_clock::now();

    while (!stopRequested) {
        // Calibration Phase
        err = Pa_OpenStream(&stream, &inputParams, NULL, SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff, NULL, NULL);
        if (err != paNoError) break;
        Pa_StartStream(stream);
        
        calibrateNoiseFloor(stream, (SAMPLE_RATE / FRAMES_PER_BUFFER) * CALIBRATION_SAMPLING_SEC, data);
        lastActivityOrCalibration = std::chrono::steady_clock::now();
        
        Pa_StopStream(stream);
        Pa_CloseStream(stream);

        // Monitoring Loop
        err = Pa_OpenStream(&stream, &inputParams, NULL, SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff, audioCallback, &data);
        if (err != paNoError) break;
        Pa_StartStream(stream);

        while (!stopRequested && !data.transmissionFinished) {
            Pa_Sleep(200);
            
            auto now = std::chrono::steady_clock::now();
            auto idle_sec = std::chrono::duration_cast<std::chrono::seconds>(now - lastActivityOrCalibration).count();
            
            if (data.isRecording || data.triggerStartTime > 0) {
                lastActivityOrCalibration = now;
            }

            if (!data.isRecording && idle_sec >= IDLE_RECALIBRATION_SEC) {
                std::cout << "\n[IDLE] 60s reached. Starting 2s recalibration..." << std::endl;
                break; 
            }
        }

        if (data.transmissionFinished) {
            std::printf("\n[RELEASE] Silence detected. Finalizing...\n");
            std::fflush(stdout);
            
            Pa_Sleep(100); 
            Pa_StopStream(stream);
            Pa_CloseStream(stream);
            
            double duration = (double)data.recordedFrames / SAMPLE_RATE;

            bool validSignal = true;
            if (duration < 1.0) validSignal = false;
            
            if (validSignal && duration >= SAFETY_ANALYSIS_SEC) {
                long analysisFrames = SAMPLE_RATE * SAFETY_ANALYSIS_SEC;
                double sumSqr = 0;
                for(long i=0; i<analysisFrames; ++i) {
                    double v = (double)data.recordBuffer[i];
                    sumSqr += v*v;
                }
                double avgStartRms = std::sqrt(sumSqr / analysisFrames);
                if (avgStartRms < data.lowThreshold * 1.2) {
                    std::cout << "[!] Discarded: Initial 500ms signal too weak (" << (int)avgStartRms << " RMS)" << std::endl;
                    validSignal = false;
                }
            }

            if (!validSignal) {
                if (duration < 1.0) std::cout << "[!] Discarded: Transmission too short (" << duration << "s)" << std::endl;
            } else {
                std::string filename = "intercept.wav";
                saveWav(filename, data.recordBuffer.data(), data.recordedFrames);
                std::cout << "[+] Transmission saved (" << duration << "s). Handoff to Python..." << std::endl;
                std::system(("python3 telegram_gateway.py " + filename + " " + std::to_string(duration)).c_str());
            }

            data.isRecording = false;
            data.transmissionFinished = false;
            data.recordedFrames = 0;
            data.triggerStartTime = 0;

            std::cout << "[SLEEP] Waiting for squelch tail to clear..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(POST_TX_RECOVERY_MS));
            
            lastActivityOrCalibration = std::chrono::steady_clock::now();
        } else {
            Pa_StopStream(stream);
            Pa_CloseStream(stream);
        }
    }

    Pa_Terminate();
    std::cout << "\n[√] Hardware released. Safe shutdown." << std::endl;
    return 0;
}
