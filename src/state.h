
#ifndef __STATE_H
#define __STATE_H

#include "phoneme.h"

/* A structure holding what was all global state */
struct SamState {
    //standard sam sound
    unsigned char speed; // = 72;
    unsigned char pitch; // = 64;
    unsigned char mouth; // = 128;
    unsigned char throat; // = 128;
    bool singmode;
    bool debug;

    // the sound output buffer
    int bufferpos;
    char *buffer;

    struct Phoneme phonemes[256];
    unsigned char phonemeIndexOutput[60];
    unsigned char stressOutput[60]; //tab47365
    unsigned char phonemeLengthOutput[60]; //tab47416

    // global tables from render.cc
    unsigned char pitches[256]; // tab43008

    unsigned char frequency1[256];
    unsigned char frequency2[256];
    unsigned char frequency3[256];

    unsigned char amplitude1[256];
    unsigned char amplitude2[256];
    unsigned char amplitude3[256];

    unsigned char sampledConsonantFlag[256]; // tab44800


    SamState();
};

#endif