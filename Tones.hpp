#pragma once

#include <cstdint>
#include <vector>

//All of the game's audio is generated here at load time.

namespace Tones {

constexpr uint32_t Rate = 48000; //Sound expects 48kHz mono

struct Note {
	float hz = 440.0f;
	float seconds = 0.22f;
	float gain = 1.0f;
	float brightness = 1.0f; //scales the harmonics; 1 is the plain voice
};

//semitones above root, plus an optional offset in cents. 100 cents to the
//semitone, twelve semitones to the octave; see
//https://en.wikipedia.org/wiki/Cent_(music)
float pitch(float root_hz, float semitones, float cents = 0.0f);

//one note on its own, for playing a single slot of the loop:
std::vector< float > render(Note const &note);

std::vector< float > chime();   //a correct guess
std::vector< float > buzz();    //a wrong guess
std::vector< float > fanfare(); //a level cleared

} //namespace Tones
