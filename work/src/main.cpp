//---------------------------------------------------------------------------
//
// Copyright (c) 2016 Taehyun Rhee, Joshua Scott, Ben Allen
//
// This software is provided 'as-is' for assignment of COMP308 in ECS,
// Victoria University of Wellington, without any express or implied warranty. 
// In no event will the authors be held liable for any damages arising from
// the use of this software.
//
// The contents of this file may not be copied or duplicated in any form
// without the prior permission of its owner.
//
//----------------------------------------------------------------------------

#include <cmath>
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <vector>

#include "cgra_geometry.hpp"
#include "cgra_math.hpp"
#include "simple_image.hpp"
#include "simple_shader.hpp"
#include "simple_gui.hpp"
#include "opengl.hpp"

using namespace std;
using namespace cgra;

// Window
//
GLFWwindow* g_window;


// Projection values
// 
float g_fovy = 60.0;
float g_znear = 0.1;
float g_zfar = 20000000.0;


// Mouse controlled Camera values
//
bool g_leftMouseDown = false;
vec2 g_mousePosition;
float g_pitch = 0;
float g_yaw = 0;
float g_zoom = 1.0;


// Buffers
//
ivec2 g_last_frame_size;
GLuint g_fbo_scene = 0;
GLuint g_tex_scene_depth = 0;
GLuint g_tex_scene_normal = 0;
GLuint g_tex_scene_diffuse = 0;
GLuint g_tex_scene_specular = 0;


// Shaders
//
GLuint g_scene_shader;
GLuint g_deferred_shader;



// Lights
struct Light {
	vec3 acl_w; // acceleration
	vec3 vel_w; // velocity
	vec3 pos_w; // position
	vec3 flux;
	Light(vec3 p, vec3 f) : pos_w(p), flux(f), vel_w(0), acl_w(0) { }
};

vector<Light> g_lights;



// Other Controllable variables
float g_exposure = 15.0;

float g_flux_mult = 15.0;

// float g_bloom_thresh = 1.0;
int g_num_lights = 4;
bool g_simulate_lights = false;
float g_min_light_speed = 0.01;
float g_max_light_speed = 0.1;

bool g_draw_lights = false;

vec3 g_zone_hize(30, 10, 30);
vec3 g_zone_position(0, 10, 0);




// Mouse Button callback
// Called for mouse movement event on since the last glfwPollEvents
//
void cursorPosCallback(GLFWwindow* win, double xpos, double ypos) {
	// cout << "Mouse Movement Callback :: xpos=" << xpos << "ypos=" << ypos << endl;
	if (g_leftMouseDown) {
		g_yaw -= g_mousePosition.x - xpos;
		g_pitch -= g_mousePosition.y - ypos;
	}
	g_mousePosition = vec2(xpos, ypos);
}


// Mouse Button callback
// Called for mouse button event on since the last glfwPollEvents
//
void mouseButtonCallback(GLFWwindow *win, int button, int action, int mods) {
	// cout << "Mouse Button Callback :: button=" << button << "action=" << action << "mods=" << mods << endl;
	SimpleGUI::mouseButtonCallback(win, button, action, mods);
	if(ImGui::IsMouseHoveringAnyWindow()) return; // block input with gui 

	if (button == GLFW_MOUSE_BUTTON_LEFT)
		g_leftMouseDown = (action == GLFW_PRESS);
}


// Scroll callback
// Called for scroll event on since the last glfwPollEvents
//
void scrollCallback(GLFWwindow *win, double xoffset, double yoffset) {
	// cout << "Scroll Callback :: xoffset=" << xoffset << "yoffset=" << yoffset << endl;
	SimpleGUI::scrollCallback(win, xoffset, yoffset);
	if(ImGui::IsMouseHoveringAnyWindow()) return; // block input with gui

	g_zoom -= yoffset * g_zoom * 0.2;
}


// Keyboard callback
// Called for every key event on since the last glfwPollEvents
//
void keyCallback(GLFWwindow *win, int key, int scancode, int action, int mods) {
	// cout << "Key Callback :: key=" << key << "scancode=" << scancode
	// 	<< "action=" << action << "mods=" << mods << endl;
	SimpleGUI::keyCallback(win, key, scancode, action, mods);
	if(ImGui::IsAnyItemActive()) return; // block input with gui

}


