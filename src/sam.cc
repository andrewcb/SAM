#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "debug.h"
#include "sam.h"
#include "render.h"
#include "SamTabs.h"
#include "state.h"

enum {
    pR    = 23,
    pD    = 57,
    pT    = 69,
    BREAK = 254,
    END   = 255
};

unsigned char *parser1(struct SamState &state, unsigned char *input);
void parser2(struct SamState &state);
void copyStress(struct SamState &state);
void setPhonemeLength(struct SamState &state);
void adjustLengths(struct SamState &state);
void code41240(struct SamState &state);
void insert(struct SamState &state, unsigned char position, unsigned char mem60, unsigned char mem59,
            unsigned char mem58);
void insertBreath(struct SamState &state);
void prepareOutput(struct SamState &state);
void setMouthThroat(unsigned char mouth, unsigned char throat);


int SAMMain(unsigned char *input, struct SamState &state) {
	unsigned char x = 0; //!! is this intended like this?
    setMouthThroat(state.mouth, state.throat);

	if (parser1(state, input) == 0) return 0;
	if (state.debug) PrintPhonemes(state.phonemeindex, state.phonemeLength, state.stress);
    parser2(state);
    copyStress(state);
    setPhonemeLength(state);
    adjustLengths(state);
    code41240(state);
	do {
		if (state.phonemeindex[x] > 80) {
			state.phonemeindex[x] = END;
			break; // error: delete all behind it
		}
	} while (++x != 0);
    insertBreath(state);

	if (state.debug) PrintPhonemes(state.phonemeindex, state.phonemeLength, state.stress);

    prepareOutput(state);
	return 1;
}

void prepareOutput(struct SamState &state) {
	unsigned char srcpos  = 0; // Position in source
	unsigned char destpos = 0; // Position in output

	while(1) {
		unsigned char A = state.phonemeindex[srcpos];
        state.phonemeIndexOutput[destpos] = A;
        switch(A) {
        case END:
			Render(state);
			return;
		case BREAK:
			state.phonemeIndexOutput[destpos] = END;
			Render(state);
			destpos = 0;
            break;
        case 0:
            break;
        default:
            state.phonemeLengthOutput[destpos] = state.phonemeLength[srcpos];
            state.stressOutput[destpos]        = state.stress[srcpos];
            ++destpos;
            break;
        }
		++srcpos;
	}
}


void insertBreath(struct SamState &state) {
	unsigned char mem54 = 255;
	unsigned char len = 0;
	unsigned char index; //variable Y

	unsigned char pos = 0;

	while((index = state.phonemeindex[pos]) != END) {
		len += state.phonemeLength[pos];
		if (len < 232) {
			if (index == BREAK) {
            } else if (!(flags[index] & FLAG_PUNCT)) {
                if (index == 0) mem54 = pos;
            } else {
                len = 0;
                insert(state, ++pos, BREAK, 0, 0);
            }
		} else {
            pos = mem54;
            state.phonemeindex[pos]  = 31;   // 'Q*' glottal stop
            state.phonemeLength[pos] = 4;
            state.stress[pos] = 0;

            len = 0;
            insert(state, ++pos, BREAK, 0, 0);
        }
        ++pos;
	}
}


// Iterates through the phoneme buffer, copying the stress value from
// the following phoneme under the following circumstance:
       
//     1. The current phoneme is voiced, excluding plosives and fricatives
//     2. The following phoneme is voiced, excluding plosives and fricatives, and
//     3. The following phoneme is stressed
//
//  In those cases, the stress value+1 from the following phoneme is copied.
//
// For example, the word LOITER is represented as LOY5TER, with as stress
// of 5 on the dipthong OY. This routine will copy the stress value of 6 (5+1)
// to the L that precedes it.



