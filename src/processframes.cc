#include "render.h"
#include "state.h"

// From RenderTabs.h
extern unsigned char multtable[];
extern unsigned char sinus[];
extern unsigned char rectangle[];

extern void Output(struct SamState &state, int index, unsigned char A);

static void CombineGlottalAndFormants(struct SamState &state, unsigned char phase1, unsigned char phase2, unsigned char phase3, unsigned char Y)
{
    unsigned int tmp;

    tmp   = multtable[sinus[phase1]     | state.amplitude1[Y]];
    tmp  += multtable[sinus[phase2]     | state.amplitude2[Y]];
    tmp  += tmp > 255 ? 1 : 0; // if addition above overflows, we for some reason add one;
    tmp  += multtable[rectangle[phase3] | state.amplitude3[Y]];
    tmp  += 136;
    tmp >>= 4; // Scale down to 0..15 range of C64 audio.
            
    Output(state, 0, tmp & 0xf);
}

// PROCESS THE FRAMES
//
// In traditional vocal synthesis, the glottal pulse drives filters, which
// are attenuated to the frequencies of the formants.
//
// SAM generates these formants directly with sin and rectangular waves.
// To simulate them being driven by the glottal pulse, the waveforms are
// reset at the beginning of each glottal pulse.
//
void ProcessFrames(struct SamState &state, unsigned char mem48)
{
    unsigned char speedcounter = 72;
	unsigned char phase1 = 0;
    unsigned char phase2 = 0;
	unsigned char phase3 = 0;
    unsigned char mem66 = 0; //!! was not initialized
    
    unsigned char Y = 0;

    unsigned char glottal_pulse = state.pitches[0];
    unsigned char mem38 = glottal_pulse - (glottal_pulse >> 2); // mem44 * 0.75

	while(mem48) {
		unsigned char flags = state.sampledConsonantFlag[Y];
		
		// unvoiced sampled phoneme?
        if(flags & 248) {
			RenderSample(state, &mem66, flags,Y);
			// skip ahead two in the phoneme buffer
			Y += 2;
			mem48 -= 2;
            speedcounter = state.speed;
		} else {
            CombineGlottalAndFormants(state, phase1, phase2, phase3, Y);

			speedcounter--;
			if (speedcounter == 0) { 
                Y++; //go to next amplitude
                // decrement the frame count
                mem48--;
                if(mem48 == 0) return;
                speedcounter = state.speed;
            }
         
            --glottal_pulse;
		
            if(glottal_pulse != 0) {
                // not finished with a glottal pulse

                --mem38;
                // within the first 75% of the glottal pulse?
                // is the count non-zero and the sampled flag is zero?
                if((mem38 != 0) || (flags == 0)) {
                    // reset the phase of the formants to match the pulse
                    phase1 += state.frequency1[Y];
                    phase2 += state.frequency2[Y];
                    phase3 += state.frequency3[Y];
                    continue;
                }
                
                // voiced sampled phonemes interleave the sample with the
                // glottal pulse. The sample flag is non-zero, so render
                // the sample for the phoneme.
                RenderSample(state, &mem66, flags,Y);
            }
        }

        glottal_pulse = state.pitches[Y];
        mem38 = glottal_pulse - (glottal_pulse>>2); // mem44 * 0.75

        // reset the formant wave generators to keep them in 
        // sync with the glottal pulse
        phase1 = 0;
        phase2 = 0;
        phase3 = 0;
	}
}