// Character callback
// Called for every character input event on since the last glfwPollEvents
//
void charCallback(GLFWwindow *win, unsigned int c) {
	// cout << "Char Callback :: c=" << char(c) << endl;
	SimpleGUI::charCallback(win, c);
	if(ImGui::IsAnyItemActive()) return; // block input with gui
}



// An example of how to load a shader from a hardcoded location
//
void initShader() {
	g_scene_shader = makeShaderProgramFromFile(
		{GL_VERTEX_SHADER, GL_FRAGMENT_SHADER },
		{ "./work/res/shaders/scene_shader.vert", "./work/res/shaders/scene_shader.frag" }
	);

	g_deferred_shader = makeShaderProgramFromFile(
		{GL_VERTEX_SHADER, GL_FRAGMENT_SHADER },
		{ "./work/res/shaders/deferred_shader.vert", "./work/res/shaders/deferred_shader.frag" }
	);
}


void addLight() {
	// creation
	vec3 position = (vec3::random(-20, 20) + vec3(0, 20, 0)) * vec3(1, 0.3, 1);
	vec3 flux = normalize(vec3::random(0, 1));
	Light l = Light(position, flux);

	// intial velocity
	vec3 velocity = 0.01 * normalize(vec3::random(-1, 1));
	l.vel_w = velocity;

	g_lights.push_back(l);
}


void updateLights() {
	while(g_lights.size() < g_num_lights)
		addLight();
	while(g_lights.size() > g_num_lights)
		g_lights.pop_back();

	if(g_simulate_lights) {

		vec3 zone_min = g_zone_position - g_zone_hize;
		vec3 zone_max = g_zone_position + g_zone_hize;

		for (Light &l : g_lights) {

			// apply acceleration if outside zone
			vec3 accel;
			accel += 0.01 * mix(vec3(0.0), vec3(1.0), lessThanEqual(l.pos_w, zone_min));
			accel += 0.01 * mix(vec3(0.0), vec3(-1.0), greaterThanEqual(l.pos_w, zone_max));
			l.acl_w += accel;

			// update velocities
			l.vel_w += l.acl_w;
			l.acl_w = vec3(0);

			// clamp speed to min max speed
			float speed = length(l.vel_w);
			if (speed == 0) l.vel_w = vec3(1) * g_min_light_speed;
			else if (speed < g_min_light_speed) l.vel_w *= g_min_light_speed/speed;
			else if (speed > g_max_light_speed) l.vel_w *= g_max_light_speed/speed;

			// update position
			l.pos_w += l.vel_w;

		}
	}
}


// Sets up where the camera is in the scene
// 
void setupCamera(int width, int height) {
	// Set up the projection matrix
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(g_fovy, width / float(height), g_znear, g_zfar);

	// Set up the view part of the model view matrix
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glTranslatef(0, 0, -10 * g_zoom);
	glRotatef(g_pitch, 1, 0, 0);
	glRotatef(g_yaw, 0, 1, 0);
}


// Helper methods to create a FrameBuffer Object in the right size
// 
void ensureFBO(int w, int h) {
	if (ivec2(w, h) == g_last_frame_size) return;

	if (!g_fbo_scene) glGenFramebuffers(1, &g_fbo_scene);
	
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_fbo_scene);
	glActiveTexture(GL_TEXTURE0);

	// If texture target doesn't exist, create it
	if (!g_tex_scene_depth) {
		glGenTextures(1, &g_tex_scene_depth);
		glBindTexture(GL_TEXTURE_2D, g_tex_scene_depth);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, g_tex_scene_depth, 0);

	// Otherwise, bind the existing target with the right size
	} else {
		glBindTexture(GL_TEXTURE_2D, g_tex_scene_depth);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	}

	if (!g_tex_scene_normal) {
		glGenTextures(1, &g_tex_scene_normal);
		glBindTexture(GL_TEXTURE_2D, g_tex_scene_normal);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
		glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, g_tex_scene_normal, 0);
	} else {
		glBindTexture(GL_TEXTURE_2D, g_tex_scene_normal);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
	}

	if (!g_tex_scene_diffuse) {
		glGenTextures(1, &g_tex_scene_diffuse);
		glBindTexture(GL_TEXTURE_2D, g_tex_scene_diffuse);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
		glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, g_tex_scene_diffuse, 0);
	} else {
		glBindTexture(GL_TEXTURE_2D, g_tex_scene_diffuse);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
	}

	if (!g_tex_scene_specular) {
		glGenTextures(1, &g_tex_scene_specular);
		glBindTexture(GL_TEXTURE_2D, g_tex_scene_specular);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
		glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, g_tex_scene_specular, 0);
	} else {
		glBindTexture(GL_TEXTURE_2D, g_tex_scene_specular);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
	}

	GLenum bufs[] { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	glDrawBuffers(3, bufs);

	glBindTexture(GL_TEXTURE_2D, 0);

	g_last_frame_size = ivec2(w, h);
}



