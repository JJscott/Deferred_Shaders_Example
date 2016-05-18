#version 120

uniform vec3 uDiffuse;
uniform vec3 uSpecular;

varying vec3 vPosition;
varying vec3 vNormal;

void main() {
	vNormal = gl_NormalMatrix * gl_Normal;

	vec4 new_position = gl_Vertex + vec4(3, 0, 0, 0);


	vPosition = (gl_ModelViewMatrix * new_position).xyz;
	gl_Position = gl_ModelViewProjectionMatrix * new_position;
}