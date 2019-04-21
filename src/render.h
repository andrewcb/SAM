#ifndef RENDER_H
#define RENDER_H

void Render(struct SamState& state);
void setMouthThroat(unsigned char mouth, unsigned char throat);

void ProcessFrames(struct SamState& state, unsigned char mem48);
void RenderSample(struct SamState &state, unsigned char *mem66, unsigned char consonantFlag, unsigned char mem49);
unsigned char CreateTransitions(struct SamState& state);

#define PHONEME_PERIOD (1)
#define PHONEME_QUESTION (2)

#define RISING_INFLECTION (1)
#define FALLING_INFLECTION (255)

#endif