//
//
void renderSceneBuffer(int width, int height) {
	ensureFBO(width, height);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_fbo_scene);
	glViewport(0, 0, width, height);

	// Clear all scene buffers to 0
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);


	// Enable flags for scene buffer rendering
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_NORMALIZE);

	setupCamera(width, height);

	glUseProgram(g_scene_shader);

	// Render scene 
	//
	glUniform1f(glGetUniformLocation(g_scene_shader, "uZFar"), g_zfar);


	// Golden sphere
	vec3 gold_spec_chroma { 0.9f, 0.8f, 0.6f };
	float gold_spec_ratio = 0.9;
	float gold_shininess = 1000.0;

	glUniform1i(glGetUniformLocation(g_scene_shader, "uEmissive"), false);
	glUniform3fv(glGetUniformLocation(g_scene_shader, "uDiffuse"), 1, (pow(gold_spec_chroma, vec3(2)) * (1-gold_spec_ratio)).dataPointer());
	glUniform3fv(glGetUniformLocation(g_scene_shader, "uSpecular"), 1, (gold_spec_chroma * gold_spec_ratio).dataPointer());
	glUniform1f(glGetUniformLocation(g_scene_shader, "uShininess"), gold_shininess);

	glPushMatrix();
		glTranslatef(0,4,0);
		cgraSphere(4.0, 100, 100);
	glPopMatrix();



	// White pillar
	vec3 white_spec_chroma { 0.9f, 0.8f, 0.6f };
	float white_spec_ratio = 0.5;
	float white_shininess = 1.0;

	glUniform1i(glGetUniformLocation(g_scene_shader, "uEmissive"), false);
	glUniform3fv(glGetUniformLocation(g_scene_shader, "uDiffuse"), 1, (pow(white_spec_chroma, vec3(2)) * (1-white_spec_ratio)).dataPointer());
	glUniform3fv(glGetUniformLocation(g_scene_shader, "uSpecular"), 1, (white_spec_chroma * white_spec_ratio).dataPointer());
	glUniform1f(glGetUniformLocation(g_scene_shader, "uShininess"), white_shininess);

	glPushMatrix();
		glTranslatef(15,0,15);
		glRotatef(-90, 1, 0, 0);
		cgraCylinder(2.0, 2.0, 20, 100, 100);
	glPopMatrix();




	// Red Cone
	vec3 red_spec_chroma { 0.9f, 0.1f, 0.1f };
	float red_spec_ratio = 0.8;
	float red_shininess = 300.0;

	glUniform1i(glGetUniformLocation(g_scene_shader, "uEmissive"), false);
	glUniform3fv(glGetUniformLocation(g_scene_shader, "uDiffuse"), 1, (pow(red_spec_chroma, vec3(2)) * (1-red_spec_ratio)).dataPointer());
	glUniform3fv(glGetUniformLocation(g_scene_shader, "uSpecular"), 1, (red_spec_chroma * red_spec_ratio).dataPointer());
	glUniform1f(glGetUniformLocation(g_scene_shader, "uShininess"), red_shininess);

	glPushMatrix();
		glTranslatef(-15,0,15);
		glRotatef(-90, 1, 0, 0);
		cgraCone(3.0, 8.0, 100, 100);
	glPopMatrix();





	// Green bottom heavy cylinder
	vec3 green_spec_chroma { 0.1f, 0.9f, 0.1f };
	float green_spec_ratio = 0.5;
	float green_shininess = 100.0;

	glUniform1i(glGetUniformLocation(g_scene_shader, "uEmissive"), false);
	glUniform3fv(glGetUniformLocation(g_scene_shader, "uDiffuse"), 1, (pow(green_spec_chroma, vec3(2)) * (1-green_spec_ratio)).dataPointer());
	glUniform3fv(glGetUniformLocation(g_scene_shader, "uSpecular"), 1, (green_spec_chroma * green_spec_ratio).dataPointer());
	glUniform1f(glGetUniformLocation(g_scene_shader, "uShininess"), green_shininess);

	glPushMatrix();
		glTranslatef(15,0,-15);
		glRotatef(-90, 1, 0, 0);
		cgraCylinder(4.0, 1.0, 20, 100, 100);
	glPopMatrix();



	// Blue top heavy cylinder
	vec3 blue_spec_chroma { 0.1f, 0.1f, 0.9f };
	float blue_spec_ratio = 0.2;
	float blue_shininess = 1.0;

	glUniform1i(glGetUniformLocation(g_scene_shader, "uEmissive"), false);
	glUniform3fv(glGetUniformLocation(g_scene_shader, "uDiffuse"), 1, (pow(blue_spec_chroma, vec3(2)) * (1-blue_spec_ratio)).dataPointer());
	glUniform3fv(glGetUniformLocation(g_scene_shader, "uSpecular"), 1, (blue_spec_chroma * blue_spec_ratio).dataPointer());
	glUniform1f(glGetUniformLocation(g_scene_shader, "uShininess"), blue_shininess);

	glPushMatrix();
		glTranslatef(-15,0,-15);
		glRotatef(-90, 1, 0, 0);
		cgraCylinder(1.5, 3.0, 20, 100, 100);
	glPopMatrix();





	// Silver floor
	vec3 silver_spec_chroma { 0.8f };
	float silver_spec_ratio = 0.5;
	float silver_shininess = 100.0;

	glUniform1i(glGetUniformLocation(g_scene_shader, "uEmissive"), false);
	glUniform3fv(glGetUniformLocation(g_scene_shader, "uDiffuse"), 1, (pow(silver_spec_chroma, vec3(2)) * silver_spec_ratio).dataPointer());
	glUniform3fv(glGetUniformLocation(g_scene_shader, "uSpecular"), 1, (silver_spec_chroma * silver_spec_ratio).dataPointer());
	glUniform1f(glGetUniformLocation(g_scene_shader, "uShininess"), silver_shininess);

	glPushMatrix();
		glTranslatef(0,-0.01,0);
		glBegin(GL_TRIANGLES);
		glNormal3f(0, 1.0, 0);
		glVertex3f(-40.0, 0, -40.0);
		glVertex3f( 40.0, 0, -40.0);
		glVertex3f(-40.0, 0,  40.0);
		glVertex3f( 40.0, 0, -40.0);
		glVertex3f( 40.0, 0,  40.0);
		glVertex3f(-40.0, 0,  40.0);
		glEnd();
		glFlush();
	glPopMatrix();





	// Big grey sphere
	glPushMatrix();
		vec3 grey(0.8, 0.8, 0.8);
		glTranslatef(0,0, 4000000);
		glUniform1i(glGetUniformLocation(g_scene_shader, "uEmissive"), false);
		glUniform3fv(glGetUniformLocation(g_scene_shader, "uDiffuse"), 1, grey.dataPointer());
		glUniform3fv(glGetUniformLocation(g_scene_shader, "uSpecular"), 1, grey.dataPointer());
		glUniform1f(glGetUniformLocation(g_scene_shader, "uShininess"), 1.0);
		cgraSphere(1500000, 100, 100);
	glPopMatrix();






	//Draw Lights
	if (g_draw_lights) {
		for (const Light &l : g_lights) {
			glPushMatrix();
				glTranslatef(l.pos_w.x, l.pos_w.y, l.pos_w.z);
				glUniform1i(glGetUniformLocation(g_scene_shader, "uEmissive"), true);
				glUniform3fv(glGetUniformLocation(g_scene_shader, "uDiffuse"), 1, normalize(l.flux).dataPointer());
				cgraSphere(0.1);
			glPopMatrix();
		}
	}


	glUseProgram(0);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_NORMALIZE);
}




