#version 120

uniform float uZFar;

uniform float uZUnproject;

uniform sampler2D uDepth;
uniform sampler2D uNormal;
uniform sampler2D uDiffuse;
uniform sampler2D uSpecular;

const float pi = 3.14159265;

struct Light {
	vec3 pos_v; // Viewspace position
	vec3 flux;
};

uniform Light[64] uLights;
uniform int uNumLights;

varying vec2 vTextureCoord;

// also +ve
float read_depth() {
	const float c = 0.01;
	float fc = 1.0 / log(uZFar * c + 1.0);
	// gl_FragDepth = log(z * c + 1.0) * fc;
	return (exp(texture2D(uDepth, vTextureCoord).r / fc) - 1.0) / c;
}

void main() {
	// view-space depth (+ve)
	float depth_v = read_depth();
	// view-space near plane ray intersection
	vec4 pos0_vh = gl_ProjectionMatrixInverse * vec4((vTextureCoord * 2.0 - 1.0), -1.0, 1.0);
	vec3 pos0_v = (pos0_vh / pos0_vh.w).xyz;
	// view-space 'far' plane ray intersection
	// @josh i hadn't tested this with insane far planes
	// it would be nice if we could just use the far plane, but application of the
	// inverse projection matrix results in precision problems. so, we let the
	// application decide what z (ndc) will be used for unprojection.
	vec4 pos1_vh = gl_ProjectionMatrixInverse * vec4((vTextureCoord * 2.0 - 1.0), uZUnproject, 1.0);
	vec3 pos1_v = (pos1_vh / pos1_vh.w).xyz;
	// view-space ray direction
	vec3 dir_v = normalize(pos1_v - pos0_v);
	// view-space fragment position
	vec3 pos_v = pos0_v + dir_v * ((depth_v + pos0_v.z) / -dir_v.z);
	// view-space normal
	vec3 norm_v = texture2D(uNormal, vTextureCoord).xyz;
	// fragment material properties
	vec3 diffuse = texture2D(uDiffuse, vTextureCoord).rgb;
	vec3 specular = texture2D(uSpecular, vTextureCoord).rgb;

	// radiance
	vec3 l = vec3(0.0);

	// ambient hack
	l += diffuse * 0.05;

	for (int i = 0; i < uNumLights; ++i) {
		// direction & distance from fragment to light
		vec3 ldir_v = uLights[i].pos_v - pos_v;
		float d = dot(ldir_v, ldir_v);
		ldir_v = normalize(ldir_v);
		// irradiance from light
		vec3 e0 = uLights[i].flux / pow(d, 2.0);
		// lambert diffuse radiance leaving surface
		l += e0 * max(0.0, dot(ldir_v, norm_v)) * diffuse / pi;
		// for the purposes of specular, we'll assume the light is a disc
		// light radius
		float lrad = 0.05;
		// light solid angle
		// https://en.wikipedia.org/wiki/Solid_angle
		float lsa = 2.0 * pi * (1.0 - cos(atan(lrad / d)));
		// radiance from light (preventing div-by-0)
		vec3 l0 = e0 / (lsa + 0.000001);
		// reflection direction
		vec3 rdir_v = reflect(-ldir_v, norm_v);
		// phong specular radiance leaving surface
		// TODO shininess as uniform
		l += l0 * specular * pow(max(0.0, dot(rdir_v, -dir_v)), 500.0);
	}

	// simple tonemapping for HDR
	// TODO exposure as uniform
	const float exposure = 15.0;
	gl_FragColor.rgb = 1.0 - exp(-exposure * l);

	gl_FragColor.a = 1.0;

	//ugh
	// gl_FragColor = vec4((0.8*norm_v.z + 0.2) * diffuse, 1.0);
	// gl_FragColor = vec4(diffuse, 1.0);
	// gl_FragColor = vec4(abs(norm_v), 1.0);

}