void copyStress(struct SamState &state) {
    // loop thought all the phonemes to be output
	unsigned char pos=0; //mem66
    unsigned char Y;
	while((Y = state.phonemeindex[pos]) != END) {
		// if CONSONANT_FLAG set, skip - only vowels get stress
		if (flags[Y] & 64) {
            Y = state.phonemeindex[pos+1];

            // if the following phoneme is the end, or a vowel, skip
            if (Y != END && (flags[Y] & 128) != 0) {
                // get the stress value at the next position
                Y = state.stress[pos+1];
                if (Y && !(Y&128)) {
                    // if next phoneme is stressed, and a VOWEL OR ER
                    // copy stress from next phoneme to this one
                    state.stress[pos] = Y+1;
                }
            }
        }

		++pos;
	}
}

void insert(struct SamState &state, unsigned char position/*var57*/, unsigned char mem60, unsigned char mem59,
            unsigned char mem58)
{
	int i;
	for(i=253; i >= position; i--) // ML : always keep last safe-guarding 255	
	{
        state.phonemeindex[i+1]  = state.phonemeindex[i];
        state.phonemeLength[i+1] = state.phonemeLength[i];
        state.stress[i+1]        = state.stress[i];
	}

    state.phonemeindex[position]  = mem60;
    state.phonemeLength[position] = mem59;
    state.stress[position]        = mem58;
}


signed int full_match(unsigned char sign1, unsigned char sign2) {
    unsigned char Y = 0;
    do {
        // GET FIRST CHARACTER AT POSITION Y IN signInputTable
        // --> should change name to PhonemeNameTable1
        unsigned char A = signInputTable1[Y];
        
        if (A == sign1) {
            A = signInputTable2[Y];
            // NOT A SPECIAL AND MATCHES SECOND CHARACTER?
            if ((A != '*') && (A == sign2)) return Y;
        }
    } while (++Y != 81);
    return -1;
}

signed int wild_match(unsigned char sign1) {
    signed int Y = 0;
    do {
		if (signInputTable2[Y] == '*') {
			if (signInputTable1[Y] == sign1) return Y;
		}
	} while (++Y != 81);
    return -1;
}



// The input[] buffer contains a string of phonemes and stress markers along
// the lines of:
//
//     DHAX KAET IHZ AH5GLIY. <0x9B>
//
// The byte 0x9B marks the end of the buffer. Some phonemes are 2 bytes 
// long, such as "DH" and "AX". Others are 1 byte long, such as "T" and "Z". 
// There are also stress markers, such as "5" and ".".
//
// The first character of the phonemes are stored in the table signInputTable1[].
// The second character of the phonemes are stored in the table signInputTable2[].
// The stress characters are arranged in low to high stress order in stressInputTable[].
// 
// The following process is used to parse the input[] buffer:
// 
// Repeat until the <0x9B> character is reached:
//
//        First, a search is made for a 2 character match for phonemes that do not
//        end with the '*' (wildcard) character. On a match, the index of the phoneme 
//        is added to phonemeIndex[] and the buffer position is advanced 2 bytes.
//
//        If this fails, a search is made for a 1 character match against all
//        phoneme names ending with a '*' (wildcard). If this succeeds, the 
//        phoneme is added to phonemeIndex[] and the buffer position is advanced
//        1 byte.
// 
//        If this fails, search for a 1 character match in the stressInputTable[].
//        If this succeeds, the stress value is placed in the last stress[] table
//        at the same index of the last added phoneme, and the buffer position is
//        advanced by 1 byte.
//
//        If this fails, return a 0.
//
// On success:
//
//    1. phonemeIndex[] will contain the index of all the phonemes.
//    2. The last index in phonemeIndex[] will be 255.
//    3. stress[] will contain the stress value for each phoneme

