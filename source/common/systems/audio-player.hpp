#pragma once

#include <miniaudio.h>
#include <iostream>

namespace our {

    class AudioPlayer {
        ma_decoder decoder;
        ma_device* device = nullptr;
        bool ready = false;
        bool isLooping = false;

        static void audioCallback(ma_device* pDevice, void* pOutput, const void*, ma_uint32 frameCount) {
            auto* player = static_cast<AudioPlayer*>(pDevice->pUserData);
            ma_uint64 framesRead = 0;
            ma_decoder_read_pcm_frames(&player->decoder, pOutput, frameCount, &framesRead);
            if (framesRead < frameCount) {
                if (player->isLooping) {
                    ma_decoder_seek_to_pcm_frame(&player->decoder, 0);
                    ma_uint64 remaining = frameCount - framesRead;
                    ma_uint64 framesRead2 = 0;
                    ma_decoder_read_pcm_frames(&player->decoder,
                        (ma_uint8*)pOutput + framesRead * player->decoder.outputChannels * sizeof(float),
                        remaining, &framesRead2);
                } else {
                    // Fill remaining with silence
                    ma_uint32 remaining = frameCount - framesRead;
                    memset((ma_uint8*)pOutput + framesRead * player->decoder.outputChannels * sizeof(float), 0, remaining * player->decoder.outputChannels * sizeof(float));
                }
            }
        }

    public:
        bool play(const char* path, bool loop = false) {
            stop();
            isLooping = loop;
            if (ma_decoder_init_file(path, nullptr, &decoder) == MA_SUCCESS) {
                device = new ma_device();
                ma_device_config devCfg = ma_device_config_init(ma_device_type_playback);
                devCfg.playback.format = decoder.outputFormat;
                devCfg.playback.channels = decoder.outputChannels;
                devCfg.sampleRate = decoder.outputSampleRate;
                devCfg.dataCallback = audioCallback;
                devCfg.pUserData = this;
                if (ma_device_init(nullptr, &devCfg, device) == MA_SUCCESS) {
                    ma_device_start(device);
                    ready = true;
                    std::cerr << "[AudioPlayer] Started playing " << path << std::endl;
                } else {
                    std::cerr << "[AudioPlayer] Device init failed" << std::endl;
                    delete device; device = nullptr;
                    ma_decoder_uninit(&decoder);
                }
            } else {
                std::cerr << "[AudioPlayer] Could not load " << path << std::endl;
            }
            return ready;
        }

        void setVolume(float volume) {
            if (device) ma_device_set_master_volume(device, volume);
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
        
        ~AudioPlayer() {
            stop();
        }
    };

} // namespace our
