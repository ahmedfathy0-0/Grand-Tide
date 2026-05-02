#pragma once

#include "audio-player.hpp"
#include <iostream>

namespace our {

    // Handles looping ocean background audio playback using AudioPlayer
    class OceanAudio {
        AudioPlayer audio;

    public:
        bool start(const char* path = "assets/audios/ocean.mp3") {
            return audio.play(path, true);
        }

        void setVolume(float volume) {
            audio.setVolume(volume);
        }

        void stop() {
            audio.stop();
        }
    };

} // namespace our
