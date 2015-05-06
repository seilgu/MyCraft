
#include "Shader.h"

int Shader::UniformLocation(int id, char *name) {
	if (id >= MAX_SHADERS)
		return 0;

	return glGetUniformLocation(programs[id].program, name);
}

void Shader::UseProgram(int id) {
	if (id >= MAX_SHADERS)
		return;

	glUseProgram(programs[id].program);

	currentProgram = id;
}