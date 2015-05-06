#ifndef APPLE

#include <Windows.h>

#endif

#include <stdio.h>

#include "Render.h"
#include "texture.h"
#include "World.h"
#include "SOIL.h"
#include "Block.h"


int TextureMgr::LoadBlockTextures() {
	block_texture = SOIL_load_OGL_texture("./Textures/terrain.png", SOIL_LOAD_RGBA, SOIL_CREATE_NEW_ID, SOIL_FLAG_POWER_OF_TWO | SOIL_FLAG_INVERT_Y);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	return 1;
}

extern int _width, _height;
void TextureMgr::CreateFBOTextures() {
	
	glDeleteRenderbuffersEXT(1, &depthBuffer);
	glGenRenderbuffersEXT(1, &depthBuffer);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depthBuffer);
		glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, _width, _height);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);

	glDeleteTextures(1, &screenTexture);
	glGenTextures(1, &screenTexture);
		glBindTexture(GL_TEXTURE_2D, screenTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);	// Linear Min Filter
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);	// Linear Mag Filter
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F_ARB, _width, _height, 0, GL_RGBA, GL_FLOAT, 0);
		glBindTexture(GL_TEXTURE_2D, 0);

	glDeleteFramebuffersEXT(1, &frameBuffer);
	glGenFramebuffersEXT(1, &frameBuffer);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, frameBuffer);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, depthBuffer);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, screenTexture, 0);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	glDeleteTextures(1, &displayTexture);
	glGenTextures(1, &displayTexture);
		glBindTexture(GL_TEXTURE_2D, displayTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);	// Linear Min Filter
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);	// Linear Mag Filter
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, _width, _height, 0, GL_RGBA, GL_FLOAT, 0);
		glBindTexture(GL_TEXTURE_2D, 0);

	glDeleteFramebuffersEXT(1, &displayBuffer);
		glGenFramebuffersEXT(1, &displayBuffer);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, displayBuffer);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, displayTexture, 0);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void TextureMgr::DeleteAllTextures() {
	glDeleteTextures(1, &displayTexture);
	glDeleteFramebuffersEXT(1, &displayBuffer);
	glDeleteTextures(1, &block_texture);
	glDeleteRenderbuffersEXT(1, &depthBuffer);
	glDeleteTextures(1, &screenTexture);
	glDeleteFramebuffersEXT(1, &frameBuffer);
}
