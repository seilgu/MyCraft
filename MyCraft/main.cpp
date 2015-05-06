 #pragma comment (lib, "glew32s.lib")

#ifndef APPLE
	#include <windows.h>
	#include <process.h>
	#include <commctrl.h>
#endif

#include <stdio.h>
#include <time.h>
#include <math.h>

#include "init.h"

#include "asmlibran.h"

int currX, currY;
int prevX, prevY;

#ifdef APPLE
#include "common.h"
#include <GLUT/glut.h>
#endif


#ifndef APPLE
// WINAPI event procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
		return 0;
	case WM_ACTIVATE:
		if (!HIWORD(wParam))
			active = TRUE;
		else
			active = FALSE;
		return 0;
	case WM_SIZE:
		_width = LOWORD(lParam);
		_height = HIWORD(lParam);
		ReSizeGLScene(_width, _height);
		s_Texture->CreateFBOTextures();
		return 0;
	case WM_MOUSEMOVE:
		return 0;
	case WM_LBUTTONDOWN: mouseDown = true;
		return 0;
	case WM_LBUTTONUP: mouseDown = false;
		return 0;
	case WM_KEYDOWN:
		switch (wParam) {
		case VK_ESCAPE: captureMouse = 1 - captureMouse;
			if (captureMouse == 1) {
				ShowCursor(false);
			} else {
				ShowCursor(true);
			}
			break;
		case VK_CONTROL:
			controlKey = true;
			break;
		default: keys[wParam] = TRUE;
			break;
		}
		return 0;
	case WM_KEYUP:
		switch (wParam) {
		case VK_ESCAPE: break;
		case VK_CONTROL:
			controlKey = false;
			break;
		default: keys[wParam] = FALSE; break;
		}
		return 0;
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	};

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

// FPS

#else

POINT motion;
void GetCursorPos(POINT *cs)
{
	cs->x = motion.x;
	cs->y = motion.y;
}

void SetCursorPos(int x, int y)
{
	glutWarpPointer(x, y);
	captureMouse = 0;
}
void Motion(int x, int y)
{
	motion.x = x;
	motion.y = y;
	captureMouse = 1;

/*	int midX = WIDTH/2, midY = HEIGHT/2;
	m_Player->phi += -(x - midX)/400.0f;
	m_Player->theta += -(y - midY)/400.0f;
	glutWarpPointer(midX, midY);*/
}
void Key(unsigned char key, int x, int y) 
{ 
	fprintf(stderr, "key '%c' is pressed.\n", key);
	switch (key) { 
		case 27: 
			exit(0); 
		default:
			if (key>='a' && key <='z')
				keys[key-'a'+'A'] = true;
			else if (key>='A' && key <= 'Z')
				keys[key-'A'+'a'] = true;
	} 
}

#endif

float watern = 1.2f;

float3 lightpos[4];

