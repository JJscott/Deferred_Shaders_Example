#version 120

uniform bool uEmissive;
uniform vec3 uDiffuse;
uniform vec3 uSpecular;
uniform float uShininess;

varying vec3 vPosition;
varying vec3 vNormal;

void main() {
	vNormal = gl_NormalMatrix * gl_Normal;
	vPosition = (gl_ModelViewMatrix * gl_Vertex).xyz;
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