// 
//
void renderDeferred(int width, int height) {

	// Set to draw to the screen frame buffer
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glViewport(0, 0, width, height);


	// Use the deferred shading program
	// 
	glUseProgram(g_deferred_shader);
	glDisable(GL_DEPTH_TEST);


	// Upload the far plane
	// 
	glUniform1f(glGetUniformLocation(g_deferred_shader, "uZFar"), g_zfar);

	// Get the projection matrix to work out the plane to project onto
	mat4 proj;
	glGetFloatv(GL_PROJECTION_MATRIX, proj.dataPointer());
	// Pick a z for unprojection (nearly arbitrary)
	vec4 unproj = proj * vec4(0, 0, -g_znear * 10, 1);
	glUniform1f(glGetUniformLocation(g_deferred_shader, "uZUnproject"), unproj.z / unproj.w);


	// Upload Exposure
	// 
	glUniform1f(glGetUniformLocation(g_deferred_shader, "uExposure"), g_exposure);


	// Upload the scene buffer textures
	// 
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_tex_scene_depth);
	glUniform1i(glGetUniformLocation(g_deferred_shader, "uDepth"), 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, g_tex_scene_normal);
	glUniform1i(glGetUniformLocation(g_deferred_shader, "uNormal"), 1);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, g_tex_scene_diffuse);
	glUniform1i(glGetUniformLocation(g_deferred_shader, "uDiffuse"), 2);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, g_tex_scene_specular);
	glUniform1i(glGetUniformLocation(g_deferred_shader, "uSpecular"), 3);



	// Setup the camera again, so the GL_MODELVIEW_MATRIX contains just the view matrix
	setupCamera(width, height);
	mat4 view;
	glGetFloatv(GL_MODELVIEW_MATRIX, view.dataPointer());


	// Upload lights
	// Positions must be in view-space
	//
	glUniform1i(glGetUniformLocation(g_deferred_shader, "uNumLights"), GLuint(g_lights.size()));

	for (size_t i = 0; i < g_lights.size(); ++i) {
		ostringstream ss;
		ss << "uLights[" << i << "].pos_v";
		glUniform3fv(glGetUniformLocation(g_deferred_shader, ss.str().c_str()), 1, (view * vec4(g_lights[i].pos_w, 1)).dataPointer());
		
		ss = ostringstream();
		ss << "uLights[" << i << "].flux";
		glUniform3fv(glGetUniformLocation(g_deferred_shader, ss.str().c_str()), 1, (g_flux_mult * g_lights[i].flux).dataPointer());
	}


	// Draw a triangle that covers the screen
	// This does the deferred shading pass
	glBegin(GL_TRIANGLES);
	glVertex3f(-1.0, -1.0, 0.0);
	glVertex3f(3.0, -1.0, 0.0);
	glVertex3f(-1.0, 3.0, 0.0);
	glEnd();
	glFlush();

	glUseProgram(0);
}


