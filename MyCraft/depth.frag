#version 130

#extension GL_EXT_gpu_shader4 : enable

precision highp float;

#define zNear 1.0f
#define zFar 1000.0f

uniform vec4 player_pos;
in vec4 world_pos;
out vec4 fragColor;

void main(void) {
	//float depth = length(player_pos.xyz - world_pos.xyz);
	float depth = dot(player_pos.xyz - world_pos.xyz, player_pos.xyz - world_pos.xyz);
	fragColor = vec4(depth, depth, depth, 1);

	//fragColor = vec4(gl_FragCoord.z, gl_FragCoord.w, 0, 1);
}