void ProcessInput() {
	
	// Ugly mouse handling
	if (captureMouse) {
		POINT cs;
		GetCursorPos(&cs);
		m_Player->phi += -(cs.x - 500)/1000.0f;
		m_Player->theta += -(cs.y - 500)/1000.0f;
		SetCursorPos(500, 500);
	}

	if (mouseDown == true) {
		//mouseDown = false;

		int3 tmpId;
		int3 tmpOffset;
		int tmpSide;
		if (s_Render->FindBlock(m_Player->eyepos, m_Player->dir, 128, tmpId, tmpOffset, tmpSide) == 1) {
			if (controlKey == true) {
				switch (tmpSide) {
				case Render::PX: tmpOffset.x += 1; break;
				case Render::NX: tmpOffset.x -= 1; break;
				case Render::PY: tmpOffset.y += 1; break;
				case Render::NY: tmpOffset.y -= 1; break;
				case Render::PZ: tmpOffset.z += 1; break;
				case Render::NZ: tmpOffset.z -= 1; break;
				}

				s_World->AddBlock(tmpId, tmpOffset, Block::LAVA);
			}
			else {
				s_World->RemoveBlock(tmpId, tmpOffset);
			}

			// 2015/5/5
			map_chunk *mapchk = s_World->world_map.GetChunk(tmpId);
			int W = NUM_W*CHUNK_W;
			int L = NUM_L*CHUNK_L;
			int H = NUM_H*CHUNK_H;
			int x = tmpOffset.x + (tmpId.x + NUM_W/2)*CHUNK_W;
			int y = tmpOffset.y + (tmpId.y + NUM_L/2)*CHUNK_L;
			int z = tmpOffset.z + (tmpId.z + NUM_H/2)*CHUNK_H;
			test[ z*W*L + y*W + x ] = (GLubyte)mapchk->blocks[_1D(tmpOffset.x, tmpOffset.y, tmpOffset.z)].type;
			glBindTexture(GL_TEXTURE_3D, testTex);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);	// Linear Min Filter
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);	// Linear Mag Filter
			glTexSubImage3D(GL_TEXTURE_3D, 0, x, y, z, 1, 1, 1, GL_ALPHA_INTEGER_EXT, GL_UNSIGNED_BYTE, &test[ z*W*L + y*W + x ]);
			glBindTexture(GL_TEXTURE_3D, 0);

			mapchk->modified = 1;
			/*map_chunk *nbrchk;
			nbrchk = s_World->world_map.GetChunk(int3(tmpId.x-1, tmpId.y, tmpId.z));
			if (nbrchk) nbrchk->modified = 1;
			nbrchk = s_World->world_map.GetChunk(int3(tmpId.x, tmpId.y-1, tmpId.z));
			if (nbrchk) nbrchk->modified = 1;*/
		}

	}

	if (m_Player->theta < 0.01) m_Player->theta = 0.0001f; // theta=0 leads to null crossproduct with unit_z
	if (m_Player->theta > PI) m_Player->theta = PI-0.0001f;
	if (m_Player->phi > 2*PI) m_Player->phi -= 2*PI;
	if (m_Player->phi < 0) m_Player->phi += 2*PI;

	m_Player->dir = spher2car(m_Player->theta, m_Player->phi);

	// keyboard
	if (keys['W'] == TRUE) {
		m_Player->eyepos = m_Player->eyepos + m_Player->dir/0.5;
#ifdef APPLE
		keys['W'] = FALSE;
#endif
	}
	if (keys['S'] == TRUE) {
		m_Player->eyepos = m_Player->eyepos - m_Player->dir/0.5;
#ifdef APPLE
		keys['S'] = FALSE;
#endif
	}
	float3 unit_z(0, 0, 1);
	float3 crpd = cross_prod(unit_z, m_Player->dir);
	normalize(crpd);
	// strafe
	if (keys['A'] == TRUE) {
		m_Player->eyepos = m_Player->eyepos + crpd/2.0;
#ifdef APPLE
		keys['A'] = FALSE;
#endif
	}
	if (keys['D'] == TRUE) {
		m_Player->eyepos = m_Player->eyepos - crpd/2.0;
#ifdef APPLE
		keys['D'] = FALSE;
#endif
	}

	if (keys['1'] == TRUE) {
		watern -= 0.01f;
	}
	if (keys['2'] == TRUE) {
		watern += 0.01f;
	}
}

double millis = 0;
void PrintDebugMessage() {
	glBindTexture(GL_TEXTURE_2D, 0);

	float3 pos = m_Player->eyepos;

	int3 id((int)floor(pos.x / (BLOCK_LEN*CHUNK_W)), 
		(int)floor(pos.y / (BLOCK_LEN*CHUNK_L)), 
		(int)floor(pos.z / (BLOCK_LEN*CHUNK_H)));
#ifndef APPLE
	millis = tickFreq/(currTick.LowPart - lastTick.LowPart);
#endif
	char buffer[128];

	glPushMatrix();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glOrtho(0, _width, 0, _height, -1, 1);
	glTranslatef(0, (GLfloat)_height, 0);
	glScalef(20, 20, 1);
	
	glPushMatrix();
	glTranslatef(0, -1, 0);
	glPrint("FPS:%4.0f chunk_id:%d,%d,%d", millis, id.x, id.y, id.z);
	glPopMatrix();
	
	s_Render->PrintChunkStatistics(buffer);
	glPushMatrix();
	glTranslatef(0, -2, 0);
	glPrint("dir:%.2f,%.2f,%.2f", m_Player->dir.x, m_Player->dir.y, m_Player->dir.z);
	glPopMatrix();

	glPopMatrix();
}