// input[] holds the string of phonemes, each two bytes wide
// signInputTable1[] holds the first character of each phoneme
// signInputTable2[] holds te second character of each phoneme
// phonemeIndex[] holds the indexes of the phonemes after parsing input[]
//
// The parser scans through the input[], finding the names of the phonemes
// by searching signInputTable1[] and signInputTable2[]. On a match, it
// copies the index of the phoneme into the phonemeIndexTable[].
//
// The character <0x9B> marks the end of text in input[]. When it is reached,
// the index 255 is placed at the end of the phonemeIndexTable[], and the
// function returns with a 1 indicating success.
unsigned char *parser1(struct SamState &state, unsigned char *input)
{
	unsigned char sign1;
	unsigned char position = 0;
	unsigned char srcpos   = 0;

	memset(state.stress, 0, 256); // Clear the stress table.

	while((sign1 = input[srcpos]) != 155) { // 155 (\233) is end of line marker
		signed int match;
		unsigned char sign2 = input[++srcpos];
        if ((match = full_match(sign1, sign2)) != -1) {
            // Matched both characters (no wildcards)
            state.phonemeindex[position++] = (unsigned char)match;
            ++srcpos; // Skip the second character of the input as we've matched it
        } else if ((match = wild_match(sign1)) != -1) {
            // Matched just the first character (with second character matching '*'
            state.phonemeindex[position++] = (unsigned char)match;
        } else {
            // Should be a stress character. Search through the
            // stress table backwards.
            match = 8; // End of stress table. FIXME: Don't hardcode.
            while( (sign1 != stressInputTable[match]) && (match>0) ) --match;
            
            if (match == 0) return 0; // failure

            state.stress[position-1] = (unsigned char)match; // Set stress for prior phoneme
        }
	} //while

    state.phonemeindex[position] = END;
    return state.phonemeindex;
}


//change phonemelength depedendent on stress
void setPhonemeLength(struct SamState &state) {
	int position = 0;
	while(state.phonemeindex[position] != 255) {
		unsigned char A = state.stress[position];
		if ((A == 0) || ((A&128) != 0)) {
            state.phonemeLength[position] = phonemeLengthTable[state.phonemeindex[position]];
		} else {
            state.phonemeLength[position] = phonemeStressedLengthTable[state.phonemeindex[position]];
		}
		position++;
	}
}

void code41240(struct SamState &state) {
	unsigned char pos=0;

	while(state.phonemeindex[pos] != END) {
		unsigned char index = state.phonemeindex[pos];

		if ((flags[index] & FLAG_STOPCONS)) {
            if ((flags[index] & FLAG_PLOSIVE)) {
                unsigned char A;
                unsigned char X = pos;
                while(!state.phonemeindex[++X]); /* Skip pause */
                A = state.phonemeindex[X];
                if (A != END) {
                    if ((flags[A] & 8) || (A == 36) || (A == 37)) {++pos; continue;} // '/H' '/X'
                }
                
            }
            insert(state, pos + 1, index + 1, phonemeLengthTable[index + 1], state.stress[pos]);
            insert(state, pos + 2, index + 2, phonemeLengthTable[index + 2], state.stress[pos]);
            pos += 2;
        }
        ++pos;
	}
}


void ChangeRule(struct SamState &state, unsigned char position, unsigned char mem60, const char * descr)
{
    if (state.debug) printf("RULE: %s\n",descr);
    state.phonemeindex[position] = 13; //rule;
    insert(state, position + 1, mem60, 0, state.stress[position]);
}

void drule(struct SamState &state, const char * str) {
    if (state.debug) printf("RULE: %s\n",str);
}

void drule_pre(struct SamState &state,const char *descr, unsigned char X) {
    drule(state, descr);
    if (state.debug) {
        printf("PRE\n");
        printf("phoneme %d (%c%c) length %d\n", X, signInputTable1[state.phonemeindex[X]], signInputTable2[state.phonemeindex[X]], state.phonemeLength[X]);
    }
}

void drule_post(struct SamState &state,unsigned char X) {
    if (state.debug) {
        printf("POST\n");
        printf("phoneme %d (%c%c) length %d\n", X, signInputTable1[state.phonemeindex[X]], signInputTable2[state.phonemeindex[X]], state.phonemeLength[X]);
    }
}


