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

struct Phoneme *parser1(struct SamState &state, unsigned char *input);
void parser2(struct SamState &state);
void copyStress(struct SamState &state);
void setPhonemeLength(struct SamState &state);
void adjustLengths(struct SamState &state);
void code41240(struct SamState &state);
void insert(struct SamState &state, unsigned char position, unsigned char index, unsigned char length,
            unsigned char stress);
void insertBreath(struct SamState &state);
void prepareOutput(struct SamState &state);
void setMouthThroat(unsigned char mouth, unsigned char throat);


int SAMMain(unsigned char *input, struct SamState &state) {
	unsigned char x = 0; //!! is this intended like this?
    setMouthThroat(state.mouth, state.throat);

	if (parser1(state, input) == 0) return 0;
//	if (state.debug) PrintPhonemes(state.phonemeindex, state.phonemeLength, state.stress);
    parser2(state);
    copyStress(state);
    setPhonemeLength(state);
    adjustLengths(state);
    code41240(state);
	do {
		if (state.phonemes[x].index > 80) {
			state.phonemes[x].index = END;
			break; // error: delete all behind it
		}
	} while (++x != 0);
    insertBreath(state);

//	if (state.debug) PrintPhonemes(state.phonemeindex, state.phonemeLength, state.stress);

    prepareOutput(state);
	return 1;
}

