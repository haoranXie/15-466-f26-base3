#include "Mode.hpp"

#include "Sound.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	static constexpr int MaxSlots = 8;

	static constexpr float NoteSeconds = 0.22f;
	static constexpr float ClearPause = 0.90f;
	static constexpr float RunSeconds = 60.0f;
	static constexpr float WrongPenalty = 5.0f;

	//the shape of the current loop; both move as the level goes up:
	int slot_count = 4;
	float beat_seconds = 0.40f;

	//the ways a note can differ:
	enum Axis { AxisPitch = 0, AxisVolume, AxisTimbre, AxisCount };

	struct Slot {
		bool odd = false;
		bool ruled_out = false; //already guessed during this loop
		int sample = 0; //index into samples; 0 is the plain note
	};

	static int slots_for(int loops_cleared);

	void start_run();
	void new_loop();
	void guess(int slot);

	//samples for the current loop, kept alive while the mixer might read them:
	std::vector< std::unique_ptr< Sound::Sample > > samples;
	Slot slots[MaxSlots];

	float time = 0.0f;
	float next_beat = 0.0f;
	int beat = 0;
	float lit[MaxSlots] = {0.0f};
	float flash[MaxSlots] = {0.0f};
	bool flash_good[MaxSlots] = {false};

	float clear_timer = 0.0f; //counts down between loops
	std::string clear_banner; //what changed, shown while the loop is clean

	float clock = RunSeconds;
	int cleared = 0; //loops finished; the level shown is one past this
	int best = 0;
	bool game_over = false;
};
