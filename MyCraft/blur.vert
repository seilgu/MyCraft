

varying vec2 screenPos;

void main(void) {
	gl_Position = ftransform();
	screenPos = gl_Position.xy/gl_Position.w;
}