// Draw function
//
void render(int width, int height) {
	renderSceneBuffer(width, height);
	renderDeferred(width, height);
}


// Draw GUI function
//
void renderGUI() {
	ImGui::Begin("Debug");
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

	ImGui::Separator();

	ImGui::SliderFloat("Exposure", &g_exposure, 0.0, 100.0, "%.1f");
	ImGui::SliderFloat("Flux Multiplier", &g_flux_mult, 1.0, 100.0, "%.0f");


	ImGui::SliderInt("# of Lights", &g_num_lights, 0, 64);

	ImGui::Checkbox("Draw Lights", &g_draw_lights);
	ImGui::Checkbox("Simulate Lights", &g_simulate_lights);
	if (g_simulate_lights) {
		ImGui::SliderFloat("Min speed", &g_min_light_speed, 0.0, 1.0, "%.1f");
		if (ImGui::SliderFloat("Max speed", &g_max_light_speed, 0.0, 1.0, "%.1f")) {
			g_max_light_speed = std::max(g_min_light_speed, g_max_light_speed);
		}
	}

	ImGui::End();
}



// Forward decleration for cleanliness (Ignore)
void APIENTRY debugCallbackARB(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);


//Main program
// 
int main(int argc, char **argv) {

	// Initialize the GLFW library
	if (!glfwInit()) {
		cerr << "Error: Could not initialize GLFW" << endl;
		abort(); // Unrecoverable error
	}

	// Get the version for GLFW for later
	int glfwMajor, glfwMinor, glfwRevision;
	glfwGetVersion(&glfwMajor, &glfwMinor, &glfwRevision);

#ifndef NDEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif

	// Create a windowed mode window and its OpenGL context
	g_window = glfwCreateWindow(640, 480, "Hello World", nullptr, nullptr);
	if (!g_window) {
		cerr << "Error: Could not create GLFW window" << endl;
		abort(); // Unrecoverable error
	}

	// Make the g_window's context is current.
	// If we have multiple windows we will need to switch contexts
	glfwMakeContextCurrent(g_window);



	// Initialize GLEW
	// must be done after making a GL context current (glfwMakeContextCurrent in this case)
	glewExperimental = GL_TRUE; // required for full GLEW functionality for OpenGL 3.0+
	GLenum err = glewInit();
	if (GLEW_OK != err) { // Problem: glewInit failed, something is seriously wrong.
		cerr << "Error: " << glewGetErrorString(err) << endl;
		abort(); // Unrecoverable error
	}



	// Print out our OpenGL verisions
	cout << "Using OpenGL " << glGetString(GL_VERSION) << endl;
	cout << "Using GLEW " << glewGetString(GLEW_VERSION) << endl;
	cout << "Using GLFW " << glfwMajor << "." << glfwMinor << "." << glfwRevision << endl;



	// Attach input callbacks to g_window
	glfwSetCursorPosCallback(g_window, cursorPosCallback);
	glfwSetMouseButtonCallback(g_window, mouseButtonCallback);
	glfwSetScrollCallback(g_window, scrollCallback);
	glfwSetKeyCallback(g_window, keyCallback);
	glfwSetCharCallback(g_window, charCallback);



	// Enable GL_ARB_debug_output if available. Not nessesary, just helpful
	if (glfwExtensionSupported("GL_ARB_debug_output")) {
		// This allows the error location to be determined from a stacktrace
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		// Set the up callback
		glDebugMessageCallbackARB(debugCallbackARB, nullptr);
		glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, true);
		cout << "GL_ARB_debug_output callback installed" << endl;
	}
	else {
		cout << "GL_ARB_debug_output not available. No worries." << endl;
	}



	// Initialize IMGUI
	// Second argument is true if we dont need to use GLFW bindings for input
	// if set to false we must manually call the SimpleGUI callbacks when we
	// process the input.
	if (!SimpleGUI::init(g_window, false)) {
		cerr << "Error: Could not initialize IMGUI" << endl;
		abort();
	}




	// Initialize Geometry/Material/Lights
	initShader();

	// Loop until the user closes the window
	while (!glfwWindowShouldClose(g_window)) {

		// Make sure we draw to the WHOLE window
		int width, height;
		glfwGetFramebufferSize(g_window, &width, &height);
		glViewport(0, 0, width, height);


		// Update Scene
		updateLights();

		// Main Render
		render(width, height);

		// Render GUI on top
		SimpleGUI::newFrame();
		renderGUI();
		SimpleGUI::render();

		// Swap front and back buffers
		glfwSwapBuffers(g_window);

		// Poll for and process events
		glfwPollEvents();
	}

	glfwTerminate();
}






