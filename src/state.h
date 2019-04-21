
#ifndef __STATE_H
#define __STATE_H

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

    // just copied from globals for now...
    unsigned char stress[256]; //numbers from 0 to 8
    unsigned char phonemeLength[256];
    unsigned char phonemeindex[256];
    unsigned char phonemeIndexOutput[60];
    unsigned char stressOutput[60]; //tab47365
    unsigned char phonemeLengthOutput[60]; //tab47416


    SamState();
};

#endif