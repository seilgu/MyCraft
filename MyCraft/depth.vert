

uniform vec4 offset;
varying out vec4 world_pos;

void main(void) {
	world_pos = gl_Vertex + offset;
	gl_Position = ftransform();
}
