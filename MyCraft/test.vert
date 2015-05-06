

attribute vec4 gl_Color; // direction

varying vec2 screenPos;

varying vec4 color;

void main(void) {
	gl_Position = ftransform();
	screenPos = gl_Position.xy/gl_Position.w;

	color = gl_Color;
}
