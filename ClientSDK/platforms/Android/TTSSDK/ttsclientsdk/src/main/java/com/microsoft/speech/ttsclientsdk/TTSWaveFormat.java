package com.microsoft.speech.ttsclientsdk;

/**
 * Created by v-guoya on 5/25/2017.
 */

public class TTSWaveFormat {
    short     wFormatTag;         /* format type */
    short     nChannels;          /* number of channels (i.e. mono, stereo...) */
    int       nSamplesPerSec;     /* sample rate */
    int       nAvgBytesPerSec;    /* for buffer estimation */
    short     nBlockAlign;        /* block size of data */
    short     wBitsPerSample;     /* number of bits per sample of mono data */
    int       cbSize;             /* the count in bytes of the size of */
                                  /* extra information (after cbSize) */
}
