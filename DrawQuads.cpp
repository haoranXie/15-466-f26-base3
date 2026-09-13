#include "DrawQuads.hpp"
#include "ColorProgram.hpp"
#include "Load.hpp"

#include "gl_errors.hpp"

#include <glm/gtc/type_ptr.hpp>

//based on DrawLines.cpp from the base code.

//n.b. declared static so they don't conflict with the identically named globals
//in DrawLines.cpp:
static GLuint vertex_buffer = 0;
static GLuint vertex_buffer_for_color_program = 0;

static Load< void > setup_buffers(LoadTagDefault, [](){
	glGenBuffers(1, &vertex_buffer);

	glGenVertexArrays(1, &vertex_buffer_for_color_program);
	glBindVertexArray(vertex_buffer_for_color_program);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

	glVertexAttribPointer(
		color_program->Position_vec4,
		3, GL_FLOAT, GL_FALSE,
		sizeof(DrawQuads::Vertex),
		(GLbyte *)0 + offsetof(DrawQuads::Vertex, Position)
	);
	glEnableVertexAttribArray(color_program->Position_vec4);

	glVertexAttribPointer(
		color_program->Color_vec4,
		4, GL_UNSIGNED_BYTE, GL_TRUE,
		sizeof(DrawQuads::Vertex),
		(GLbyte *)0 + offsetof(DrawQuads::Vertex, Color)
	);
	glEnableVertexAttribArray(color_program->Color_vec4);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	GL_ERRORS();
});

DrawQuads::DrawQuads(glm::mat4 const &world_to_clip_) : world_to_clip(world_to_clip_) {
}

void DrawQuads::draw_rect(glm::vec2 const &min, glm::vec2 const &max, glm::u8vec4 const &color) {
	glm::vec3 a = glm::vec3(min.x, min.y, 0.0f);
	glm::vec3 b = glm::vec3(max.x, min.y, 0.0f);
	glm::vec3 c = glm::vec3(max.x, max.y, 0.0f);
	glm::vec3 d = glm::vec3(min.x, max.y, 0.0f);

	attribs.emplace_back(a, color);
	attribs.emplace_back(b, color);
	attribs.emplace_back(c, color);

	attribs.emplace_back(a, color);
	attribs.emplace_back(c, color);
	attribs.emplace_back(d, color);
}

DrawQuads::~DrawQuads() {
	if (attribs.empty()) return;

	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, attribs.size() * sizeof(attribs[0]), attribs.data(), GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glUseProgram(color_program->program);
	glUniformMatrix4fv(color_program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(world_to_clip));

	glBindVertexArray(vertex_buffer_for_color_program);
	glDrawArrays(GL_TRIANGLES, 0, GLsizei(attribs.size()));
	glBindVertexArray(0);

	glUseProgram(0);
}
