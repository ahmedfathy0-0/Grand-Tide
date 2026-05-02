#include "video-player.hpp"

#ifdef USE_FFMPEG

#include <miniaudio.h>
#include <iostream>
#include <cstring>

namespace our {

// Miniaudio callback — reads from the pre-decoded PCM buffer
static void audioCallback(ma_device* pDevice, void* pOutput, const void*, ma_uint32 frameCount) {
    auto* player = static_cast<VideoPlayer*>(pDevice->pUserData);
    float* out = static_cast<float*>(pOutput);
    // Access audio buffer through the player's public interface isn't possible,
    // so we use a simple approach: the player stores a pointer to its buffer.
    // Instead, we'll use a struct to pass the data.
    // Actually, let's use a different approach - store a pointer to the data.
    // We'll handle this via a helper struct stored in pUserData.
}

// Helper struct to pass audio data to miniaudio callback
struct AudioContext {
    const std::vector<float>* pcm;
    size_t* readPos;
};

static void audioCallback2(ma_device* pDevice, void* pOutput, const void*, ma_uint32 frameCount) {
    auto* ctx = static_cast<AudioContext*>(pDevice->pUserData);
    const std::vector<float>& pcm = *ctx->pcm;
    size_t& readPos = *ctx->readPos;
    float* out = static_cast<float*>(pOutput);
    size_t totalFloats = pcm.size();
    size_t samplesToWrite = frameCount * 2; // stereo

    for (size_t i = 0; i < samplesToWrite; i++) {
        if (readPos < totalFloats) {
            out[i] = pcm[readPos++];
        } else {
            out[i] = 0.0f;
        }
    }
}

VideoPlayer::VideoPlayer()
    : formatContext(nullptr), videoCodecContext(nullptr), frame(nullptr)
    , frameRGBA(nullptr), swsContext(nullptr), packet(nullptr)
    , textureID(0), videoBuffer(nullptr), videoStreamIndex(-1)
    , width(0), height(0), duration(0.0), currentTime(0.0)
    , nextFrameTime(0.0), loaded(false), endOfStream(false)
    , audioCodecContext(nullptr), swrContext(nullptr)
    , audioStreamIndex(-1), audioDevice(nullptr), audioReadPos(0) {}

VideoPlayer::~VideoPlayer() {
    stopAudio();
    if (textureID) glDeleteTextures(1, &textureID);
    if (videoBuffer) av_free(videoBuffer);
    if (frameRGBA) av_frame_free(&frameRGBA);
    if (frame) av_frame_free(&frame);
    if (packet) av_packet_free(&packet);
    if (swsContext) sws_freeContext(swsContext);
    if (swrContext) swr_free(&swrContext);
    if (audioCodecContext) avcodec_free_context(&audioCodecContext);
    if (videoCodecContext) avcodec_free_context(&videoCodecContext);
    if (formatContext) avformat_close_input(&formatContext);
}

void VideoPlayer::stopAudio() {
    if (audioDevice) {
        ma_device_stop(audioDevice);
        ma_device_uninit(audioDevice);
        delete audioDevice;
        audioDevice = nullptr;
    }
}

bool VideoPlayer::load(const std::string& filepath) {
    if (avformat_open_input(&formatContext, filepath.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "[VideoPlayer] Could not open: " << filepath << std::endl;
        return false;
    }
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        std::cerr << "[VideoPlayer] No stream info" << std::endl;
        return false;
    }

    // Find video and audio streams
    videoStreamIndex = -1;
    audioStreamIndex = -1;
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoStreamIndex == -1)
            videoStreamIndex = i;
        else if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIndex == -1)
            audioStreamIndex = i;
    }
    if (videoStreamIndex == -1) {
        std::cerr << "[VideoPlayer] No video stream" << std::endl;
        return false;
    }

    // --- Video codec ---
    AVCodecParameters* videoParams = formatContext->streams[videoStreamIndex]->codecpar;
    const AVCodec* videoCodec = avcodec_find_decoder(videoParams->codec_id);
    if (!videoCodec) { std::cerr << "[VideoPlayer] Unsupported video codec" << std::endl; return false; }
    videoCodecContext = avcodec_alloc_context3(videoCodec);
    if (!videoCodecContext) return false;
    if (avcodec_parameters_to_context(videoCodecContext, videoParams) < 0) return false;
    if (avcodec_open2(videoCodecContext, videoCodec, nullptr) < 0) {
        std::cerr << "[VideoPlayer] Could not open video codec" << std::endl;
        return false;
    }

    frame = av_frame_alloc();
    frameRGBA = av_frame_alloc();
    packet = av_packet_alloc();
    if (!frame || !frameRGBA || !packet) return false;

    width = videoCodecContext->width;
    height = videoCodecContext->height;
    duration = formatContext->duration / (double)AV_TIME_BASE;

    // Use RGBA to avoid GL_UNPACK_ALIGNMENT issues (fixes gray color)
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height, 1);
    videoBuffer = (uint8_t*)av_malloc(numBytes);
    if (!videoBuffer) return false;
    av_image_fill_arrays(frameRGBA->data, frameRGBA->linesize, videoBuffer, AV_PIX_FMT_RGBA, width, height, 1);

    swsContext = sws_getContext(width, height, videoCodecContext->pix_fmt,
                                width, height, AV_PIX_FMT_RGBA,
                                SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!swsContext) { std::cerr << "[VideoPlayer] SWS context failed" << std::endl; return false; }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Decode first frame
    if (decodeNextVideoFrame()) {
        uploadFrameToTexture();
        nextFrameTime = frame->pts * av_q2d(formatContext->streams[videoStreamIndex]->time_base);
    }

    // --- Audio ---
    if (audioStreamIndex != -1) {
        decodeAllAudio();
    }

    loaded = true;
    currentTime = 0.0;
    endOfStream = false;
    std::cerr << "[VideoPlayer] Loaded: " << filepath << " (" << width << "x" << height
              << ", " << duration << "s, audio=" << (audioStreamIndex != -1 ? "yes" : "no") << ")" << std::endl;
    return true;
}

