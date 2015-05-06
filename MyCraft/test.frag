#version 130
#extension GL_EXT_gpu_shader4 : enable
precision highp float;

#define PZ 2
#define NZ 3
#define PY 0
#define NY 1
#define PX 4
#define NX 5

#define EPSILON 0.002

in vec2 screenPos;

uniform ivec3 NUM;
uniform ivec3 CHUNK;

uniform float WATER_N;
uniform float time;

uniform vec3 startpos;

uniform sampler2D block_texture;		//block texture
uniform usampler3D testbuffer;	//voxel data
uniform sampler2D depthfbo;

in vec4 color;		// for ray dir
out vec4 fragColor;

uniform vec3 lightpos[4];


#define NUL		0U
#define CRATE	1U
#define GRASS	2U
#define SOIL	3U
#define STONE	4U
#define SAND	8U
#define GLASS	9U
#define LAVA	10U
#define SNOW	11U

struct Ray {
	vec3 pos;
	vec3 dir;
};


vec4 get_tex_color(Ray r, float depth, uint texel, int side) {
	if (texel == NUL)
		return vec4(0);

	vec2 tcoord;
	switch (texel) {
	case CRATE: tcoord = vec2(19, 4); break;
	case GRASS:
		switch (side) {
		case PZ: tcoord = vec2(0, 15); break;
		case NZ: tcoord = vec2(2, 15); break;
		default: tcoord = vec2(3, 15); break;
		}
		break;
	case SOIL: tcoord = vec2(2, 15); break;
	case STONE: tcoord = vec2(1, 15); break;
	case SAND: tcoord = vec2(2, 14); break;
	case GLASS: tcoord = vec2(15, 2); break;
	case LAVA: tcoord = vec2(15, 0); break;
	case SNOW:
		switch (side) {
		case PZ: tcoord = vec2(2, 11); break;
		case NZ: tcoord = vec2(2, 15); break;
		default: tcoord = vec2(4, 11); break;
		}
		break;
	default: break;
	}

	switch (side) {
	case PZ: case NZ: tcoord += fract(r.pos.xy + depth*r.dir.xy);
		break;
	case PY: case NY: tcoord += fract(r.pos.xz + depth*r.dir.xz);
		break;
	case PX: case NX: tcoord += fract(r.pos.yz + depth*r.dir.yz);
		break;
	}

	tcoord /= 16;

	return texture2D(block_texture, tcoord);
}

uint voxel(ivec3 pos) {
	return texelFetch(testbuffer, pos, 0).w;
}

vec4 air_ray(Ray r, int count);

vec4 light_ray(Ray r, float depth) {
	if (depth >= 20.0f)
		return vec4(0);
	ivec3 voxpos = ivec3( floor(r.pos) );
	ivec3 next = voxpos + ivec3(0.5*sign(r.dir) + 0.5);
	ivec3 step = ivec3(sign(r.dir));
	vec3 tMax = (next - r.pos)/r.dir;
	vec3 tDelta = step/r.dir;

	voxpos = (NUM/2)*CHUNK + voxpos;

	uint result;
	
	int count = 64;
	while (count-- != 0) {
		if (tMax.x < tMax.y && tMax.x < tMax.z) {
			if (tMax.x > depth) { depth = tMax.x; break; }
			voxpos.x += step.x;
			tMax.x += tDelta.x;
		}
		else if (tMax.y < tMax.z) {
			if (tMax.y > depth) { depth = tMax.y; break; }
			voxpos.y += step.y;
			tMax.y += tDelta.y;
		}
		else {
			if (tMax.z > depth) { depth = tMax.z; break; }
			voxpos.z += step.z;
			tMax.z += tDelta.z;
		}

		result = voxel(voxpos);

		if (result != 0U) {
			return vec4(0, 0, 0, 0);
		}
	}

	return vec4(1, 1, 1, 1)*pow(0.95, depth);
}

