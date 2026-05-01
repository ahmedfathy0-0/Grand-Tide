#pragma once

#include <miniaudio.h>
#include <iostream>

namespace our {

    // Handles looping ocean background audio playback.
    class OceanAudio {
        ma_decoder decoder;
        ma_device* device = nullptr;
        bool ready = false;

        static void audioCallback(ma_device* pDevice, void* pOutput, const void*, ma_uint32 frameCount) {
            auto* dec = static_cast<ma_decoder*>(pDevice->pUserData);
            ma_uint64 framesRead = 0;
            ma_decoder_read_pcm_frames(dec, pOutput, frameCount, &framesRead);
            // Loop: if we didn't get enough frames, seek to start and read the rest
            if (framesRead < frameCount) {
                ma_decoder_seek_to_pcm_frame(dec, 0);
                ma_uint64 remaining = frameCount - framesRead;
                ma_uint64 framesRead2 = 0;
                ma_decoder_read_pcm_frames(dec,
                    (ma_uint8*)pOutput + framesRead * dec->outputChannels * sizeof(float),
                    remaining, &framesRead2);
            }
        }

    public:
        bool start(const char* path = "assets/audios/ocean.mp3") {
            if (ma_decoder_init_file(path, nullptr, &decoder) == MA_SUCCESS) {
                device = new ma_device();
                ma_device_config devCfg = ma_device_config_init(ma_device_type_playback);
                devCfg.playback.format = decoder.outputFormat;
                devCfg.playback.channels = decoder.outputChannels;
                devCfg.sampleRate = decoder.outputSampleRate;
                devCfg.dataCallback = audioCallback;
                devCfg.pUserData = &decoder;
                if (ma_device_init(nullptr, &devCfg, device) == MA_SUCCESS) {
                    ma_device_start(device);
                    ready = true;
                    std::cerr << "[OceanAudio] Started (looping)" << std::endl;
                } else {
                    std::cerr << "[OceanAudio] Device init failed" << std::endl;
                    delete device; device = nullptr;
                    ma_decoder_uninit(&decoder);
                }
            } else {
                std::cerr << "[OceanAudio] Could not load " << path << std::endl;
            }
            return ready;
        }

        void setVolume(float volume) {
            if (device) {
                ma_device_set_master_volume(device, volume);
            }
        }

        void stop() {
            if (device) {
                ma_device_stop(device);
                ma_device_uninit(device);
                delete device; device = nullptr;
            }
            if (ready) {
                ma_decoder_uninit(&decoder);
                ready = false;
            }
        }
    };

} // namespace our
