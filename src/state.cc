//
// Created by acb on 20/04/19.
//

#include <stdlib.h>
#include "state.h"

SamState::SamState() {
    int i;

    speed = 72;
    pitch = 64;
    mouth = 128;
    throat = 128;
    singmode = false;

    debug = false;

    bufferpos = 0;
    // TODO, check for free the memory, 10 seconds of output should be more than enough
    buffer = (char *)malloc(22050*10);

    for(i=0; i<256; i++) {
        phonemes[i] = Phoneme();
    }

    for(i=0; i<60; i++) {
        phonemeIndexOutput[i] = 0;
        stressOutput[i] = 0;
        phonemeLengthOutput[i] = 0;
    }
    phonemes[255].index = 32; //to prevent buffer overflow
    // should this be 32 or 255? the code had it at 32, though a comment suggested 255 would prevent freezing with long inputs
}
