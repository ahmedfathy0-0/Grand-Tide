#pragma once

#include <glad/gl.h>
#include <string>
#include <vector>

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

}