DWORD tick1, tick2;
#ifndef APPLE
int DrawGLScene(GLvoid)
#else
void DrawGLScene()
#endif
{	
	
#ifndef APPLE
	QueryPerformanceCounter(&lastTick);
#endif

	ProcessInput();

	// frame update

	s_World->UpdateWorld();

	s_Render->DiscardUnneededChunks(m_Player->eyepos, m_Player->dir, s_World);
	s_Render->LoadNeededChunks(m_Player->eyepos, m_Player->dir, s_World);

	glBlendFunc(GL_ONE, GL_ZERO);
	glEnable(GL_TEXTURE_2D);

	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	gluPerspective(fovY, (GLfloat)_width/(GLfloat)_height, zNear, zFar);
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	
		
		
	if (keys['V'] == TRUE) {
		s_Shader->UseProgram(Shader::DEPTH);
		//s_Shader->UseProgram(Shader::NUL);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
		s_Render->DrawScene(m_Player->eyepos, m_Player->dir, 400, s_World, 1);
		return TRUE;
	}
	else {
		glDisable(GL_BLEND);
		s_Shader->UseProgram(Shader::DEPTH);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, s_Texture->frameBuffer);
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
			s_Render->DrawScene(m_Player->eyepos, m_Player->dir, 400, s_World, 1);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	}
	
	for (int w=0; w<4; w++) {
		lightpos[w].x += 0.1*(MersenneRandomD() - 0.5);
		lightpos[w].y += 0.1*(MersenneRandomD() - 0.5);
		lightpos[w].z += 0.1*(MersenneRandomD() - 0.5);
	}
	/////////////
	// Draw Rendered framebuffer texture
	s_Shader->UseProgram(Shader::BLUE);

	//glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, s_Texture->displayBuffer);

		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

		glMatrixMode(GL_PROJECTION); glLoadIdentity();
		glMatrixMode(GL_MODELVIEW); glLoadIdentity();
		glOrtho(0.0f, (GLfloat)_width, 0.0f, (GLfloat)_height, -1.0f, 1.0f);
		glScalef((GLfloat)_width, (GLfloat)_height, 1.0f);
		
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, s_Texture->block_texture);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, s_Texture->screenTexture);

		s_Render->DrawSceneBlue(m_Player->eyepos, m_Player->dir, 400, s_World->world_time);
		
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);

	//glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	
	/*s_Shader->UseProgram(Shader::BLUR);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, s_Texture->screenTexture);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, s_Texture->displayTexture);
		glBegin(GL_QUADS);
			glTexCoord2f(0, 0); glVertex2f(0, 0);
			glTexCoord2f(1, 0); glVertex2f(1, 0);
			glTexCoord2f(1, 1); glVertex2f(1, 1);
			glTexCoord2f(0, 1); glVertex2f(0, 1);
		glEnd();
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);*/

#ifndef APPLE
	QueryPerformanceCounter(&currTick);
#endif

	s_Shader->UseProgram(Shader::NUL);
	
	glColor3f(1, 1, 1);
	PrintDebugMessage();
		// Draw Red Dot
		glPointSize(1.0f);
		glColor3f(1, 0, 0);
		glBegin(GL_POINTS);
			glVertex2f(0.5, 0.5);
		glEnd();
		glColor3f(1, 1, 1);


	
#ifdef APPLE
	glutSwapBuffers();
#else
	return TRUE;
#endif
}


