#version 120

uniform float uZFar;
uniform float uZUnproject;

uniform float uExposure;

uniform sampler2D uDepth;
uniform sampler2D uNormal;
uniform sampler2D uDiffuse;
uniform sampler2D uSpecular;

const vec3 beta_sc = vec3(0.02);
const vec3 beta_ex = 1.1 * beta_sc;

struct Light {
	vec3 pos_v; // Viewspace position
	vec3 flux;
};

uniform int uNumLights;
uniform Light[64] uLights;

const float pi = 3.14159265;

varying vec2 vTextureCoord;

// also +ve
float read_log_depth() {
	const float c = 0.01;
	float fc = 1.0 / log(uZFar * c + 1.0);
	return (exp(texture2D(uDepth, vTextureCoord).r / fc) - 1.0) / c;
}



// also +ve
float read_depth() {
	return texture2D(uDepth, vTextureCoord).r * uZFar;
}



// approximation to integral of phong specular lobe
float phong_lobe_integral(float a) {
	return pi * (0.24 + 1.0 / pow(a + 0.5, 0.42) - 0.26 * (1.0 - exp(-sqrt(a * 0.4))));
}

vec3 lambertPhong(vec3 e, vec3 ldir, vec3 norm, vec3 vdir, vec3 diffuse, vec3 specular, float shininess) {
		vec3 l = vec3(0.0);

		// lambert diffuse radiance leaving surface
		l += e * diffuse / pi;

		// reflection direction
		vec3 rdir = reflect(-ldir, norm);

		// phong specular radiance leaving surface
		l += e * specular * pow(max(0.0, dot(rdir, vdir)), shininess) / phong_lobe_integral(shininess);

		return l;
}


// mie scattering phase function
float phase_m(float mu) {
	const float g = 0.75;
	return 3.0 * (1.0 - g * g) * (1.0 + mu * mu) / (8.0 * pi * (2.0 + g * g) * pow(1.0 + g * g - 2.0 * g * mu, 1.5));
}

// transmittance over some distance
vec3 transmittance(float d) {
	return exp(-beta_ex * d);
}

// size of next inscatter sample
float inscatter_sample_size(vec3 pos_v, vec3 dir_v, Light light) {
	// random hacky stuff, will have to do
	vec3 ld = light.pos_v - pos_v;
	float dl2 = dot(ld, ld);
	ld = normalize(ld);
	float k = abs(dot(dir_v, ld));
	return 0.3 + mix(dl2 * 0.02, 0.2 * sqrt(dl2), k);
}

// inscattered radiance along a ray from a light
vec3 inscatter(vec3 pos0_v, vec3 dir_v, float d_max, Light light) {
	vec3 p = pos0_v;
	vec3 l = vec3(0.0);
	for (float d = 0.0; d < min(d_max, 100.0); ) {
		float dd = inscatter_sample_size(p, dir_v, light);
		vec3 ph = p + dd * 0.5;
		float dl = length(light.pos_v - ph);
		float muvs = dot(normalize(light.pos_v - ph), dir_v);
		vec3 e0 = light.flux / pow(dl, 2.0) * transmittance(dl);
		l += transmittance(d + 0.5 * dd) * e0 * beta_sc * phase_m(muvs) * dd;
		p += dd * dir_v;
		d += dd;
	}
	return l;
}

float cos_atan(float v) {
	return 1/sqrt(1+v*v);
}


void main() {
	// view-space depth (+ve)
	// float depth_v = read_depth();
	float depth_v = read_log_depth();

	// view-space near plane ray intersection
	vec4 projection_nearplane = gl_ProjectionMatrixInverse * vec4((vTextureCoord * 2.0 - 1.0), -1.0, 1.0);
	vec3 pos_nearplane = (projection_nearplane / projection_nearplane.w).xyz;

	// view-space 'far' plane ray intersection
	// it would be nice if we could just use the far plane, but application of the
	// inverse projection matrix results in precision problems. so, we let the
	// application decide what z (ndc) will be used for unprojection.
	vec4 projection_farplane = gl_ProjectionMatrixInverse * vec4((vTextureCoord * 2.0 - 1.0), uZUnproject, 1.0);
	vec3 pos_farplane = (projection_farplane / projection_farplane.w).xyz;
	
	// view-space ray direction (from viewer to frament)
	// 
	vec3 dir_v = normalize(pos_farplane - pos_nearplane);

	// view-space fragment position
	// 
	vec3 pos_v = pos_nearplane + dir_v * ((depth_v + pos_nearplane.z) / -dir_v.z);


	// view-space normal
	// need to renormalize because normals are stored in lower precision
	vec3 norm_v = normalize(texture2D(uNormal, vTextureCoord).xyz);

	// fragment material properties
	// 
	vec3 diffuse = texture2D(uDiffuse, vTextureCoord).rgb;
	bool emmisive = texture2D(uDiffuse, vTextureCoord).a > 0.5; //hack
	vec3 specular = texture2D(uSpecular, vTextureCoord).rgb;
	float shininess = texture2D(uSpecular, vTextureCoord).a;


	if (emmisive) {
		gl_FragColor.rgb = diffuse;
		gl_FragColor.a = 1.0;

	} else {

		// Now we calculate the lighting
		// output radiance of surface
		vec3 l = vec3(0.0);

		// ambient hack to make everything NOT black
		l += diffuse * 0.05;

		for (int i = 0; i < uNumLights; ++i) {
			// direction and distance from fragment to light
			vec3 ldir_v = uLights[i].pos_v - pos_v;
			float d = length(ldir_v);
			ldir_v = normalize(ldir_v);
			
			// Irradiance from light
			vec3 e = uLights[i].flux / pow(d, 2.0) * transmittance(d);
			e *= max(0.0, dot(ldir_v, norm_v));

			// Add the result of radiance from this light
			l += lambertPhong(e, ldir_v, norm_v, -dir_v, diffuse, specular, shininess);


			l += inscatter(pos_nearplane, dir_v, length(pos_v), uLights[i]);
		}

		// simple tonemapping for HDR
		gl_FragColor.rgb = 1.0 - exp(-uExposure * l);

		gl_FragColor.a = 1.0;
	}

}