bool VideoPlayer::decodeNextVideoFrame() {
    int ret;
    while (true) {
        ret = avcodec_receive_frame(videoCodecContext, frame);
        if (ret == 0) {
            sws_scale(swsContext, frame->data, frame->linesize, 0, height,
                      frameRGBA->data, frameRGBA->linesize);
            return true;
        }
        if (ret == AVERROR_EOF) { endOfStream = true; return false; }
        if (ret != AVERROR(EAGAIN)) return false;

        while (true) {
            ret = av_read_frame(formatContext, packet);
            if (ret < 0) {
                avcodec_send_packet(videoCodecContext, nullptr);
                break;
            }
            if (packet->stream_index == videoStreamIndex) {
                ret = avcodec_send_packet(videoCodecContext, packet);
                av_packet_unref(packet);
                if (ret < 0 && ret != AVERROR(EAGAIN)) continue;
                break;
            }
            av_packet_unref(packet);
        }
    }
}

bool VideoPlayer::decodeAllAudio() {
    AVCodecParameters* audioParams = formatContext->streams[audioStreamIndex]->codecpar;
    const AVCodec* audioCodec = avcodec_find_decoder(audioParams->codec_id);
    if (!audioCodec) { std::cerr << "[VideoPlayer] Unsupported audio codec" << std::endl; return false; }

    audioCodecContext = avcodec_alloc_context3(audioCodec);
    if (!audioCodecContext) return false;
    if (avcodec_parameters_to_context(audioCodecContext, audioParams) < 0) return false;
    if (avcodec_open2(audioCodecContext, audioCodec, nullptr) < 0) {
        std::cerr << "[VideoPlayer] Could not open audio codec" << std::endl;
        return false;
    }

    // Set up resampler: convert to stereo float32 at 48kHz
    int outSampleRate = 48000;
    swrContext = swr_alloc();
    if (!swrContext) { std::cerr << "[VideoPlayer] SWR alloc failed" << std::endl; return false; }

    AVChannelLayout outChLayout = AV_CHANNEL_LAYOUT_STEREO;
    av_opt_set_chlayout(swrContext, "out_chlayout", &outChLayout, 0);
    av_opt_set_int(swrContext, "out_sample_rate", outSampleRate, 0);
    av_opt_set_sample_fmt(swrContext, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
    av_opt_set_chlayout(swrContext, "in_chlayout", &audioCodecContext->ch_layout, 0);
    av_opt_set_int(swrContext, "in_sample_rate", audioCodecContext->sample_rate, 0);
    av_opt_set_sample_fmt(swrContext, "in_sample_fmt", audioCodecContext->sample_fmt, 0);

    if (swr_init(swrContext) < 0) {
        std::cerr << "[VideoPlayer] SWR init failed" << std::endl;
        return false;
    }

    // Seek back to beginning for audio
    av_seek_frame(formatContext, audioStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
    // Also need to flush video decoder since we seeked
    avcodec_flush_buffers(videoCodecContext);

    AVFrame* audioFrame = av_frame_alloc();
    AVPacket* audioPacket = av_packet_alloc();

    while (av_read_frame(formatContext, audioPacket) >= 0) {
        if (audioPacket->stream_index == audioStreamIndex) {
            if (avcodec_send_packet(audioCodecContext, audioPacket) == 0) {
                while (avcodec_receive_frame(audioCodecContext, audioFrame) == 0) {
                    // Estimate output samples
                    int outSamples = swr_get_out_samples(swrContext, audioFrame->nb_samples);
                    std::vector<float> tempBuf(outSamples * 2);
                    uint8_t* outBuf = reinterpret_cast<uint8_t*>(tempBuf.data());
                    int converted = swr_convert(swrContext, &outBuf, outSamples,
                                                const_cast<const uint8_t**>(audioFrame->data), audioFrame->nb_samples);
                    if (converted > 0) {
                        size_t floatCount = converted * 2;
                        audioPCM.insert(audioPCM.end(), tempBuf.begin(), tempBuf.begin() + floatCount);
                    }
                }
            }
        }
        av_packet_unref(audioPacket);
    }
    // Flush audio decoder
    avcodec_send_packet(audioCodecContext, nullptr);
    while (avcodec_receive_frame(audioCodecContext, audioFrame) == 0) {
        int outSamples = swr_get_out_samples(swrContext, audioFrame->nb_samples);
        std::vector<float> tempBuf(outSamples * 2);
        uint8_t* outBuf = reinterpret_cast<uint8_t*>(tempBuf.data());
        int converted = swr_convert(swrContext, &outBuf, outSamples,
                                    const_cast<const uint8_t**>(audioFrame->data), audioFrame->nb_samples);
        if (converted > 0) {
            size_t floatCount = converted * 2;
            audioPCM.insert(audioPCM.end(), tempBuf.begin(), tempBuf.begin() + floatCount);
        }
    }
    // Flush resampler
    {
        int outSamples = 4096;
        std::vector<float> tempBuf(outSamples * 2);
        uint8_t* outBuf = reinterpret_cast<uint8_t*>(tempBuf.data());
        int converted = swr_convert(swrContext, &outBuf, outSamples, nullptr, 0);
        if (converted > 0) {
            size_t floatCount = converted * 2;
            audioPCM.insert(audioPCM.end(), tempBuf.begin(), tempBuf.begin() + floatCount);
        }
    }

    av_frame_free(&audioFrame);
    av_packet_free(&audioPacket);

    std::cerr << "[VideoPlayer] Audio decoded: " << audioPCM.size() / 2 << " frames at " << outSampleRate << "Hz" << std::endl;

    // Now seek back to start for video playback
    av_seek_frame(formatContext, videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(videoCodecContext);
    // Re-decode first video frame after seek
    nextFrameTime = 0.0;
    if (decodeNextVideoFrame()) {
        uploadFrameToTexture();
        nextFrameTime = frame->pts * av_q2d(formatContext->streams[videoStreamIndex]->time_base);
    }

    // Start miniaudio playback
    if (!audioPCM.empty()) {
        audioDevice = new ma_device();
        AudioContext* ctx = new AudioContext{&audioPCM, &audioReadPos};

        ma_device_config devCfg = ma_device_config_init(ma_device_type_playback);
        devCfg.playback.format = ma_format_f32;
        devCfg.playback.channels = 2;
        devCfg.sampleRate = outSampleRate;
        devCfg.dataCallback = audioCallback2;
        devCfg.pUserData = ctx;

        if (ma_device_init(nullptr, &devCfg, audioDevice) != MA_SUCCESS) {
            std::cerr << "[VideoPlayer] miniaudio init failed" << std::endl;
            delete audioDevice;
            audioDevice = nullptr;
            delete ctx;
        } else {
            audioReadPos = 0;
            ma_device_start(audioDevice);
            std::cerr << "[VideoPlayer] Audio playback started" << std::endl;
        }
    }

    return true;
}

bool VideoPlayer::update(double deltaTime) {
    if (!loaded || endOfStream) return false;

    currentTime += deltaTime;
    if (currentTime >= duration) {
        endOfStream = true;
        return false;
    }

    while (nextFrameTime < currentTime) {
        if (!decodeNextVideoFrame()) {
            endOfStream = true;
            return false;
        }
        nextFrameTime = frame->pts * av_q2d(formatContext->streams[videoStreamIndex]->time_base);
        uploadFrameToTexture();
    }

    return true;
}

void VideoPlayer::uploadFrameToTexture() {
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, videoBuffer);
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace our

#endif // USE_FFMPEG
