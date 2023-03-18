#pragma once

extern "C" unsigned long AudioDecodeMP3(char* InMP3Bytes, int InLength, char* &OutputBytes, float &Duration, int &NumOfChannels, int &SampleRate, bool s16=true);