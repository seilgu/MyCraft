#version 130
#extension GL_EXT_gpu_shader4 : enable
precision highp float;

#define EPSILON 0.5

in vec2 screenPos;


uniform sampler2D scrbuffer;
uniform sampler2D depthbuffer;

out vec4 fragColor;

#define SIZE 5
uniform float mask1[SIZE*SIZE] = float[](0, 0, 0, 0, 0, 
										 0, 0, 1, 0, 0, 
										 0, 1, 1, 1, 0, 
										 0, 0, 1, 0, 0, 
										 0, 0, 0, 0, 0 );
uniform float mask2[SIZE*SIZE] = float[](0, 0, 0, 0, 0, 
										 0, 1, 1, 1, 0, 
										 0, 1, 1, 1, 0, 
										 0, 1, 1, 1, 0, 
										 0, 0, 0, 0, 0 );
uniform float mask3[SIZE*SIZE] = float[](0, 0, 1, 0, 0, 
										 0, 1, 1, 1, 0, 
										 1, 1, 1, 1, 1, 
										 0, 1, 1, 1, 0, 
										 0, 0, 1, 0, 0 );
uniform float mask4[SIZE*SIZE] = float[](0, 1, 1, 1, 0, 
										 1, 1, 1, 1, 1, 
										 1, 1, 1, 1, 1, 
										 1, 1, 1, 1, 1, 
										 0, 1, 1, 1, 0 );
void main(void) {
	vec2 tex = 0.5*screenPos + 0.5;
	vec4 fboclr = texture(depthbuffer, tex);
	vec4 scrclr = vec4(0);

	vec2 dxy = vec2(1.0/1200, 1.0/800);

	float focus = sqrt( texture(depthbuffer, vec2(0.5, 0.5)).x );
	float diff = abs( sqrt(fboclr.x) - focus );
	float accum = 0;
	if ( diff < 2.0f ) {
		scrclr = texture( scrbuffer, tex);
	}
	else if ( diff < 4.0f ) {
		for (int i=0; i<SIZE; i++) {
			for (int j=0; j<SIZE; j++) {
				scrclr += texture( scrbuffer, tex + vec2((j - SIZE/2), (i-SIZE/2))*dxy ) * mask1[j*SIZE + i];
			}
		}
		scrclr /= 5;
	}
	else if (diff < 8.0f ) {
		for (int i=0; i<SIZE; i++) {
			for (int j=0; j<SIZE; j++) {
				scrclr += texture( scrbuffer, tex + vec2((j - SIZE/2), (i-SIZE/2))*dxy ) * mask2[j*SIZE + i];
			}
		}
		scrclr /= 9;
	}
	else if (diff < 12.0f ) {
		for (int i=0; i<SIZE; i++) {
			for (int j=0; j<SIZE; j++) {
				scrclr += texture( scrbuffer, tex + vec2((j - SIZE/2), (i-SIZE/2))*dxy ) * mask3[j*SIZE + i];
			}
		}
		scrclr /= 13;
	}
	else {
		for (int i=0; i<SIZE; i++) {
			for (int j=0; j<SIZE; j++) {
				scrclr += texture( scrbuffer, tex + vec2((j - SIZE/2), (i-SIZE/2))*dxy ) * mask4[j*SIZE + i];
			}
		}
		scrclr /= 21;
	}
	
	fragColor = scrclr;
}
