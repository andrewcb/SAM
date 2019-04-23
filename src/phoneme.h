//
// Created by acb on 23/04/19.
//

#ifndef SAM_PHONEME_H
#define SAM_PHONEME_H

// The unified phoneme record, which was previously spread over phonemeIndex, phonemeLength and stress
struct Phoneme {
    unsigned char index;
    unsigned char length;
    unsigned char stress;

    Phoneme();
    Phoneme(unsigned char index, unsigned char length, unsigned char stress);
};

#endif //SAM_PHONEME_H