#ifndef APPLE
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow)
{
	MSG		msg;
	BOOL	done = FALSE;

	::hInst = hInst;

	if (!CreateGLWindow())
		return 0;

	ShowWindow(hWnd, SW_SHOW);
	ReSizeGLScene(WIDTH, HEIGHT);

	glewInit();
	
	/************* GL things ***************/
	if (!InitGL()) {
		MessageBox(0, "Fail to init GL", "ERROR", MB_OK);
		return FALSE;
	}
	
	/************* Game things ***************/
	InitClasses();

	LARGE_INTEGER _FREQ;
	QueryPerformanceFrequency(&_FREQ);
	tickFreq = (double)_FREQ.QuadPart;
	
	MersenneRandomInit((int)ReadTSC());

	// load textures
	s_Texture->LoadBlockTextures();
	
	//************* Game preinit ***************/

	s_World->LoadWorld();

	
		lightpos[0] = float3(24, 24, 8);
		lightpos[1] = float3(4, 4, 8);
		lightpos[2] = float3(24, 4, 8);
		//lightpos[3] = float3(4, 24, 8);
		lightpos[3] = float3(14, 14, 8);


	m_Player->eyepos = float3(20, 20, 5);
	m_Player->theta = PI/2;
	m_Player->phi = PI/4;
	m_Player->dir = spher2car(m_Player->theta, m_Player->phi);

	s_Render->DiscardUnneededChunks(m_Player->eyepos, m_Player->dir, s_World);
	s_Render->LoadNeededChunks(m_Player->eyepos, m_Player->dir, s_World);
		
	Sleep(500);
	
	
	// The following loop takes about 0.1 seconds !!
			for (int i=-NUM_W/2; i<NUM_W/2; i++) {
				for (int j=-NUM_L/2; j<NUM_L/2; j++) {
					map_chunk *mapchk = s_World->world_map.GetChunk(int3(i, j, 0));
					if (mapchk == 0 || mapchk->failed == 1 || mapchk->loaded == 0) {
						continue;
					}
					for_xyz(tx, ty, tz) {
						int W = NUM_W*CHUNK_W;
						int L = NUM_L*CHUNK_L;
						int H = NUM_H*CHUNK_H;
						int x = tx + (i + NUM_W/2)*CHUNK_W;
						int y = ty + (j + NUM_L/2)*CHUNK_L;
						int z = tz + (0 + NUM_H/2)*CHUNK_H;

						test[ z*W*L + y*W + x] = (GLubyte)mapchk->blocks[_1D(tx, ty, tz)].type;
					} end_xyz()
				}
			}


		s_Texture->CreateFBOTextures();
		
		
			///// Shader input
			glGenTextures(1, &testTex);
			glBindTexture(GL_TEXTURE_3D, testTex);

			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);	// Linear Min Filter
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);	// Linear Mag Filter

			// This takes 1/15 seconds !!
			glTexImage3D(GL_TEXTURE_3D, 0, GL_ALPHA8I_EXT, (NUM_W*CHUNK_W), (NUM_L*CHUNK_L), (NUM_H*CHUNK_H), 
				0, GL_ALPHA_INTEGER_EXT, GL_UNSIGNED_BYTE, test);

			glBindTexture(GL_TEXTURE_3D, 0);


		// Blue shader texture uniforms
		s_Shader->UseProgram(Shader::BLUE);

		int loc1 = s_Shader->UniformLocation(Shader::BLUE, "texture");
		int loc2 = s_Shader->UniformLocation(Shader::BLUE, "testbuffer");
		int loc3 = s_Shader->UniformLocation(Shader::BLUE, "depthfbo");

		glUniform1i(loc1, 0);
		glUniform1i(loc2, 1);
		glUniform1i(loc3, 2);
		glActiveTexture(GL_TEXTURE1);
		glEnable(GL_TEXTURE_3D);
		glBindTexture(GL_TEXTURE_3D, testTex);


		s_Shader->UseProgram(Shader::BLUR);
		int loc_scr = s_Shader->UniformLocation(Shader::BLUR, "scrbuffer");
		int loc_dep = s_Shader->UniformLocation(Shader::BLUR, "depthbuffer");
		
		glUniform1i(loc_scr, 0);
		glUniform1i(loc_dep, 2);

		s_Shader->UseProgram(Shader::NUL);

		glActiveTexture(GL_TEXTURE0);

		//setVSync(0);
		
	
	//************* Event loop ***************/

	ShowCursor(false);

	while (!done) {
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				done = TRUE;
			else {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else {
			if (active && !DrawGLScene()) {
				done = TRUE;
			}
			else {
				SwapBuffers(hDC);
			}
		}
	}
	
	ShowCursor(true);

	s_Texture->DeleteAllTextures();
	glDeleteTextures(1, &testTex);

	DeInitClasses();

	wglMakeCurrent(0, 0);
	wglDeleteContext(hRC);
	ReleaseDC(hWnd, hDC);
	DestroyWindow(hWnd);
	UnregisterClass("OpenGL", hInst);

	KillFont();

	return 0;
}

#else
int main(int argc, char** argv)
{
	GLenum type;

	glutInit(&argc, argv);

	type = GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH;
	glutInitDisplayMode(type); 

	glutInitWindowSize(800 ,600);
	glutCreateWindow("ABGR extension");
	
	/************* Game things ***************/
	ReSizeGLScene(WIDTH, HEIGHT);
	glewInit();
	
	/************* GL things ***************/
	if (!InitGL()) {
		fprintf( stderr, "Init GL failed. End.\n");
		exit(0);
	}
	InitClasses();
	// load textures
	s_Texture->LoadAllTextures();

	//************* Game preinit ***************/

	s_World->LoadWorld();

	m_Player->eyepos = float3(20, 20, 20);
	m_Player->theta = PI/2;
	m_Player->phi = PI/4;

	glutKeyboardFunc(Key);
	glutDisplayFunc(DrawGLScene);
	glutIdleFunc(glutPostRedisplay);
	glutPassiveMotionFunc(Motion);
	glutMainLoop();


	DeInitClasses();
	return 0;
}
#endif