#define TRANS_COEF 1
vec4 refraction_ray(Ray r, int count) {
	float cosine;

	ivec3 voxpos = ivec3( floor(r.pos) );
	ivec3 next = voxpos + ivec3(0.5*sign(r.dir) + 0.5);
	ivec3 step = ivec3(sign(r.dir));
	vec3 tMax = (next - r.pos)/r.dir;
	vec3 tDelta = step/r.dir;

	voxpos = (NUM/2)*CHUNK + voxpos;

	bool bX = false;
	bool bY = false;
	uint result;
	
	while (--count != 0) {

		if (bX = (tMax.x < tMax.y && tMax.x < tMax.z)) {
			voxpos.x += step.x;
			tMax.x += tDelta.x;
		}
		else if (bY = (tMax.y < tMax.z)) {
			voxpos.y += step.y;
			tMax.y += tDelta.y;
		}
		else {
			voxpos.z += step.z;
			tMax.z += tDelta.z;
		}

		result = voxel(voxpos);
		
		if (result != 9U) {
			float depth;
			int tmp_side;
			if (bX)			{ depth = tMax.x - tDelta.x; cosine = r.dir.x*step.x; tmp_side = (step.x > 0 ? NX : PX); }
			else if (bY)	{ depth = tMax.y - tDelta.y; cosine = r.dir.y*step.y; tmp_side = (step.y > 0 ? NY : PY); }
			else			{ depth = tMax.z - tDelta.z; cosine = r.dir.z*step.z; tmp_side = (step.z > 0 ? NZ : PZ); }

			/*if (result == 0U) {
				// refraction
				
					// n1 sin theta1 = n2 sin theta2
					float rad = length(r.dir.xy);
					float sint2 = WATER_N*rad / length(r.dir);
					if (sint2 > 1)
						return vec4(0);

					Ray sec;
					sec.pos = r.pos + (depth+EPSILON)*r.dir;
					sec.dir = r.dir;
					sec.dir.xy *= sec.dir.z*tan(asin(sint2))/rad;
					
					return air_ray(sec, count);
			}*/

			// pow is slow!
			return get_tex_color(r, depth, result, tmp_side)*cosine;//*pow(TRANS_COEF, depth);
		}
	}

	return vec4(0);
}

vec4 air_ray(Ray r, int count) {
	float cosine;

	ivec3 voxpos = ivec3( floor(r.pos) );
	ivec3 next = voxpos + ivec3(0.5*sign(r.dir) + 0.5);
	ivec3 step = ivec3(sign(r.dir));
	vec3 tMax = (next - r.pos)/r.dir;
	vec3 tDelta = step/r.dir;

	voxpos = (NUM/2)*CHUNK + voxpos;

	bool bX = false;
	bool bY = false;
	uint result;
	
	while (--count != 0) {

		if (bX = (tMax.x < tMax.y && tMax.x < tMax.z)) {
			voxpos.x += step.x;
			tMax.x += tDelta.x;
		}
		else if (bY = (tMax.y < tMax.z)) {
			voxpos.y += step.y;
			tMax.y += tDelta.y;
		}
		else {
			voxpos.z += step.z;
			tMax.z += tDelta.z;
		}

		result = voxel(voxpos);
		if (result != 0U) {
			float depth;
			int tmp_side;
			if (bX)			{ depth = tMax.x - tDelta.x; cosine = r.dir.x*step.x; tmp_side = (step.x > 0 ? NX : PX); }
			else if (bY)	{ depth = tMax.y - tDelta.y; cosine = r.dir.y*step.y; tmp_side = (step.y > 0 ? NY : PY); }
			else			{ depth = tMax.z - tDelta.z; cosine = r.dir.z*step.z; tmp_side = (step.z > 0 ? NZ : PZ); }

			return get_tex_color(r, depth, result, tmp_side)*cosine;//*pow(TRANS_COEF, depth);
		}
	}

	return vec4(0);
}

