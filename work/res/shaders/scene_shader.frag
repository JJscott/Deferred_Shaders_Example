#version 120

uniform float uZFar;

uniform bool uEmissive;
uniform vec3 uDiffuse;
uniform vec3 uSpecular;
uniform float uShininess;

varying vec3 vPosition;
varying vec3 vNormal;

// depth_v should be +ve
// so like: write_depth(-vPosition.z);
void write_log_depth(float depth_v) {
	const float c = 0.01;
	float fc = 1.0 / log(uZFar * c + 1.0);
	gl_FragDepth = log(depth_v * c + 1.0) * fc;
}

void write_depth(float depth_v) {
	gl_FragDepth = depth_v/uZFar;
}

void main() {
	// write_depth(-vPosition.z);
	write_log_depth(-vPosition.z);
	
	// Normal
	gl_FragData[0].rgb = normalize(vNormal);

	// Diffuse and emissive flag
	gl_FragData[1].rgb = uDiffuse;
	gl_FragData[1].a = float(uEmissive);

	// Specular and shininess
	gl_FragData[2].rgb = uSpecular;
	gl_FragData[2].a = uShininess;
}