// Rewrites the phonemes using the following rules:
//
//       <DIPTHONG ENDING WITH WX> -> <DIPTHONG ENDING WITH WX> WX
//       <DIPTHONG NOT ENDING WITH WX> -> <DIPTHONG NOT ENDING WITH WX> YX
//       UL -> AX L
//       UM -> AX M
//       <STRESSED VOWEL> <SILENCE> <STRESSED VOWEL> -> <STRESSED VOWEL> <SILENCE> Q <VOWEL>
//       T R -> CH R
//       D R -> J R
//       <VOWEL> R -> <VOWEL> RX
//       <VOWEL> L -> <VOWEL> LX
//       G S -> G Z
//       K <VOWEL OR DIPTHONG NOT ENDING WITH IY> -> KX <VOWEL OR DIPTHONG NOT ENDING WITH IY>
//       G <VOWEL OR DIPTHONG NOT ENDING WITH IY> -> GX <VOWEL OR DIPTHONG NOT ENDING WITH IY>
//       S P -> S B
//       S T -> S D
//       S K -> S G
//       S KX -> S GX
//       <ALVEOLAR> UW -> <ALVEOLAR> UX
//       CH -> CH CH' (CH requires two phonemes to represent it)
//       J -> J J' (J requires two phonemes to represent it)
//       <UNSTRESSED VOWEL> T <PAUSE> -> <UNSTRESSED VOWEL> DX <PAUSE>
//       <UNSTRESSED VOWEL> D <PAUSE>  -> <UNSTRESSED VOWEL> DX <PAUSE>


void rule_alveolar_uw(struct SamState &state, unsigned char X) {
    // ALVEOLAR flag set?
    if (flags[state.phonemeindex[X-1]] & FLAG_ALVEOLAR) {
        drule(state, "<ALVEOLAR> UW -> <ALVEOLAR> UX");
        state.phonemeindex[X] = 16;
    }
}

void rule_ch(struct SamState &state, unsigned char X) {
    drule(state, "CH -> CH CH+1");
    insert(state, X + 1, 43, 0, state.stress[X]);
}

void rule_j(struct SamState &state, unsigned char X) {
    drule(state, "J -> J J+1");
    insert(state, X + 1, 45, 0, state.stress[X]);
}

void rule_g(struct SamState &state, unsigned char pos) {
    // G <VOWEL OR DIPTHONG NOT ENDING WITH IY> -> GX <VOWEL OR DIPTHONG NOT ENDING WITH IY>
    // Example: GO

    unsigned char index = state.phonemeindex[pos+1];
            
    // If dipthong ending with YX, move continue processing next phoneme
    if ((index != 255) && ((flags[index] & FLAG_DIP_YX) == 0)) {
        // replace G with GX and continue processing next phoneme
        drule(state, "G <VOWEL OR DIPTHONG NOT ENDING WITH IY> -> GX <VOWEL OR DIPTHONG NOT ENDING WITH IY>");
        state.phonemeindex[pos] = 63; // 'GX'
    }
}


void change(struct SamState &state, unsigned char pos, unsigned char val, const char * rule) {
    drule(state, rule);
    state.phonemeindex[pos] = val;
}


void rule_dipthong(struct SamState &state, unsigned char p, unsigned short pf, unsigned char pos) {
    // <DIPTHONG ENDING WITH WX> -> <DIPTHONG ENDING WITH WX> WX
    // <DIPTHONG NOT ENDING WITH WX> -> <DIPTHONG NOT ENDING WITH WX> YX
    // Example: OIL, COW

    // If ends with IY, use YX, else use WX
    unsigned char A = (pf & FLAG_DIP_YX) ? 21 : 20; // 'WX' = 20 'YX' = 21
                
    // insert at WX or YX following, copying the stress
    if (A==20) drule(state, "insert WX following dipthong NOT ending in IY sound");
    else if (A==21) drule(state, "insert YX following dipthong ending in IY sound");
    insert(state, pos + 1, A, 0, state.stress[pos]);
                
    if (p == 53) rule_alveolar_uw(state, pos); // Example: NEW, DEW, SUE, ZOO, THOO, TOO
    else if (p == 42) rule_ch(state, pos);     // Example: CHEW
    else if (p == 44) rule_j(state, pos);      // Example: JAY
}

