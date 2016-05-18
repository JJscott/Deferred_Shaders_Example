#version 120

uniform float uZFar;

uniform sampler2D uDepth;
uniform sampler2D uNormal;
uniform sampler2D uDiffuse;
uniform sampler2D uSpecular;


struct Light {
	vec3 pos_v; // Viewspace position
	vec3 flux;
};

uniform Light[64] uLights;


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
	// view-space far plane ray intersection
	vec4 pos1_vh = gl_ProjectionMatrixInverse * vec4((vTextureCoord * 2.0 - 1.0), 1.0, 1.0);
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


	vec3 color = vec3(0, 0, 0);
	for (int i = 0; i < 64; ++i) {
	// 	// float d = 1/distance(pos_v, uLights[i].pos_v);
	// 	// if (d == d){
			color += uLights[i].flux * diffuse;
	// 	// }
	}
	gl_FragColor = vec4(color, 1.0);

	//ugh
	// gl_FragColor = vec4((0.8*norm_v.z + 0.2) * diffuse, 1.0);
	// gl_FragColor = vec4(diffuse, 1.0);
	// gl_FragColor = vec4(abs(norm_v), 1.0);
}