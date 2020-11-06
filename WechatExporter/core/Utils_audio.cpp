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
#include <lame/lame.h>
}
#include "Utils.h"

bool pcmToMp3(const std::string& pcmPath, const std::string& mp3Path)
{
	std::vector<unsigned char> pcmData;
	if (!readFile(pcmPath, pcmData))
	{
		return false;
	}

    return pcmToMp3(pcmData, mp3Path);
}

bool pcmToMp3(const std::vector<unsigned char>& pcmData, const std::string& mp3Path)
{
#ifdef ENABLE_AUDIO_CONVERTION
    const int MP3_SIZE = 4096;
    const int num_of_channels = 1;

    lame_global_flags *gfp = NULL;
    gfp = lame_init();
    if (NULL == gfp)
    {
        return false;
    }

    lame_set_in_samplerate(gfp, 24000);
    lame_set_preset(gfp, 56);
    lame_set_mode(gfp, MONO);
    // RG is enabled by default
    lame_set_findReplayGain(gfp, 1);
    //Setting Channels
    lame_set_num_channels(gfp, num_of_channels);

    unsigned long fsize = (unsigned long) (pcmData.size() / (2 * num_of_channels));
    lame_set_num_samples(gfp, fsize);
    
    int samples_to_read = lame_get_framesize(gfp);
    int samples_of_channel = 576;
    samples_to_read = samples_of_channel * num_of_channels;    //
    // int framesize = samples_to_read;
    // std::assert(framesize <= 1152);
    // int bytes_per_sample = sizeof(short int);
    
    lame_set_out_samplerate(gfp, 24000);

    if(lame_init_params(gfp) == -1)
    {
        //lame initialization failed
        lame_close(gfp);
        return false;
    }
    
    std::vector<short int> pcm_buffer;
    pcm_buffer.resize(samples_to_read, 0);
    int bytesOfPcmBuffer = static_cast<int>(pcm_buffer.size() * sizeof(short int));
    unsigned char mp3_buffer[MP3_SIZE];
    int count = static_cast<int>((pcmData.size() + bytesOfPcmBuffer - 1) / bytesOfPcmBuffer);
    
    FILE *mp3 = fopen(mp3Path.c_str(), "wb");
    if(mp3 == NULL)
    {
        lame_close(gfp);
        return false;
    }
    for (int idx = 0; idx < count; ++idx)
    {
        if (idx == (count - 1) && (pcmData.size() % bytesOfPcmBuffer) != 0)
        {
            pcm_buffer.assign(pcm_buffer.size(), 0);
            samples_to_read = ((pcmData.size() % bytesOfPcmBuffer) + sizeof(short int) - 1) / sizeof(short int);
        }
        memcpy(reinterpret_cast<void *>(&(pcm_buffer[0])), reinterpret_cast<const void *>(&(pcmData[idx * bytesOfPcmBuffer])), samples_to_read * sizeof(short int));
        
        int write = lame_encode_buffer(gfp, &pcm_buffer[0], NULL, samples_of_channel, mp3_buffer, MP3_SIZE);
        fwrite(mp3_buffer, write, sizeof(char), mp3);
    }

    fclose(mp3);
    lame_close(gfp);

#endif // ENABLE_AUDIO_CONVERTION
    
    return true;
}
