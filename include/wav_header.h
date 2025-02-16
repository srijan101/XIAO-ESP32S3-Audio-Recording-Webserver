#ifndef WAV_HEADER_H
#define WAV_HEADER_H

struct WAVHeader {
    // RIFF chunk
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t chunkSize = 0;  // Will be updated later
    char wave[4] = {'W', 'A', 'V', 'E'};
    
    // fmt subchunk
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t subchunk1Size = 16;
    uint16_t audioFormat = 1;  // PCM = 1
    uint16_t numChannels = 1;  // Mono
    uint32_t sampleRate = 16000;
    uint32_t byteRate = 16000 * 2;  // SampleRate * NumChannels * BitsPerSample/8
    uint16_t blockAlign = 2;  // NumChannels * BitsPerSample/8
    uint16_t bitsPerSample = 16;
    
    // data subchunk
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t subchunk2Size = 0;  // Will be updated later
};

#endif 