// Main tracing function
vec4 trace_ray(Ray r, int count) {
	vec4 clr = vec4(0);
	float cosine;

	ivec3 voxpos = ivec3( floor(r.pos) );
	ivec3 next = voxpos + ivec3(0.5*sign(r.dir) + 0.5);
	ivec3 step = ivec3(sign(r.dir));
	vec3 tMax = (next - r.pos)/r.dir;
	vec3 tDelta = step/r.dir;

	voxpos = (NUM/2)*CHUNK + voxpos;

	bool bX = false;
	bool bY = false;
	uint result;
	
	while (--count > 0) {

		if (bX = (tMax.x < tMax.y && tMax.x < tMax.z)) {
			voxpos.x += step.x;
			tMax.x += tDelta.x;
		}
		else if (bY = (tMax.y < tMax.z)) {
			voxpos.y += step.y;
			tMax.y += tDelta.y;
		}
		else {
			voxpos.z += step.z;
			tMax.z += tDelta.z;
		}

		result = voxel(voxpos);
		if (result != 0U) {
			int tmp_side;
			float depth;
			if (bX)			{ depth = tMax.x - tDelta.x; cosine = r.dir.x*step.x; tmp_side = (step.x > 0 ? NX : PX); }
			else if (bY)	{ depth = tMax.y - tDelta.y; cosine = r.dir.y*step.y; tmp_side = (step.y > 0 ? NY : PY); }
			else			{ depth = tMax.z - tDelta.z; cosine = r.dir.z*step.z; tmp_side = (step.z > 0 ? NZ : PZ); }

			clr = get_tex_color(r, depth, result, tmp_side);

			vec3 rtmp = r.pos + depth*r.dir;

			if (result == 9U) { // glass/water.   PERFORMANCE HIT!!
				vec2 kvec = vec2(1, 1);
				float hxy = sin(dot(kvec, rtmp.xy) - time);
				vec3 normal = normalize( vec3(0,0,1) + 0.03*vec3(-kvec*hxy, 0) );


				// reflection
				Ray sec;
				sec.dir = reflect(r.dir, normal);
				sec.pos = rtmp - EPSILON*r.dir;
				vec3 refl_clr = air_ray(sec, 64).xyz;

				// refraction
				sec.pos = rtmp + EPSILON*r.dir;
				sec.dir = refract(r.dir, normal, 1.0/WATER_N);
				vec3 refr_clr = refraction_ray(sec, 8).xyz;
				
				refr_clr.xyz = mix( refr_clr.xyz, vec3(0, 0, 1), 0.4);
				clr.xyz = mix(refr_clr.xyz, refl_clr.xyz, 0.4);
			}

			// shadow
			Ray sec;
			sec.pos = rtmp - EPSILON*r.dir;
			vec3 light_clr = vec3(0);

			for (int w=0; w<4; w++) {
				sec.dir = lightpos[w] - sec.pos;
				float leng = length( sec.dir ) - EPSILON;
				sec.dir = normalize(sec.dir);

				vec3 tmp = light_ray(sec, leng).xyz;
				if (bX)			tmp *= abs(sec.dir.x);
				else if (bY)	tmp *= abs(sec.dir.y);
				else			tmp *= abs(sec.dir.z);

				light_clr += tmp;
			}
			clr.xyz *= light_clr;

			return clr;
		}
	}

	return clr;
}

void main(void) {
	vec4 fboclr = texture(depthfbo, screenPos*0.5 + 0.5);
	
	if (fboclr.w < 0.5) discard; // should really do early Z culling instead

	Ray ray;
	ray.dir = normalize(color.xyz);
	ray.pos = startpos;	// relative to chk(000), always positive
	
	float depth;
	vec4 res;
	if (voxel(ivec3( floor(ray.pos) ) + (NUM/2)*CHUNK ) == 9U) // water
		res = mix( vec4(0, 0, 1, 1), refraction_ray(ray, 64), 0.33);
	else {
		float depth = sqrt(fboclr.x);
		ray.pos += (depth - EPSILON)*ray.dir;
		res = trace_ray(ray, 2);
	}

	fragColor = vec4(res.xyz, depth);
}
