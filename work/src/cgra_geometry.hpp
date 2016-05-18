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

#pragma once

#include "cgra_math.hpp"
#include "opengl.hpp"

namespace cgra {

	inline void cgraSphere(float radius, int slices = 10, int stacks = 10, bool wire = false) {
		assert(slices > 0 && stacks > 0 && radius > 0);

		// set wire mode if needed
		if (wire) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		int dualslices = slices * 2;

		// precompute sin/cos values for the range of phi
		std::vector<float> u_texture_vector;
		std::vector<float> sin_phi_vector;
		std::vector<float> cos_phi_vector;

		for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {
			float u = float(slice_count) / dualslices;
			u_texture_vector.push_back(u);

			float phi = 2 * math::pi() * u;
			sin_phi_vector.push_back(std::sin(phi));
			cos_phi_vector.push_back(std::cos(phi));
		}


		// precompute the normalized coordinates of sphere
		std::vector<vec3> verts;
		std::vector<vec2> uvs;

		for (int stack_count = 0; stack_count <= stacks; ++stack_count) {
			float v = float(stack_count) / stacks;
			float theta = math::pi() * v;
			float sin_theta = std::sin(theta);
			float cos_theta = std::cos(theta);

			for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {
				uvs.push_back(vec2(u_texture_vector[slice_count], v));
				verts.push_back(vec3(
					sin_theta*cos_phi_vector[slice_count],
					sin_theta*sin_phi_vector[slice_count],
					cos_theta));
			}
		}

		// use triangle strips to display each stack of the sphere
		for (int stack_count = 0; stack_count < stacks; ++stack_count) {
			glBegin(GL_TRIANGLE_STRIP);

			for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {

				vec2 &th = uvs[slice_count + stack_count*(dualslices + 1)];
				vec2 &tl = uvs[slice_count + (stack_count + 1)*(dualslices + 1)];

				vec3 &nh = verts[slice_count + stack_count*(dualslices + 1)];
				vec3 &nl = verts[slice_count + (stack_count + 1)*(dualslices + 1)];

				vec3 ph = nh*radius;
				vec3 pl = nl*radius;

				glTexCoord2f(th.x, th.y);
				glNormal3f(nh.x, nh.y, nh.z);
				glVertex3f(ph.x, ph.y, ph.z);

				glTexCoord2f(tl.x, tl.y);
				glNormal3f(nl.x, nl.y, nl.z);
				glVertex3f(pl.x, pl.y, pl.z);
			}

			glEnd();
		}

		// reset mode
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}


	inline void cgraCylinder(float base_radius, float top_radius, float height, int slices = 10, int stacks = 10, bool wire = false) {
		assert(slices > 0 && stacks > 0 && (base_radius > 0 || base_radius > 0) && height > 0);

		// set wire mode if needed
		if (wire) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		int dualslices = slices * 2;

		// precompute sin/cos values for the range of phi
		std::vector<float> u_texture_vector;
		std::vector<float> sin_phi_vector;
		std::vector<float> cos_phi_vector;

		for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {
			float u = float(slice_count) / dualslices;
			u_texture_vector.push_back(u);

			float phi = 2 * math::pi() * u;
			sin_phi_vector.push_back(std::sin(phi));
			cos_phi_vector.push_back(std::cos(phi));
		}


		// precompute the coordinates and normals of cylinder
		std::vector<vec3> verts;
		std::vector<vec3> norms;
		std::vector<vec2> uvs;

		// thanks ben, you shall forever be immortalized
		float bens_theta = math::pi() / 2 * std::atan((base_radius - top_radius) / height);
		float sin_bens_theta = std::sin(bens_theta);
		float cos_bens_theta = std::cos(bens_theta);

		for (int stack_count = 0; stack_count <= stacks; ++stack_count) {
			float t = float(stack_count) / stacks;
			float z = height * t;
			float width = base_radius + (top_radius - base_radius) * t;

			for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {

				verts.push_back(vec3(
					width * cos_phi_vector[slice_count],
					width * sin_phi_vector[slice_count],
					z));

				norms.push_back(vec3(
					cos_bens_theta * cos_phi_vector[slice_count],
					cos_bens_theta * sin_phi_vector[slice_count],
					sin_bens_theta));

				uvs.push_back(vec2(u_texture_vector[slice_count], t));
			}
		}

		// use triangle strips to display each stack of the cylinder
		for (int stack_count = 0; stack_count < stacks; ++stack_count) {
			glBegin(GL_TRIANGLE_STRIP);

			for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {

				vec2 &th = uvs[slice_count + stack_count*(dualslices + 1)];
				vec2 &tl = uvs[slice_count + (stack_count + 1)*(dualslices + 1)];

				vec3 &ph = verts[slice_count + stack_count*(dualslices + 1)];
				vec3 &pl = verts[slice_count + (stack_count + 1)*(dualslices + 1)];

				vec3 &nh = norms[slice_count + stack_count*(dualslices + 1)];
				vec3 &nl = norms[slice_count + (stack_count + 1)*(dualslices + 1)];

				glTexCoord2f(th.x, th.y);
				glNormal3f(nh.x, nh.y, nh.z);
				glVertex3f(ph.x, ph.y, ph.z);

				glTexCoord2f(tl.x, tl.y);
				glNormal3f(nl.x, nl.y, nl.z);
				glVertex3f(pl.x, pl.y, pl.z);
			}

			glEnd();
		}

		// cap off the top and bottom of the cylinder
		if (base_radius > 0) {
			glBegin(GL_TRIANGLE_FAN);
			glTexCoord2f(0, 0);
			glNormal3f(0, 0, -1);
			glVertex3f(0, 0, 0);

			for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {
				vec3 &p = verts[slice_count];
				glVertex3f(p.x, p.y, p.z);
			}
			glEnd();
		}

		if (top_radius > 0) {
			glBegin(GL_TRIANGLE_FAN);
			glTexCoord2f(1, 1);
			glNormal3f(0, 0, 1);
			glVertex3f(0, 0, height);

			for (int slice_count = dualslices; slice_count >= 0; --slice_count) {
				vec3 &p = verts[slice_count + (stacks)*(dualslices + 1)];
				glVertex3f(p.x, p.y, p.z);
			}
			glEnd();
		}

		// reset mode
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}


	inline void cgraCone(float base_radius, float height, int slices = 10, int stacks = 10, bool wire = false) {
		cgraCylinder(base_radius, 0, height, slices, stacks, wire);
	}
}