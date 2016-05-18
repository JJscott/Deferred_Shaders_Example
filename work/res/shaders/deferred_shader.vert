#version 120

uniform float uZFar;

uniform sampler2D uDepth;
uniform sampler2D uNormal;
uniform sampler2D uDiffuse;
uniform sampler2D uSpecular;

varying vec2 vTextureCoord;

void main() {
	vTextureCoord = gl_Vertex.xy * 0.5 + 0.5;
	gl_Position = gl_Vertex;
}
