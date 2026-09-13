#pragma once

/*
 * Filled rectangles, for the parts DrawLines can't do.
 *
 * based on DrawLines.hpp from the base code: same immediate-mode pattern and
 * the same ColorProgram, just triangles instead of line segments.
 *
 */

#include <glm/glm.hpp>

#include <vector>

struct DrawQuads {
	//start drawing; will remember world_to_clip matrix:
	DrawQuads(glm::mat4 const &world_to_clip);

	//draw an axis-aligned rectangle (in world space):
	void draw_rect(glm::vec2 const &min, glm::vec2 const &max, glm::u8vec4 const &color = glm::u8vec4(0xff));

	//finish drawing (push attribs to GPU):
	~DrawQuads();

	glm::mat4 world_to_clip;
	struct Vertex {
		Vertex(glm::vec3 const &Position_, glm::u8vec4 const &Color_) : Position(Position_), Color(Color_) { }
		glm::vec3 Position;
		glm::u8vec4 Color;
	};
	std::vector< Vertex > attribs;
};