//-------------------------------------------------------------
// Fancy debug stuff
//-------------------------------------------------------------

// function to translate source to string
string getStringForSource(GLenum source) {

	switch (source) {
	case GL_DEBUG_SOURCE_API:
		return("API");
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
		return("Window System");
	case GL_DEBUG_SOURCE_SHADER_COMPILER:
		return("Shader Compiler");
	case GL_DEBUG_SOURCE_THIRD_PARTY:
		return("Third Party");
	case GL_DEBUG_SOURCE_APPLICATION:
		return("Application");
	case GL_DEBUG_SOURCE_OTHER:
		return("Other");
	default:
		return("n/a");
	}
}

// function to translate severity to string
string getStringForSeverity(GLenum severity) {

	switch (severity) {
	case GL_DEBUG_SEVERITY_HIGH:
		return("HIGH!");
	case GL_DEBUG_SEVERITY_MEDIUM:
		return("Medium");
	case GL_DEBUG_SEVERITY_LOW:
		return("Low");
	default:
		return("n/a");
	}
}

// function to translate type to string
string getStringForType(GLenum type) {
	switch (type) {
	case GL_DEBUG_TYPE_ERROR:
		return("Error");
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
		return("Deprecated Behaviour");
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
		return("Undefined Behaviour");
	case GL_DEBUG_TYPE_PORTABILITY:
		return("Portability Issue");
	case GL_DEBUG_TYPE_PERFORMANCE:
		return("Performance Issue");
	case GL_DEBUG_TYPE_OTHER:
		return("Other");
	default:
		return("n/a");
	}
}

// actually define the function
void APIENTRY debugCallbackARB(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei, const GLchar* message, GLvoid*) {
	if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

	cerr << endl; // extra space

	cerr << "Type: " <<
		getStringForType(type) << "; Source: " <<
		getStringForSource(source) << "; ID: " << id << "; Severity: " <<
		getStringForSeverity(severity) << endl;

	cerr << message << endl;

	if (type == GL_DEBUG_TYPE_ERROR) throw runtime_error(message);
}