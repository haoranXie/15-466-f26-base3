#include "PlayMode.hpp"

#include "DrawLines.hpp"
#include "DrawQuads.hpp"
#include "Tones.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"

#include <algorithm>
#include <cmath>
#include <random>

//the Load<> blocks, the draw() scaffolding and the shadowed text below are
//based on the PlayMode.cpp that ships with the base3 code.

Load< Sound::Sample > chime_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(Tones::chime());
});

Load< Sound::Sample > buzz_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(Tones::buzz());
});

Load< Sound::Sample > clear_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(Tones::fanfare());
});

static std::mt19937 mt(std::random_device{}());

//finished loops are held a while: the mixer may still be reading a note that was
//playing when the loop ended.
static std::vector< std::unique_ptr< Sound::Sample > > retired;

//main.cpp turns on GL_FRAMEBUFFER_SRGB, so colors picked in sRGB have to be
//linear by the time they reach the shader. 2.2 is the usual approximation, from
//https://en.wikipedia.org/wiki/SRGB
static float to_linear(float srgb) {
	return std::pow(srgb, 2.2f);
}

static glm::u8vec4 srgb_color(uint32_t srgb) {
	glm::u8vec4 out = glm::u8vec4(0x00, 0x00, 0x00, 0xff);
	for (int c = 0; c < 3; ++c) {
		float s = float((srgb >> (8 * (2 - c))) & 0xff) / 255.0f;
		out[c] = uint8_t(std::round(255.0f * to_linear(s)));
	}
	return out;
}

PlayMode::PlayMode() {
	start_run();
}

PlayMode::~PlayMode() {
	Sound::stop_all_samples();
}

int PlayMode::slots_for(int loops_cleared) {
	return (loops_cleared < 3 ? 4 : (loops_cleared < 7 ? 6 : 8));
}

void PlayMode::start_run() {
	clock = RunSeconds;
	cleared = 0;
	game_over = false;
	clear_banner.clear();
	new_loop();
}

void PlayMode::new_loop() {
	for (auto &sample : samples) retired.emplace_back(std::move(sample));
	while (retired.size() > 12) retired.erase(retired.begin());
	samples.clear();

	//loops get longer and quicker as the levels go by:
	slot_count = slots_for(cleared);
	beat_seconds = std::max(0.26f, 0.40f - 0.012f * float(cleared));
	float shrink = std::pow(0.93f, float(cleared));

	//one repeated pitch per loop, so the odd note is the only thing that differs:
	float root = Tones::pitch(261.6f, float(std::uniform_int_distribution< int >(0, 11)(mt)));

	Tones::Note plain;
	plain.hz = root;
	plain.seconds = NoteSeconds;
	samples.emplace_back(std::make_unique< Sound::Sample >(Tones::render(plain)));

	for (auto &slot : slots) slot = Slot();

	int odd_slot = std::uniform_int_distribution< int >(0, slot_count - 1)(mt);
	slots[odd_slot].odd = true;

	//the first two loops are always out of tune, which is the easiest kind to hear:
	int axis = (cleared < 2 ? int(AxisPitch) : std::uniform_int_distribution< int >(0, AxisCount - 1)(mt));

	Tones::Note note = plain;
	if (axis == AxisPitch) {
		float cents = std::max(30.0f, 280.0f * shrink);
		if (std::uniform_int_distribution< int >(0, 1)(mt) == 0) cents = -cents;
		note.hz = Tones::pitch(root, 0.0f, cents);
	} else if (axis == AxisVolume) {
		note.gain = std::min(0.88f, 0.38f + 0.5f * (1.0f - shrink));
	} else if (axis == AxisTimbre) {
		note.brightness = std::max(1.20f, 2.60f * shrink);
	}

	samples.emplace_back(std::make_unique< Sound::Sample >(Tones::render(note)));
	slots[odd_slot].sample = int(samples.size()) - 1;

	beat = 0;
	next_beat = time;
	clear_timer = 0.0f;
}

void PlayMode::guess(int slot) {
	if (slots[slot].ruled_out) return;

	flash[slot] = 0.30f;
	flash_good[slot] = slots[slot].odd;

	if (slots[slot].odd) {
		cleared += 1;
		clear_timer = ClearPause;
		//warn the player when the loop is about to get longer:
		int next = slots_for(cleared);
		clear_banner = (next > slot_count ? std::to_string(next) + " NOTES NEXT" : "THAT WAS IT");
		Sound::play(*clear_sample, 0.8f);
	} else {
		//a wrong guess costs clock and takes the note out of the loop:
		slots[slot].ruled_out = true;
		clock -= WrongPenalty;
		Sound::play(*buzz_sample, 0.7f);
	}
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &) {
	if (evt.type != SDL_EVENT_KEY_DOWN) return false;

	if (game_over) {
		if (evt.key.key == SDLK_RETURN) {
			start_run();
			return true;
		}
		return false;
	}

	if (clear_timer > 0.0f) return false;

	//SDLK_1 through SDLK_8 run consecutively, so one range test covers however
	//many pads this loop has. Keycode values are from
	//https://wiki.libsdl.org/SDL3/SDL_Keycode
	if (evt.key.key >= SDLK_1 && evt.key.key < SDLK_1 + uint32_t(slot_count)) {
		guess(int(evt.key.key - SDLK_1));
		return true;
	}

	return false;
}

