//
// Created by acb on 23/04/19.
//

#include "phoneme.h"

Phoneme::Phoneme() {
    index = 0;
    length = 0;
    stress = 0;
}

Phoneme::Phoneme(unsigned char i, unsigned char l, unsigned char s) {
    index = i;
    length = l;
    stress = s;
}
