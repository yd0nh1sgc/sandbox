
#include <stdlib.h>
#include <iostream>
#include <algorithm>
#include <cctype>  // std::tolower
#include <cstdarg>   // For va_list, va_start, ...
#include <cstdio>    // For vsnprintf
// in project properties, add "../include" to the vc++ directories include path

#include "vecmath.h"
#include "vecmath.cpp"  // need to drag in this for now, will inline whats needed later.
#include "glwin.h"  // minimal opengl for windows setup wrapper

#include "hull.h"

float g_pitch, g_yaw;
int g_mouseX, g_mouseY;
int g_vlimit = 64;

std::vector<float3> g_verts;
std::vector<int3> g_tris;
int g_vcount = 20;

inline float randf(){ return static_cast<float>(rand()) / static_cast<float>(RAND_MAX); }

float3 vrand(){ return {randf(),randf(),randf()}; }
int max_element(const int3& v) { return std::max(std::max(v.x, v.y),v.z); }

void Init()
{
	g_vlimit = 64;
	g_verts.resize(0);
	for (int i = 0; i < g_vcount; i++)
		g_verts.push_back(vrand() - float3(0.5f, 0.5f, 0.5f));
	g_tris = ::calchull(g_verts.data(), g_verts.size(), g_vlimit);
	g_vlimit = 0;
	for (auto t : g_tris)
		g_vlimit = std::max(g_vlimit, 1+ max_element(t));
}


void OnKeyboard(unsigned char key, int x, int y)
{
	switch (std::tolower(key))
	{
	case ' ':
		Init();
		break;
	case 27:   // ESC
	case 'q':
		exit(0);
		break;
	case 'w':
	case 's':
		g_vlimit+=(key=='s')?-1:1;  g_vlimit = std::max(4, g_vlimit);
		g_tris = ::calchull(g_verts.data(), g_verts.size(), g_vlimit);
		break;
	default:
		std::cout << "unassigned key (" << (int)key << "): '" << key << "'\n";
		break;
	}
}


void glNormal3fv(const float3 &v) { glNormal3fv(&v.x); }
void glVertex3fv(const float3 &v) { glVertex3fv(&v.x); }
void glColor3fv (const float3 &v) { glColor3fv(&v.x);  }



int main(int argc, char *argv[])
{
	std::cout << "TestMath\n";

	Init();

	GLWin glwin("TestHull sample");
	glwin.keyboardfunc = OnKeyboard;
	while (glwin.WindowUp())
	{
		if (glwin.MouseState)  // on mouse drag 
		{
			g_yaw   += (glwin.MouseX - g_mouseX) * 0.3f;  // poor man's trackball
			g_pitch += (glwin.MouseY - g_mouseY) * 0.3f;
		}
		g_mouseX = glwin.MouseX;
		g_mouseY = glwin.MouseY;


		glPushAttrib(GL_ALL_ATTRIB_BITS);

		// Set up the viewport
		glViewport(0, 0, glwin.Width,glwin.Height);
		glClearColor(0.1f, 0.1f, 0.15f, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Set up matrices
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		gluPerspective(60, (double)glwin.Width/ glwin.Height, 0.01, 10);

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		gluLookAt(0, -2, 1, 0, 0, 0, 0, 0, 1);

		glRotatef(g_pitch, 1, 0, 0);
		glRotatef(g_yaw, 0, 0, 1);

		glEnable(GL_DEPTH_TEST);
		//glEnable(GL_TEXTURE_2D);

		glDisable(GL_BLEND);
		glPointSize(4);
		glBegin(GL_POINTS);
		glColor3f(0, 1, 0);
		for (auto &v : g_verts)
		{
			glColor3fv((&v - g_verts.data() < g_vlimit) ? float3(0.75, 1, 0) : float3(0.75, 0, 0));
			glVertex3fv(v);
		}
		glEnd();

		glEnable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		glBegin(GL_TRIANGLES);
		glColor4f(1, 1, 1, 0.25f);
		for (auto t : g_tris)
		{
			glNormal3fv(TriNormal(g_verts[t[0]], g_verts[t[1]], g_verts[t[2]]));
			for (int j = 0; j < 3; j++)
				glVertex3fv(g_verts[t[j]]);
		}
		glEnd();


		// Restore state
		glPopMatrix();  //should be currently in modelview mode
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glPopAttrib();
		glMatrixMode(GL_MODELVIEW);  

		glwin.PrintString("Press q to quit.     Spacebar new pointcloud.", 5, 1);
		glwin.PrintString("Keys w,s increase/decrease vlimit and recalc. ", 5, 2);
		char buf[256];
		sprintf(buf, "vlimit %d tris %d  y,p= %f,%f", g_vlimit, g_tris.size(), g_yaw, g_pitch);
		glwin.PrintString(buf, 5, 3);

		glwin.SwapBuffers();
	}


	std::cout << "\n";
	return 0;
}

