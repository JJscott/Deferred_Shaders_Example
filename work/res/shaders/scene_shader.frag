#version 120

uniform float uZFar;

uniform vec3 uDiffuse;
uniform vec3 uSpecular;

varying vec3 vPosition;
varying vec3 vNormal;

// depth_v should be +ve
// so like: write_depth(-vPostion.z);
void write_depth(float depth_v) {
	const float c = 0.01;
	float fc = 1.0 / log(uZFar * c + 1.0);
	gl_FragDepth = log(depth_v * c + 1.0) * fc;
}

void main() {
	write_depth(-vPosition.z);
	gl_FragData[0].rgb = normalize(vNormal);
	gl_FragData[1].rgb = uDiffuse;
	gl_FragData[2].rgb = uSpecular;
}