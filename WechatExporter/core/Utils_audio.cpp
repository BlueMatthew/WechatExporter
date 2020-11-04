//
//  Utils_audio.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//  Refer: https://www.programmersought.com/article/2635152445/
//

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}
#include "Utils.h"

struct AutoAVContext
{
private:
    AVCodecContext *&m_audioCodecContext;
    AVFormatContext *&m_formatContext;
   
public:
    AutoAVContext(AVCodecContext *&audioCodecContext, AVFormatContext *&formatContext) : m_audioCodecContext(audioCodecContext), m_formatContext(formatContext)
    {
    }
    
    ~AutoAVContext()
    {
        if (m_formatContext != nullptr)
        {
            avformat_free_context(m_formatContext);
        }
        if (m_audioCodecContext != nullptr)
        {
            avcodec_free_context(&m_audioCodecContext);
        }
    }
};

bool pcmToMp3(const std::string& pcmPath, const std::string& mp3Path)
{
    return true;
}

bool pcmToMp3(const std::vector<unsigned char>& pcmData, const std::string& mp3Path)
{
#ifdef ENABLE_AUDIO_CONVERTION
    AVCodecContext *audioCodecContext = nullptr;
    AVFormatContext *formatContext = nullptr;
    
    AutoAVContext ctx(audioCodecContext, formatContext);
    
    // Find Codec
    AVCodecID codecID = AV_CODEC_ID_MP3;
    AVCodec *audioCodec = avcodec_find_encoder(codecID);
    if (audioCodec == nullptr)
        return false;

     // Create an encoder context
    audioCodecContext = avcodec_alloc_context3(audioCodec);
    if (audioCodecContext == nullptr)
        return false;

    // Setting parameters
    // audioCodecContext->bit_rate = 64000;
    audioCodecContext->bit_rate = 44100;
    // audioCodecContext->sample_rate = 44100;
    audioCodecContext->sample_rate = 24000;
    
    audioCodecContext->sample_fmt = AV_SAMPLE_FMT_S16P;
    
    // audioCodecContext->channels = 2;
    audioCodecContext->channels = 2;
    audioCodecContext->channel_layout = av_get_default_channel_layout(2);
    audioCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // Turn on the encoder
    int result = avcodec_open2(audioCodecContext, audioCodec, nullptr);
    if (result < 0)
        return false;

     // Create a package
    // Create an output Format context
    result = avformat_alloc_output_context2(&formatContext, nullptr, nullptr, mp3Path.c_str());
    if (result < 0)
        return false;

    AVOutputFormat *outputFormat = formatContext->oformat;

    // Create an audio stream
    AVStream *audioStream = avformat_new_stream(formatContext, audioCodec);
    if (audioStream == nullptr)
    {
        return false;
    }
    
    // Set the parameters in the stream
    audioStream->id = formatContext->nb_streams - 1;
    audioStream->time_base = { 1, 44100 };
    result = avcodec_parameters_from_context(audioStream->codecpar, audioCodecContext);
    if (result < 0)
    {
        return false;
    }

    // Print FormatContext information
    av_dump_format(formatContext, 0, mp3Path.c_str(), 1);

    // Open file IO
    if (!(outputFormat->flags & AVFMT_NOFILE))
    {
        result = avio_open(&formatContext->pb, mp3Path.c_str(), AVIO_FLAG_WRITE);
        if (result < 0)
        {
            return false;
        }
    }
    
     // write to the file header
    result = avformat_write_header(formatContext, nullptr);
    if (result < 0)
        return false; // goto end;
    
     // Create Frame
    AVFrame *frame = av_frame_alloc();
    if (frame == nullptr)
        return false; // goto end;

    int nb_samples = 0;
    if (audioCodecContext->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
        nb_samples = 10000;
    else
        nb_samples = audioCodecContext->frame_size;

    // Set the parameters of the Frame
    frame->nb_samples = nb_samples;
    frame->format = audioCodecContext->sample_fmt;
    frame->channel_layout = audioCodecContext->channel_layout;

    // Apply for data memory
    result = av_frame_get_buffer(frame, 0);
    if (result < 0)
    {
        av_frame_free(&frame);
        return false;
    }

    // Set the Frame to be writable
    result = av_frame_make_writable(frame);
    if (result < 0)
    {
        av_frame_free(&frame);
        return false; // goto end;
    }

    int perFrameDataSize = frame->linesize[0];
    /*
    if (formatType == AudioFormat_MP3)
        perFrameDataSize *= 2;
     */

    std::vector<unsigned char>::size_type count = (pcmData.size() + perFrameDataSize - 1) / perFrameDataSize;
    int frameCount = 0;
    int inputCount = frame->nb_samples;
    for (int i = 0; i < count; ++i)
    {
         // Create a Packet
        AVPacket *pkt = av_packet_alloc();
        if (pkt == nullptr)
            return false; // goto end;
        av_init_packet(pkt);

        // Set the data
        memset(frame->data[0], 0, frame->linesize[0]);
        memset(frame->data[1], 0, frame->linesize[0]);

         // Synthesize MP3 files
        // int64_t channelLayout = av_get_default_channel_layout(2);
        int64_t channelLayout = av_get_default_channel_layout(1);

         // Format conversion S16->S16P
        SwrContext *swrContext = swr_alloc_set_opts(nullptr, channelLayout, AV_SAMPLE_FMT_S16P, 44100, \
            channelLayout, AV_SAMPLE_FMT_S16, 44100, 0, nullptr);
        // SwrContext *swrContext = swr_alloc_set_opts(nullptr, channelLayout, AV_SAMPLE_FMT_S16P, 24000, \
            channelLayout, AV_SAMPLE_FMT_S16, 24000, 0, nullptr);
        swr_init(swrContext);

        unsigned char *pSrcData[1] = {0};
        pSrcData[0] = (unsigned char*)&(pcmData[perFrameDataSize * i]);

        if (i == count - 1)
        {
            inputCount = pcmData.size() % perFrameDataSize;
        }
        swr_convert(swrContext, frame->data, frame->nb_samples, (const uint8_t **)pSrcData, inputCount);

        swr_free(&swrContext);
        AVRational rational;
        rational.den = audioCodecContext->sample_rate;
        rational.num = 1;
        // frame->pts = av_rescale_q(0, rational, audioCodecContext->time_base);

        frame->pts = frameCount++;
         // send Frame
        result = avcodec_send_frame(audioCodecContext, frame);
        if (result < 0)
            continue;

         // Receive the encoded Packet
        result = avcodec_receive_packet(audioCodecContext, pkt);
        if (result < 0)
        {
            av_packet_free(&pkt);
            continue;
        }

         // write to file
        av_packet_rescale_ts(pkt, audioCodecContext->time_base, formatContext->streams[0]->time_base);
        pkt->stream_index = 0;
        result = av_interleaved_write_frame(formatContext, pkt);
        if (result < 0)
            continue;

        av_packet_free(&pkt);
    }

     // write to the end of the file
    av_write_trailer(formatContext);
     // Close file IO
    avio_closep(&formatContext->pb);
    // Release Frame memory
    av_frame_free(&frame);
#endif // ENABLE_AUDIO_CONVERTION
    
    return true;
}