void parser2(struct SamState &state) {
	unsigned char pos = 0; //mem66;
    unsigned char p;

	if (state.debug) printf("parser2\n");

	while((p = state.phonemeindex[pos]) != END) {
		unsigned short pf;
		unsigned char prior;

		if (state.debug) printf("%d: %c%c\n", pos, signInputTable1[p], signInputTable2[p]);

		if (p == 0) { // Is phoneme pause?
			++pos;
			continue;
		}

        pf = flags[p];
        prior = state.phonemeindex[pos-1];

        if ((pf & FLAG_DIPTHONG)) rule_dipthong(state, p, pf, pos);
        else if (p == 78) ChangeRule(state, pos, 24, "UL -> AX L"); // Example: MEDDLE
        else if (p == 79) ChangeRule(state, pos, 27, "UM -> AX M"); // Example: ASTRONOMY
        else if (p == 80) ChangeRule(state, pos, 28, "UN -> AX N"); // Example: FUNCTION
        else if ((pf & FLAG_VOWEL) && state.stress[pos]) {
            // RULE:
            //       <STRESSED VOWEL> <SILENCE> <STRESSED VOWEL> -> <STRESSED VOWEL> <SILENCE> Q <VOWEL>
            // EXAMPLE: AWAY EIGHT
            if (!state.phonemeindex[pos+1]) { // If following phoneme is a pause, get next
                p = state.phonemeindex[pos+2];
                if (p!=END && (flags[p] & FLAG_VOWEL) && state.stress[pos+2]) {
                    drule(state, "insert glottal stop between two stressed vowels with space between them");
                    insert(state, pos + 2, 31, 0, 0); // 31 = 'Q'
                }
            }
        } else if (p == pR) { // RULES FOR PHONEMES BEFORE R
            if (prior == pT) change(state, pos-1,42, "T R -> CH R"); // Example: TRACK
            else if (prior == pD) change(state, pos-1,44, "D R -> J R"); // Example: DRY
            else if (flags[prior] & FLAG_VOWEL) change(state, pos, 18, "<VOWEL> R -> <VOWEL> RX"); // Example: ART
        } else if (p == 24 && (flags[prior] & FLAG_VOWEL)) change(state, pos, 19, "<VOWEL> L -> <VOWEL> LX"); // Example: ALL
        else if (prior == 60 && p == 32) { // 'G' 'S'
            // Can't get to fire -
            //       1. The G -> GX rule intervenes
            //       2. Reciter already replaces GS -> GZ
            change(state, pos, 38, "G S -> G Z");
        } else if (p == 60) rule_g(state, pos);
		else {
            if (p == 72) {  // 'K'
                // K <VOWEL OR DIPTHONG NOT ENDING WITH IY> -> KX <VOWEL OR DIPTHONG NOT ENDING WITH IY>
                // Example: COW
                unsigned char Y = state.phonemeindex[pos+1];
                // If at end, replace current phoneme with KX
                if ((flags[Y] & FLAG_DIP_YX)==0 || Y==END) { // VOWELS AND DIPTHONGS ENDING WITH IY SOUND flag set?
                    change(state, pos, 75, "K <VOWEL OR DIPTHONG NOT ENDING WITH IY> -> KX <VOWEL OR DIPTHONG NOT ENDING WITH IY>");
                    p  = 75;
                    pf = flags[p];
                }
            }

            // Replace with softer version?
            if ((flags[p] & FLAG_PLOSIVE) && (prior == 32)) { // 'S'
                // RULE:
                //      S P -> S B
                //      S T -> S D
                //      S K -> S G
                //      S KX -> S GX
                // Examples: SPY, STY, SKY, SCOWL
                
                if (state.debug) printf("RULE: S* %c%c -> S* %c%c\n", signInputTable1[p], signInputTable2[p],signInputTable1[p-12], signInputTable2[p-12]);
                state.phonemeindex[pos] = p-12;
            } else if (!(pf & FLAG_PLOSIVE)) {
                p = state.phonemeindex[pos];
                if (p == 53) rule_alveolar_uw(state, pos);   // Example: NEW, DEW, SUE, ZOO, THOO, TOO
                else if (p == 42) rule_ch(state, pos); // Example: CHEW
                else if (p == 44) rule_j(state, pos);  // Example: JAY
            }
            
            if (p == 69 || p == 57) { // 'T', 'D'
                // RULE: Soften T following vowel
                // NOTE: This rule fails for cases such as "ODD"
                //       <UNSTRESSED VOWEL> T <PAUSE> -> <UNSTRESSED VOWEL> DX <PAUSE>
                //       <UNSTRESSED VOWEL> D <PAUSE>  -> <UNSTRESSED VOWEL> DX <PAUSE>
                // Example: PARTY, TARDY
                if (flags[state.phonemeindex[pos-1]] & FLAG_VOWEL) {
                    p = state.phonemeindex[pos+1];
                    if (!p) p = state.phonemeindex[pos+2];
                    if ((flags[p] & FLAG_VOWEL) && !state.stress[pos+1]) change(state, pos,30, "Soften T or D following vowel or ER and preceding a pause -> DX");
                }
            }
        }
        pos++;
	} // while
}