void prepareOutput(struct SamState &state) {
	unsigned char srcpos  = 0; // Position in source
	unsigned char destpos = 0; // Position in output

	while(1) {
		unsigned char A = state.phonemes[srcpos].index;
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
            state.phonemeLengthOutput[destpos] = state.phonemes[srcpos].length;
            state.stressOutput[destpos]        = state.phonemes[srcpos].stress;
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

	while((index = state.phonemes[pos].index) != END) {
		len += state.phonemes[pos].length;
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
            state.phonemes[pos] = Phoneme(31, 4, 0); // 'Q*' glottal stop, length 4
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
	while((Y = state.phonemes[pos].index) != END) {
		// if CONSONANT_FLAG set, skip - only vowels get stress
		if (flags[Y] & 64) {
            Y = state.phonemes[pos+1].index;

            // if the following phoneme is the end, or a vowel, skip
            if (Y != END && (flags[Y] & 128) != 0) {
                // get the stress value at the next position
                Y = state.phonemes[pos+1].stress;
                if (Y && !(Y&128)) {
                    // if next phoneme is stressed, and a VOWEL OR ER
                    // copy stress from next phoneme to this one
                    state.phonemes[pos].stress = Y+1;
                }
            }
        }

		++pos;
	}
}

void insert(struct SamState &state, unsigned char position/*var57*/, unsigned char index, unsigned char length,
            unsigned char stress)
{
	int i;
	for(i=253; i >= position; i--) // ML : always keep last safe-guarding 255	
	{
	    state.phonemes[i+1] = state.phonemes[i];
	}

	state.phonemes[position] = Phoneme(index, length, stress);
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
struct Phoneme *parser1(struct SamState &state, unsigned char *input)
{
	unsigned char sign1;
	unsigned char position = 0;
	unsigned char srcpos   = 0;

	while((sign1 = input[srcpos]) != 155) { // 155 (\233) is end of line marker
		signed int match;
		unsigned char sign2 = input[++srcpos];
        if ((match = full_match(sign1, sign2)) != -1) {
            // Matched both characters (no wildcards)
            state.phonemes[position++] = Phoneme((unsigned char)match, 0, 0);
            ++srcpos; // Skip the second character of the input as we've matched it
        } else if ((match = wild_match(sign1)) != -1) {
            // Matched just the first character (with second character matching '*'
            state.phonemes[position++] = Phoneme((unsigned char)match, 0, 0);
        } else {
            // Should be a stress character. Search through the
            // stress table backwards.
            match = 8; // End of stress table. FIXME: Don't hardcode.
            while( (sign1 != stressInputTable[match]) && (match>0) ) --match;
            
            if (match == 0) return 0; // failure

            state.phonemes[position-1].stress = (unsigned char)match; // Set stress for prior phoneme
        }
	} //while

    state.phonemes[position] = Phoneme(END, 0, 0);
    return state.phonemes;
}


//change phonemelength depedendent on stress
void setPhonemeLength(struct SamState &state) {
	int position = 0;
	while(state.phonemes[position].index != 255) {
		unsigned char A = state.phonemes[position].stress;
		if ((A == 0) || ((A&128) != 0)) {
            state.phonemes[position].length = phonemeLengthTable[state.phonemes[position].index];
		} else {
            state.phonemes[position].length = phonemeStressedLengthTable[state.phonemes[position].index];
		}
		position++;
	}
}

void code41240(struct SamState &state) {
	unsigned char pos=0;

	while(state.phonemes[pos].index != END) {
		unsigned char index = state.phonemes[pos].index;

		if ((flags[index] & FLAG_STOPCONS)) {
            if ((flags[index] & FLAG_PLOSIVE)) {
                unsigned char A;
                unsigned char X = pos;
                while(!state.phonemes[++X].index); /* Skip pause */
                A = state.phonemes[X].index;
                if (A != END) {
                    if ((flags[A] & 8) || (A == 36) || (A == 37)) {++pos; continue;} // '/H' '/X'
                }
                
            }
            insert(state, pos + 1, index + 1, phonemeLengthTable[index + 1], state.phonemes[pos].stress);
            insert(state, pos + 2, index + 2, phonemeLengthTable[index + 2], state.phonemes[pos].stress);
            pos += 2;
        }
        ++pos;
	}
}


void ChangeRule(struct SamState &state, unsigned char position, unsigned char mem60, const char * descr)
{
    if (state.debug) printf("RULE: %s\n",descr);
    state.phonemes[position].index = 13; //rule;
    insert(state, position + 1, mem60, 0, state.phonemes[position].stress);
}

void drule(struct SamState &state, const char * str) {
    if (state.debug) printf("RULE: %s\n",str);
}

void drule_pre(struct SamState &state,const char *descr, unsigned char X) {
    drule(state, descr);
    if (state.debug) {
        printf("PRE\n");
        printf("phoneme %d (%c%c) length %d\n", X, signInputTable1[state.phonemes[X].index], signInputTable2[state.phonemes[X].index], state.phonemes[X].length);
    }
}

void drule_post(struct SamState &state,unsigned char X) {
    if (state.debug) {
        printf("POST\n");
        printf("phoneme %d (%c%c) length %d\n", X, signInputTable1[state.phonemes[X].index], signInputTable2[state.phonemes[X].index], state.phonemes[X].length);
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
    if (flags[state.phonemes[X-1].index] & FLAG_ALVEOLAR) {
        drule(state, "<ALVEOLAR> UW -> <ALVEOLAR> UX");
        state.phonemes[X].index = 16;
    }
}

void rule_ch(struct SamState &state, unsigned char X) {
    drule(state, "CH -> CH CH+1");
    insert(state, X + 1, 43, 0, state.phonemes[X].stress);
}

void rule_j(struct SamState &state, unsigned char X) {
    drule(state, "J -> J J+1");
    insert(state, X + 1, 45, 0, state.phonemes[X].stress);
}

void rule_g(struct SamState &state, unsigned char pos) {
    // G <VOWEL OR DIPTHONG NOT ENDING WITH IY> -> GX <VOWEL OR DIPTHONG NOT ENDING WITH IY>
    // Example: GO

    unsigned char index = state.phonemes[pos+1].index;
            
    // If dipthong ending with YX, move continue processing next phoneme
    if ((index != 255) && ((flags[index] & FLAG_DIP_YX) == 0)) {
        // replace G with GX and continue processing next phoneme
        drule(state, "G <VOWEL OR DIPTHONG NOT ENDING WITH IY> -> GX <VOWEL OR DIPTHONG NOT ENDING WITH IY>");
        state.phonemes[pos].index = 63; // 'GX'
    }
}


void change(struct SamState &state, unsigned char pos, unsigned char val, const char * rule) {
    drule(state, rule);
    state.phonemes[pos].index = val;
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
    insert(state, pos + 1, A, 0, state.phonemes[pos].stress);
                
    if (p == 53) rule_alveolar_uw(state, pos); // Example: NEW, DEW, SUE, ZOO, THOO, TOO
    else if (p == 42) rule_ch(state, pos);     // Example: CHEW
    else if (p == 44) rule_j(state, pos);      // Example: JAY
}

void parser2(struct SamState &state) {
	unsigned char pos = 0; //mem66;
    unsigned char p;

	if (state.debug) printf("parser2\n");

	while((p = state.phonemes[pos].index) != END) {
		unsigned short pf;
		unsigned char prior;

		if (state.debug) printf("%d: %c%c\n", pos, signInputTable1[p], signInputTable2[p]);

		if (p == 0) { // Is phoneme pause?
			++pos;
			continue;
		}

        pf = flags[p];
        prior = state.phonemes[pos-1].index;

        if ((pf & FLAG_DIPTHONG)) rule_dipthong(state, p, pf, pos);
        else if (p == 78) ChangeRule(state, pos, 24, "UL -> AX L"); // Example: MEDDLE
        else if (p == 79) ChangeRule(state, pos, 27, "UM -> AX M"); // Example: ASTRONOMY
        else if (p == 80) ChangeRule(state, pos, 28, "UN -> AX N"); // Example: FUNCTION
        else if ((pf & FLAG_VOWEL) && state.phonemes[pos].stress) {
            // RULE:
            //       <STRESSED VOWEL> <SILENCE> <STRESSED VOWEL> -> <STRESSED VOWEL> <SILENCE> Q <VOWEL>
            // EXAMPLE: AWAY EIGHT
            if (!state.phonemes[pos+1].index) { // If following phoneme is a pause, get next
                p = state.phonemes[pos+2].index;
                if (p!=END && (flags[p] & FLAG_VOWEL) && state.phonemes[pos+2].stress) {
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
                unsigned char Y = state.phonemes[pos+1].index;
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
                state.phonemes[pos].index = p-12;
            } else if (!(pf & FLAG_PLOSIVE)) {
                p = state.phonemes[pos].index;
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
                if (flags[state.phonemes[pos-1].index] & FLAG_VOWEL) {
                    p = state.phonemes[pos+1].index;
                    if (!p) p = state.phonemes[pos+2].index;
                    if ((flags[p] & FLAG_VOWEL) && !state.phonemes[pos+1].stress) change(state, pos,30, "Soften T or D following vowel or ER and preceding a pause -> DX");
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

	while((index = state.phonemes[X].index) != END) {
		unsigned char loopIndex;

		// not punctuation?
		if((flags[index] & FLAG_PUNCT) == 0) {
			++X;
			continue;
		}

		loopIndex = X;

        while (--X && !(flags[state.phonemes[X].index] & FLAG_VOWEL)); // back up while not a vowel
        if (X == 0) break;

		do {
            // test for vowel
			index = state.phonemes[X].index;

			// test for fricative/unvoiced or not voiced
			if(!(flags[index] & FLAG_FRICATIVE) || (flags[index] & FLAG_VOICED)) {     //nochmal �berpr�fen
				unsigned char A = state.phonemes[X].length;
				// change phoneme length to (length * 1.5) + 1
                drule_pre(state, "Lengthen <FRICATIVE> or <VOICED> between <VOWEL> and <PUNCTUATION> by 1.5",X);
				state.phonemes[X].length = (A >> 1) + A + 1;
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

	while((index = state.phonemes[loopIndex].index) != END) {
		unsigned char X = loopIndex;

		if (flags[index] & FLAG_VOWEL) {
			index = state.phonemes[loopIndex+1].index;
			if (!(flags[index] & FLAG_CONSONANT)) {
				if ((index == 18) || (index == 19)) { // 'RX', 'LX'
					index = state.phonemes[loopIndex+2].index;
					if ((flags[index] & FLAG_CONSONANT)) {
                        drule_pre(state, "<VOWEL> <RX | LX> <CONSONANT> - decrease length of vowel by 1\n", loopIndex);
                        state.phonemes[loopIndex].length--;
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
                        state.phonemes[loopIndex].length -= (state.phonemes[loopIndex].length >> 3);
                        drule_post(state, loopIndex);
                    }
                } else {
                    unsigned char A;
                    drule_pre(state, "<VOWEL> <VOICED CONSONANT> - increase vowel by 1/2 + 1\n",X-1);
                    // decrease length
                    A = state.phonemes[loopIndex].length;
                    state.phonemes[loopIndex].length = (A >> 2) + A + 1;     // 5/4*A + 1
                    drule_post(state, loopIndex);
                }
            }
		} else if((flags[index] & FLAG_NASAL) != 0) { // nasal?
            // RULE: <NASAL> <STOP CONSONANT>
            //       Set punctuation length to 6
            //       Set stop consonant length to 5
            index = state.phonemes[++X].index;
            if (index != END && (flags[index] & FLAG_STOPCONS)) {
                drule(state, "<NASAL> <STOP CONSONANT> - set nasal = 5, consonant = 6");
                state.phonemes[X].length   = 6; // set stop consonant length to 6
                state.phonemes[X-1].length = 5; // set nasal length to 5
            }
        } else if((flags[index] & FLAG_STOPCONS)) { // (voiced) stop consonant?
            // RULE: <VOICED STOP CONSONANT> {optional silence} <STOP CONSONANT>
            //       Shorten both to (length/2 + 1)

            // move past silence
            while ((index = state.phonemes[++X].index) == 0);

            if (index != END && (flags[index] & FLAG_STOPCONS)) {
                // FIXME, this looks wrong?
                // RULE: <UNVOICED STOP CONSONANT> {optional silence} <STOP CONSONANT>
                drule(state, "<UNVOICED STOP CONSONANT> {optional silence} <STOP CONSONANT> - shorten both to 1/2 + 1");
                state.phonemes[X].length         = (state.phonemes[X].length >> 1) + 1;
                state.phonemes[loopIndex].length = (state.phonemes[loopIndex].length >> 1) + 1;
                X = loopIndex;
            }
        } else if ((flags[index] & FLAG_LIQUIC)) { // liquic consonant?
            // RULE: <VOICED NON-VOWEL> <DIPTHONG>
            //       Decrease <DIPTHONG> by 2
            index = state.phonemes[X-1].index; // prior phoneme;

            // FIXME: The debug code here breaks the rule.
            // prior phoneme a stop consonant>
            if((flags[index] & FLAG_STOPCONS) != 0) 
                drule_pre(state, "<LIQUID CONSONANT> <DIPTHONG> - decrease by 2",X);

            state.phonemes[X].length -= 2; // 20ms
            drule_post(state, X);
         }

        ++loopIndex;
    }
}
