#pragma once

#include <glad/gl.h>
#include <string>
#include <vector>

#ifdef USE_FFMPEG

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

struct ma_device;

namespace our {

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    bool load(const std::string& filepath);
    bool update(double deltaTime);
    GLuint getTexture() const { return textureID; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    double getDuration() const { return duration; }
    bool isLoaded() const { return loaded; }
    void stopAudio();

private:
    // Video
    AVFormatContext* formatContext;
    AVCodecContext* videoCodecContext;
    AVFrame* frame;
    AVFrame* frameRGBA;
    SwsContext* swsContext;
    AVPacket* packet;

    GLuint textureID;
    uint8_t* videoBuffer;

    int videoStreamIndex;
    int width, height;
    double duration;
    double currentTime;
    double nextFrameTime;
    bool loaded;
    bool endOfStream;

    // Audio
    AVCodecContext* audioCodecContext;
    SwrContext* swrContext;
    int audioStreamIndex;
    std::vector<float> audioPCM;   // interleaved stereo float32
    ma_device* audioDevice;
    size_t audioReadPos;

    bool decodeNextVideoFrame();
    void uploadFrameToTexture();
    bool decodeAllAudio();
};

} // namespace our

#else // USE_FFMPEG not defined — stub implementation

namespace our {

// Stub VideoPlayer when FFmpeg is not available
class VideoPlayer {
public:
    VideoPlayer() = default;
    ~VideoPlayer() = default;
    bool load(const std::string&) { return false; }
    bool update(double) { return false; }
    GLuint getTexture() const { return 0; }
    int getWidth() const { return 0; }
    int getHeight() const { return 0; }
    double getDuration() const { return 0; }
    bool isLoaded() const { return false; }
    void stopAudio() {}
};

} // namespace our

#endif // USE_FFMPEG