void PlayMode::update(float elapsed) {
	time += elapsed;

	for (int slot = 0; slot < MaxSlots; ++slot) {
		lit[slot] = std::max(0.0f, lit[slot] - elapsed);
		flash[slot] = std::max(0.0f, flash[slot] - elapsed);
	}

	if (game_over) return;

	clock -= elapsed;
	if (clock <= 0.0f) {
		clock = 0.0f;
		game_over = true;
		best = std::max(best, cleared + 1);
		return;
	}

	if (clear_timer > 0.0f) {
		clear_timer -= elapsed;
		if (clear_timer <= 0.0f) new_loop();
		return;
	}

	if (time >= next_beat) {
		//a ruled out note leaves a rest behind, so the beat stays put:
		int index = beat % slot_count;
		if (!slots[index].ruled_out) {
			Sound::play(*samples[slots[index].sample], 1.0f);
			lit[index] = 0.16f;
		}
		beat += 1;
		next_beat += beat_seconds;
	}
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	glClearColor(to_linear(0.055f), to_linear(0.059f), to_linear(0.078f), 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);

	float aspect = float(drawable_size.x) / float(drawable_size.y);
	glm::mat4 world_to_clip = glm::mat4(
		1.0f / aspect, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	glm::u8vec4 const Idle = srgb_color(0x272c3d);
	glm::u8vec4 const Beat = srgb_color(0x8e97bf);
	glm::u8vec4 const Good = srgb_color(0x4cc06a);
	glm::u8vec4 const Bad = srgb_color(0xd0485c);
	glm::u8vec4 const Out = srgb_color(0x3a2230);

	//pads share the same strip however many of them there are:
	constexpr float StripHalfW = 1.35f;
	constexpr float PadHalfH = 0.26f;
	constexpr float PadY = 0.02f;
	float step_x = 2.0f * StripHalfW / float(slot_count);
	float pad_half_w = 0.5f * step_x - 0.05f;
	auto pad_x = [&](int slot) {
		return -StripHalfW + step_x * (float(slot) + 0.5f);
	};

	{
		DrawQuads quads(world_to_clip);

		quads.draw_rect(glm::vec2(-StripHalfW, 0.62f), glm::vec2(StripHalfW, 0.68f), Idle);
		float left = clock / RunSeconds;
		quads.draw_rect(glm::vec2(-StripHalfW, 0.62f),
			glm::vec2(-StripHalfW + 2.0f * StripHalfW * left, 0.68f),
			(left < 0.2f ? Bad : Beat));

		for (int slot = 0; slot < slot_count; ++slot) {
			//pads light the same on every beat, so the odd note is not shown:
			glm::u8vec4 color = (lit[slot] > 0.0f ? Beat : Idle);
			if (slots[slot].ruled_out) color = Out;
			if (flash[slot] > 0.0f) color = (flash_good[slot] ? Good : Bad);

			float x = pad_x(slot);
			quads.draw_rect(glm::vec2(x - pad_half_w, PadY - PadHalfH),
				glm::vec2(x + pad_half_w, PadY + PadHalfH), color);
		}
	}

	{
		DrawLines lines(world_to_clip);

		float ofs = 2.0f / float(drawable_size.y);
		auto text = [&](std::string const &str, float x, float y, float h) {
			lines.draw_text(str, glm::vec3(x, y, 0.0f),
				glm::vec3(h, 0.0f, 0.0f), glm::vec3(0.0f, h, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0xff));
			lines.draw_text(str, glm::vec3(x + ofs, y + ofs, 0.0f),
				glm::vec3(h, 0.0f, 0.0f), glm::vec3(0.0f, h, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		};

		for (int slot = 0; slot < slot_count; ++slot) {
			text(std::to_string(slot + 1), pad_x(slot) - 0.05f, PadY - PadHalfH - 0.20f, 0.16f);
		}

		text("LEVEL " + std::to_string(cleared + 1), -StripHalfW, 0.76f, 0.12f);

		if (game_over) {
			text("TIME - REACHED LEVEL " + std::to_string(cleared + 1) + " - BEST " + std::to_string(best),
				-1.20f, -0.66f, 0.13f);
			text("ENTER to play again", -0.60f, -0.86f, 0.10f);
		} else if (clear_timer > 0.0f) {
			text(clear_banner, -0.55f, -0.62f, 0.16f);
		} else {
			text("one note in this loop is different - press its number",
				-1.12f, -0.62f, 0.09f);
			text("a wrong guess costs five seconds", -0.68f, -0.82f, 0.09f);
		}
	}

	GL_ERRORS();
}
