#version 120

uniform float uZFar;
uniform float uZUnproject;

uniform float uExposure;

uniform sampler2D uDepth;
uniform sampler2D uNormal;
uniform sampler2D uDiffuse;
uniform sampler2D uSpecular;

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


// vec3 lambert(vec3 lflux, vec3 ldir, vec3 norm, vec3 vdir) {



// }



vec3 lambertPhong(vec3 lirrad, vec3 lrad, vec3 ldir, vec3 norm, vec3 vdir, vec3 diffuse, vec3 specular, float shininess) {
		vec3 l = vec3(0.0);

		// lambert diffuse radiance leaving surface
		l += lirrad * max(0.0, dot(ldir, norm)) * diffuse / pi;
		// reflection direction
		vec3 rdir = reflect(-ldir, norm);
		// phong specular radiance leaving surface
		// l += lrad * specular * pow(max(0.0, dot(rdir, -vdir)), shininess);

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
	// 
	vec3 norm_v = texture2D(uNormal, vTextureCoord).xyz;

	// fragment material properties
	// 
	vec3 diffuse = texture2D(uDiffuse, vTextureCoord).rgb;
	vec3 specular = texture2D(uSpecular, vTextureCoord).rgb;
	float shininess = texture2D(uSpecular, vTextureCoord).a;




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
		vec3 irradiance = uLights[i].flux / pow(d, 2.0);

		// Radiance from light
		float lrad = 0.01; // assume the light is a disc (for specular) radius 10mm
		// float lsa = 2.0 * pi * (1.0 - cos(atan(lrad / d))); // light solid angle

		float lsa = 2.0 * pi * (1.0 - cos_atan(lrad / d)); // light solid angle


		vec3 radiance = irradiance / (lsa + 0.000001); // radiance from light (preventing div-by-0)

		// Add the result of radiance from this light
		l += lambertPhong(irradiance, radiance, ldir_v, norm_v, dir_v, diffuse, specular, shininess);
	}

	// simple tonemapping for HDR
	gl_FragColor.rgb = 1.0 - exp(-uExposure * l);

	gl_FragColor.a = 1.0;

}