// Applies various rules that adjust the lengths of phonemes
//
//         Lengthen <FRICATIVE> or <VOICED> between <VOWEL> and <PUNCTUATION> by 1.5
//         <VOWEL> <RX | LX> <CONSONANT> - decrease <VOWEL> length by 1
//         <VOWEL> <UNVOICED PLOSIVE> - decrease vowel by 1/8th
//         <VOWEL> <UNVOICED CONSONANT> - increase vowel by 1/2 + 1
//         <NASAL> <STOP CONSONANT> - set nasal = 5, consonant = 6
//         <VOICED STOP CONSONANT> {optional silence} <STOP CONSONANT> - shorten both to 1/2 + 1
//         <LIQUID CONSONANT> <DIPTHONG> - decrease by 2
//
void adjustLengths(struct SamState &state) {
    // LENGTHEN VOWELS PRECEDING PUNCTUATION
    //
    // Search for punctuation. If found, back up to the first vowel, then
    // process all phonemes between there and up to (but not including) the punctuation.
    // If any phoneme is found that is a either a fricative or voiced, the duration is
    // increased by (length * 1.5) + 1

    // loop index
	{
	unsigned char X = 0;
	unsigned char index;

	while((index = state.phonemeindex[X]) != END) {
		unsigned char loopIndex;

		// not punctuation?
		if((flags[index] & FLAG_PUNCT) == 0) {
			++X;
			continue;
		}

		loopIndex = X;

        while (--X && !(flags[state.phonemeindex[X]] & FLAG_VOWEL)); // back up while not a vowel
        if (X == 0) break;

		do {
            // test for vowel
			index = state.phonemeindex[X];

			// test for fricative/unvoiced or not voiced
			if(!(flags[index] & FLAG_FRICATIVE) || (flags[index] & FLAG_VOICED)) {     //nochmal �berpr�fen
				unsigned char A = state.phonemeLength[X];
				// change phoneme length to (length * 1.5) + 1
                drule_pre(state, "Lengthen <FRICATIVE> or <VOICED> between <VOWEL> and <PUNCTUATION> by 1.5",X);
				state.phonemeLength[X] = (A >> 1) + A + 1;
                drule_post(state, X);
			}
		} while (++X != loopIndex);
		X++;
	}  // while
	}

    // Similar to the above routine, but shorten vowels under some circumstances

    // Loop through all phonemes
	unsigned char loopIndex=0;
	unsigned char index;

	while((index = state.phonemeindex[loopIndex]) != END) {
		unsigned char X = loopIndex;

		if (flags[index] & FLAG_VOWEL) {
			index = state.phonemeindex[loopIndex+1];
			if (!(flags[index] & FLAG_CONSONANT)) {
				if ((index == 18) || (index == 19)) { // 'RX', 'LX'
					index = state.phonemeindex[loopIndex+2];
					if ((flags[index] & FLAG_CONSONANT)) {
                        drule_pre(state, "<VOWEL> <RX | LX> <CONSONANT> - decrease length of vowel by 1\n", loopIndex);
                        state.phonemeLength[loopIndex]--;
                        drule_post(state, loopIndex);
                    }
				}
			} else { // Got here if not <VOWEL>
                unsigned short flag = (index == END) ? 65 : flags[index]; // 65 if end marker

                if (!(flag & FLAG_VOICED)) { // Unvoiced
                    // *, .*, ?*, ,*, -*, DX, S*, SH, F*, TH, /H, /X, CH, P*, T*, K*, KX
                    if((flag & FLAG_PLOSIVE)) { // unvoiced plosive
                        // RULE: <VOWEL> <UNVOICED PLOSIVE>
                        // <VOWEL> <P*, T*, K*, KX>
                        drule_pre(state, "<VOWEL> <UNVOICED PLOSIVE> - decrease vowel by 1/8th",loopIndex);
                        state.phonemeLength[loopIndex] -= (state.phonemeLength[loopIndex] >> 3);
                        drule_post(state, loopIndex);
                    }
                } else {
                    unsigned char A;
                    drule_pre(state, "<VOWEL> <VOICED CONSONANT> - increase vowel by 1/2 + 1\n",X-1);
                    // decrease length
                    A = state.phonemeLength[loopIndex];
                    state.phonemeLength[loopIndex] = (A >> 2) + A + 1;     // 5/4*A + 1
                    drule_post(state, loopIndex);
                }
            }
		} else if((flags[index] & FLAG_NASAL) != 0) { // nasal?
            // RULE: <NASAL> <STOP CONSONANT>
            //       Set punctuation length to 6
            //       Set stop consonant length to 5
            index = state.phonemeindex[++X];
            if (index != END && (flags[index] & FLAG_STOPCONS)) {
                drule(state, "<NASAL> <STOP CONSONANT> - set nasal = 5, consonant = 6");
                state.phonemeLength[X]   = 6; // set stop consonant length to 6
                state.phonemeLength[X-1] = 5; // set nasal length to 5
            }
        } else if((flags[index] & FLAG_STOPCONS)) { // (voiced) stop consonant?
            // RULE: <VOICED STOP CONSONANT> {optional silence} <STOP CONSONANT>
            //       Shorten both to (length/2 + 1)

            // move past silence
            while ((index = state.phonemeindex[++X]) == 0);

            if (index != END && (flags[index] & FLAG_STOPCONS)) {
                // FIXME, this looks wrong?
                // RULE: <UNVOICED STOP CONSONANT> {optional silence} <STOP CONSONANT>
                drule(state, "<UNVOICED STOP CONSONANT> {optional silence} <STOP CONSONANT> - shorten both to 1/2 + 1");
                state.phonemeLength[X]         = (state.phonemeLength[X] >> 1) + 1;
                state.phonemeLength[loopIndex] = (state.phonemeLength[loopIndex] >> 1) + 1;
                X = loopIndex;
            }
        } else if ((flags[index] & FLAG_LIQUIC)) { // liquic consonant?
            // RULE: <VOICED NON-VOWEL> <DIPTHONG>
            //       Decrease <DIPTHONG> by 2
            index = state.phonemeindex[X-1]; // prior phoneme;

            // FIXME: The debug code here breaks the rule.
            // prior phoneme a stop consonant>
            if((flags[index] & FLAG_STOPCONS) != 0) 
                drule_pre(state, "<LIQUID CONSONANT> <DIPTHONG> - decrease by 2",X);

            state.phonemeLength[X] -= 2; // 20ms
            drule_post(state, X);
         }

        ++loopIndex;
    }
}
