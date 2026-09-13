#include "Tones.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr float PI = 3.1415926f;

//the ramps at each end stop the note clicking on and off:
float envelope(float t, float len) {
	constexpr float Attack = 0.006f;
	constexpr float Release = 0.030f;
	float a = std::min(1.0f, t / Attack);
	float r = std::min(1.0f, std::max(0.0f, (len - t) / Release));
	return a * r * std::exp(-4.0f * t);
}

//mixes a note into a buffer at a given time, growing it if needed:
void mix_note(std::vector< float > &out, float at_seconds, Tones::Note const &note) {
	uint32_t start = uint32_t(std::max(0.0f, at_seconds) * float(Tones::Rate));
	uint32_t count = uint32_t(note.seconds * float(Tones::Rate));
	if (out.size() < start + count) out.resize(start + count, 0.0f);

	//divide by the total weight so a brighter note is not also a louder one:
	float second = 0.45f * note.brightness;
	float third = 0.18f * note.brightness;
	float scale = 0.36f / (1.0f + second + third);

	for (uint32_t i = 0; i < count; ++i) {
		float t = float(i) / float(Tones::Rate);
		float phase = 2.0f * PI * note.hz * t;
		float v = std::sin(phase) + second * std::sin(2.0f * phase) + third * std::sin(3.0f * phase);
		out[start + i] += note.gain * scale * envelope(t, note.seconds) * v;
	}
}

} //anonymous namespace

float Tones::pitch(float root_hz, float semitones, float cents) {
	return root_hz * std::exp2((semitones + cents / 100.0f) / 12.0f);
}

std::vector< float > Tones::render(Note const &note) {
	std::vector< float > out;
	mix_note(out, 0.0f, note);
	return out;
}

std::vector< float > Tones::chime() {
	std::vector< float > out;
	mix_note(out, 0.00f, Note{880.0f, 0.09f, 0.7f, 1.0f});
	mix_note(out, 0.07f, Note{1318.5f, 0.16f, 0.7f, 1.0f});
	return out;
}

std::vector< float > Tones::buzz() {
	std::vector< float > out;
	//two notes a semitone apart, low and sour:
	mix_note(out, 0.0f, Note{155.6f, 0.26f, 0.9f, 1.4f});
	mix_note(out, 0.0f, Note{164.8f, 0.26f, 0.9f, 1.4f});
	return out;
}

std::vector< float > Tones::fanfare() {
	std::vector< float > out;
	mix_note(out, 0.00f, Note{523.3f, 0.12f, 0.7f, 1.2f});
	mix_note(out, 0.10f, Note{659.3f, 0.12f, 0.7f, 1.2f});
	mix_note(out, 0.20f, Note{784.0f, 0.30f, 0.7f, 1.2f});
	return out;
}
