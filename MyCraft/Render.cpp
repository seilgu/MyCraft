#include "Render.h"
#include "World.h"

extern float zNear;
extern int _width, _height;

Render::~Render() {
	KillWorkThread();

	render_list::iterator it;

	for (it = r_chunks.begin(); it != r_chunks.end(); ) {
		render_chunk *chk = (*it).second;
		DeleteChunk(chk);
		r_chunks.erase(it++);
	}
}

void Render::DeleteChunk(render_chunk *chk) {
	if (chk == 0) return;

	glDeleteBuffersARB(1, &chk->vbo);

	if (chk->vertices != 0) {
		delete[] chk->vertices;
		chk->vertices = 0;
	}

	delete chk;
	chk = 0;
}

extern LARGE_INTEGER lastTick, currTick;
int Render::FindBlock(float3 pos, float3 dir, int depth, int3 &id, int3 &offset, int &side) {
	// Get pos's chunk id
	int3 chkId((int)floor(pos.x / (BLOCK_LEN*CHUNK_W)), 
		(int)floor(pos.y / (BLOCK_LEN*CHUNK_L)), 
		(int)floor(pos.z / (BLOCK_LEN*CHUNK_H)));

	map_chunk *mapchk = s_World->world_map.GetChunk(chkId);

	if (mapchk == 0)
		return 0;

	Block *blocks = mapchk->blocks;

	int3 step(dir.x > 0 ? 1 : -1, 
		dir.y > 0 ? 1 : -1, 
		dir.z > 0 ? 1 : -1);

	float3 raypos(pos.x - chkId.x*BLOCK_LEN*CHUNK_W, pos.y - chkId.y*BLOCK_LEN*CHUNK_L, pos.z - chkId.z*BLOCK_LEN*CHUNK_H);

	int3 voxpos( (int)floor(pos.x/BLOCK_LEN) - chkId.x*CHUNK_W, 
		(int)floor(pos.y/BLOCK_LEN) - chkId.y*CHUNK_L, 
		(int)floor(pos.z/BLOCK_LEN) - chkId.z*CHUNK_H);

	int3 next(voxpos.x + (step.x > 0 ? 1 : 0), voxpos.y + (step.y > 0 ? 1 : 0), voxpos.z + (step.z > 0 ? 1 : 0));

	// Beware that dir might be zero !!
	float3 tMax( (next.x*BLOCK_LEN - raypos.x)/dir.x, 
		(next.y*BLOCK_LEN - raypos.y)/dir.y, 
		(next.z*BLOCK_LEN - raypos.z)/dir.z );

	float3 tDelta( step.x / dir.x, 
		step.y / dir.y, 
		step.z / dir.z );

	while (depth--) {

		if (INVALID(voxpos.x, voxpos.y, voxpos.z)) {

			if (voxpos.x < 0) {
				voxpos.x = CHUNK_W - 1;
				chkId.x--;
			}
			else if (voxpos.x >= CHUNK_W) {
				voxpos.x = 0;
				chkId.x++;
			}
			else if (voxpos.y < 0) {
				voxpos.y = CHUNK_L - 1;
				chkId.y--;
			}
			else if (voxpos.y >= CHUNK_L) {
				voxpos.y = 0;
				chkId.y++;
			}
			else if (voxpos.z < 0) {
				voxpos.z = CHUNK_H - 1;
				chkId.z--;
			}
			else {
				voxpos.z = 0;
				chkId.z++;
			}

			mapchk = s_World->world_map.GetChunk(chkId);
			if (mapchk == 0)
				return 0;

			blocks = mapchk->blocks;
		}

		if (blocks[_1D(voxpos.x, voxpos.y, voxpos.z)].type != Block::NUL) {
			id = chkId;
			offset = voxpos;
			return 1;
		}


		if (tMax.x < tMax.y && tMax.x < tMax.z) {
			voxpos.x += step.x;
			tMax.x += tDelta.x;
			side = (step.x > 0 ? NX : PX);
		}
		else if (tMax.y < tMax.z) {
			voxpos.y += step.y;
			tMax.y += tDelta.y;
			side = (step.y > 0 ? NY : PY);
		}
		else {
			voxpos.z += step.z;
			tMax.z += tDelta.z;
			side = (step.z > 0 ? NZ : PZ);
		}
	}

	return 0;
}

void Render::CalculateVisible(int3 id, World* world) {
	chunk_list *m_chunks = s_World->world_map.GetChunkList();
	chunk_list::iterator it = m_chunks->find(id);
	if (it == m_chunks->end())
		return;

	map_chunk *map_chk = (*it).second;
	if (map_chk == 0 || map_chk->loaded == 0 || map_chk->failed == 1 || map_chk->unneeded == 1) {
		return;
	}

	Block *blocks = map_chk->blocks;
	
	// cleaning and setup
	for (int i=0; i<CHUNK_W*CHUNK_L*CHUNK_H; i++) {
		blocks[i].outside = 0;
	}

	// first pass, find solids
	// scan in 3 directions, toggle inside/outside
	for (int i=0; i<CHUNK_W; i++) {
		for (int j=0; j<CHUNK_L; j++) {
			int inside = blocks[_1D(i, j, 0)].opaque | blocks[_1D(i, j, 0)].translucent;
			for (int k=1; k<CHUNK_H; k++) {
				if (inside == 0 && (blocks[_1D(i, j, k)].opaque == 1 || blocks[_1D(i, j, k)].translucent == 1)) {
					inside = 1;
					blocks[_1D(i, j, k)].outside |= 1<<NZ;
				}
				else if (inside == 1 && (blocks[_1D(i, j, k)].opaque == 0 && blocks[_1D(i, j, k)].translucent == 0)) {
					inside = 0;
					blocks[_1D(i, j, k-1)].outside |= 1<<PZ;
				}
			}
		}
	}
	for (int i=0; i<CHUNK_W; i++) {
		for (int k=0; k<CHUNK_H; k++) {
			int inside = blocks[_1D(i, 0, k)].opaque | blocks[_1D(i, 0, k)].translucent;
			for (int j=1; j<CHUNK_L; j++) {
				if (inside == 0 && (blocks[_1D(i, j, k)].opaque == 1 || blocks[_1D(i, j, k)].translucent == 1)) {
					inside = 1;
					blocks[_1D(i, j, k)].outside |= 1<<NY;
				}
				else if (inside == 1 && (blocks[_1D(i, j, k)].opaque == 0 && blocks[_1D(i, j, k)].translucent == 0)) {
					inside = 0;
					blocks[_1D(i, j-1, k)].outside |= 1<<PY;
				}
			}
		}
	}
	for (int k=0; k<CHUNK_H; k++) {
		for (int j=0; j<CHUNK_L; j++) {
			int inside = blocks[_1D(0, j, k)].opaque | blocks[_1D(0, j, k)].translucent;
			for (int i=1; i<CHUNK_W; i++) {
				if (inside == 0 && (blocks[_1D(i, j, k)].opaque == 1 || blocks[_1D(i, j, k)].translucent == 1)) {
					inside = 1;
					blocks[_1D(i, j, k)].outside |= 1<<NX;
				}
				else if (inside == 1 && (blocks[_1D(i, j, k)].opaque == 0 && blocks[_1D(i, j, k)].translucent == 0)) {
					inside = 0;
					blocks[_1D(i-1, j, k)].outside |= 1<<PX;
				}
			}
		}
	}

	for (int w=0; w<6; w++) {
		CheckChunkSide(id, w);
	}

	for (int i=0; i<CHUNK_W*CHUNK_L*CHUNK_H; i++) {
		if (blocks[i].modified == 1) {
			blocks[i].modified = 0;
		}
	}
}

void Render::CheckChunkSide(int3 id, int dir) {

	chunk_list *m_chunks = s_World->world_map.GetChunkList();
	chunk_list::iterator it = m_chunks->find(id);
	if (it == m_chunks->end())
		return;

	map_chunk *map_chk = (*it).second;
	Block *blocks = map_chk->blocks;

	map_chunk *map_side;
	chunk_list::iterator tmp;
	switch (dir) {
	case Render::PX : tmp = m_chunks->find(int3(id.x+1, id.y, id.z)); break;
	case Render::NX : tmp = m_chunks->find(int3(id.x-1, id.y, id.z)); break;
	case Render::PY : tmp = m_chunks->find(int3(id.x, id.y+1, id.z)); break;
	case Render::NY : tmp = m_chunks->find(int3(id.x, id.y-1, id.z)); break;
	case Render::PZ : tmp = m_chunks->find(int3(id.x, id.y, id.z+1)); break;
	case Render::NZ : tmp = m_chunks->find(int3(id.x, id.y, id.z-1)); break;
	}

	map_side = (tmp == m_chunks->end() ? 0 : (*tmp).second);

	if (map_side == 0 || map_side->loaded == 0 || map_side->failed == 1 || map_side->unneeded == 1) {
		// edge of map
		return;
	}

	// not edge
	Block *aux = map_side->blocks;
	switch (dir) {
	case Render::PX : 
		for (int j=0; j<CHUNK_L; j++) {
			for (int k=0; k<CHUNK_H; k++) {
				if (aux[_1D(0, j, k)].opaque == 0 && blocks[_1D(CHUNK_W-1, j, k)].opaque == 1) {
					blocks[_1D(CHUNK_W-1, j, k)].outside |= 1<<PX;
				}
				else if (aux[_1D(0, j, k)].opaque == 1 && blocks[_1D(CHUNK_W-1, j, k)].opaque == 0) {
					aux[_1D(0, j, k)].outside |= 1<<NX;
				}
			}
		}
		break;
	case Render::NX : 
		for (int j=0; j<CHUNK_L; j++) {
			for (int k=0; k<CHUNK_H; k++) {
				if (aux[_1D(CHUNK_W-1, j, k)].opaque == 0 && blocks[_1D(0, j, k)].opaque == 1) {
					blocks[_1D(0, j, k)].outside |= 1<<NX;
				}
				else if (aux[_1D(CHUNK_W-1, j, k)].opaque == 1 && blocks[_1D(0, j, k)].opaque == 0) {
					aux[_1D(CHUNK_W-1, j, k)].outside |= 1<<PX;
				}
			}
		}
		break;
	case Render::PY : 
		for (int i=0; i<CHUNK_W; i++) {
			for (int k=0; k<CHUNK_H; k++) {
				if (aux[_1D(i, 0, k)].opaque == 0 && blocks[_1D(i, CHUNK_L-1, k)].opaque == 1) {
					blocks[_1D(i, CHUNK_L-1, k)].outside |= 1<<PY;
				}
				else if (aux[_1D(i, 0, k)].opaque == 1 && blocks[_1D(i, CHUNK_L-1, k)].opaque == 0) {
					aux[_1D(i, 0, k)].outside |= 1<<NY;
				}
			}
		}
		break;
	case Render::NY : 
		for (int i=0; i<CHUNK_W; i++) {
			for (int k=0; k<CHUNK_H; k++) {
				if (aux[_1D(i, CHUNK_L-1, k)].opaque == 0 && blocks[_1D(i, 0, k)].opaque == 1) {
					blocks[_1D(i, 0, k)].outside |= 1<<NY;
				}
				else if (aux[_1D(i, CHUNK_L-1, k)].opaque == 1 && blocks[_1D(i, 0, k)].opaque == 0) {
					aux[_1D(i, CHUNK_L-1, k)].outside |= 1<<PY;
				}
			}
		}
		break;
	case Render::PZ : 
		for (int i=0; i<CHUNK_W; i++) {
			for (int j=0; j<CHUNK_L; j++) {
				if (aux[_1D(i, j, 0)].opaque == 0 && blocks[_1D(i, j, CHUNK_W-1)].opaque == 1) {
					blocks[_1D(i, j, CHUNK_W-1)].outside |= 1<<PZ;
				}
				else if (aux[_1D(i, j, 0)].opaque == 1 && blocks[_1D(i, j, CHUNK_W-1)].opaque == 0) {
					aux[_1D(i, j, 0)].outside |= 1<<NZ;
				}
			}
		}
		break;
	case Render::NZ : 
		for (int i=0; i<CHUNK_W; i++) {
			for (int j=0; j<CHUNK_L; j++) {
				if (aux[_1D(i, j, CHUNK_H-1)].opaque == 0 && blocks[_1D(i, j, 0)].opaque == 1) {
					blocks[_1D(i, j, 0)].outside |= 1<<NZ;
				}
				else if (aux[_1D(i, j, CHUNK_H-1)].opaque == 1 && blocks[_1D(i, j, 0)].opaque == 0) {
					aux[_1D(i, j, CHUNK_H-1)].outside |= 1<<PZ;
				}
			}
		}
		break;
	}
}

void Render::GetTextureCoordinates(short int type, int dir, float2 &dst) {
	// decide texture coordinates corr. face & type
	GLuint side_texture;
	switch (type) {
	case Block::CRATE: side_texture = TextureMgr::CRATE;
		break;
	case Block::GRASS:
		switch (dir) {
		case PZ: side_texture = TextureMgr::GRASS_TOP; break;
		case NZ: side_texture = TextureMgr::GRASS_BUTTOM; break;
		default: side_texture = TextureMgr::GRASS_SIDE; break;
		}
		break;
	case Block::SOIL: side_texture = TextureMgr::SOIL;
		break;
	case Block::STONE: side_texture = TextureMgr::STONE;
		break;
	case Block::GOLD_MINE: side_texture = TextureMgr::GOLD_MINE;
		break;
	case Block::COAL_MINE: side_texture = TextureMgr::COAL_MINE;
		break;
	case Block::COAL: side_texture = TextureMgr::COAL;
		break;
	case Block::SAND: side_texture = TextureMgr::SAND;
		break;
	case Block::GLASS: side_texture = TextureMgr::GLASS;
		break;
	case Block::LAVA: side_texture = TextureMgr::LAVA;
		break;
	case Block::SNOW:
		switch (dir) {
		case PZ: side_texture = TextureMgr::SNOW_TOP; break;
		case NZ: side_texture = TextureMgr::SNOW_BUTTOM; break;
		default: side_texture = TextureMgr::SNOW_SIDE; break;
		}
		break;
	default: break;
	}

	// calculate and set coordinates of textures
	dst = get_texture_coord(side_texture);
}

void Render::LoadChunk(render_chunk *ren_chk, map_chunk *map_chk, int urgent) {
#ifdef APPLE
	// mac doesn't use multi-thread
	urgent = 1;
#endif
	if (urgent == 0) { // multithreading
		render_pair pair(ren_chk, map_chk);
		m_Thread->PushJobs(pair);
		return;
	}
	else {
		m_Thread->threadLoadChunk( std::pair<render_chunk *, map_chunk *>(ren_chk, map_chk), m_Thread);
	}
}

/**********************************************************
	now the only work is to improve UpdateVBO to make
	it faster and need not rebuild all shit around

	LoadNeededChunks :	if (didn't have) call threadLoadChunk, make empty one, modified = 1   # since VBO = 0, drawing will do nothing
						else if (had) if (modified == 1) UpdateVBO, modified = 0

		## rename 'modified' to 'rendered'

***********************************************************/

void Render::UpdateVBO(render_chunk *ren_chk, map_chunk *map_chk) {
	if (ren_chk == 0)
		return;

	if (map_chk == 0 || ren_chk->id != map_chk->id || map_chk->loaded == 0 || map_chk->failed == 1 || map_chk->unneeded == 1) {
		ren_chk->failed = 1;
		return;
	}
	
	ren_chk->failed = 0;
	ren_chk->loaded = 0;
	ren_chk->unneeded = 0;
	
	Block *blocks = map_chk->blocks;
	int size = 0;

	int i = CHUNK_W*CHUNK_L*CHUNK_H;
	while (i--) {
		if (blocks[i].type == Block::NUL || blocks[i].outside == 0)
			continue;

		if (blocks[i].outside & (1<<0)) size++;
		if (blocks[i].outside & (1<<1)) size++;
		if (blocks[i].outside & (1<<2)) size++;
		if (blocks[i].outside & (1<<3)) size++;
		if (blocks[i].outside & (1<<4)) size++;
		if (blocks[i].outside & (1<<5)) size++;
	}
	
	// size = num_faces
	// 3coord * 4 = 12
	int newvbo = 0;
	if (ren_chk->vertices == 0) {
		newvbo = 1;
		ren_chk->vertices = new GLfloat[size*12];
		ren_chk->vbo_size = size;
	}
	else if (size > ren_chk->vbo_size) {
		if (ren_chk->vertices != 0) {
			delete[] ren_chk->vertices;
			ren_chk->vertices = 0;
		}
		ren_chk->vertices = new GLfloat[size*12];
		
		ren_chk->vbo_size = size;
		newvbo = 1;
	}

	GLfloat *vertices = ren_chk->vertices;

	if (vertices == 0) {
		MessageBox(0, "vertices alloc failed", "haha", 0);
		ren_chk->failed = 1;
		return;
	}
	
	GenerateVBOArray(vertices, map_chk->blocks);

	glBindBufferARB(GL_ARRAY_BUFFER_ARB, ren_chk->vbo);

	// now upload to graphics card
	
	if (newvbo == 1)
		glBufferDataARB(GL_ARRAY_BUFFER_ARB, size*12*sizeof(GLfloat), ren_chk->vertices, GL_STATIC_DRAW_ARB);
	else
		glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, size*12*sizeof(GLfloat), ren_chk->vertices);
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

	ren_chk->loaded = 1;
}

void Render::DiscardUnneededChunks(float3 pos, float3 dir, World *world) {
	chunk_list *chunks = world->GetRenderChunks(pos, dir);
	render_list::iterator render_it;

	for (render_it = r_chunks.begin(); render_it != r_chunks.end(); ) {
		int3 id = (*render_it).first;
		render_chunk *renderchk = (*render_it).second;
		
		if (renderchk == 0)
			continue;
		
		if (renderchk->failed == 0 && renderchk->loaded == 0) // still loading
			continue;

		if (chunks->find(id) == chunks->end()) { // in render but no in map -> unneeded
			DeleteChunk(renderchk);
			r_chunks.erase(render_it++);
		}
		else if (renderchk->failed == 1 || renderchk->unneeded == 1) { // check flags
			DeleteChunk(renderchk);
			r_chunks.erase(render_it++);
		}
		else {
			++render_it;
		}
	}
}

void Render::PrintChunkStatistics(char *buffer) {
	render_list::iterator it;

	int total = 0;
	for (it = r_chunks.begin(); it != r_chunks.end(); ++it) {
		total++;
	}

	snprintf(buffer, 16, "r_total:%d", total); // overkill
}

render_chunk *Render::CreateEmptyChunk() {
	render_chunk *renderchk = 0;
	renderchk = new render_chunk();

	if(renderchk == 0) {
		return 0;
	}

	renderchk->loaded = 0;
	renderchk->failed = 0;
	renderchk->unneeded = 0;
	renderchk->id = int3(0, 0, 0);
	renderchk->vbo = 0;
	renderchk->vbo_size = 0;
	renderchk->vertices = 0;

	return renderchk;
}

void Render::LoadNeededChunks(float3 pos, float3 dir, World *world) {
	chunk_list *chunks = world->GetRenderChunks(pos, dir);
	chunk_list::iterator map_it;

	for (map_it = chunks->begin(); map_it != chunks->end(); ++map_it) {
		int3 id = (*map_it).first;
		map_chunk *map_chk = (*map_it).second;

		if (map_chk->failed == 1 || map_chk->loaded == 0 || map_chk->unneeded == 1)
			continue;

		// if we have it in vbo list

		render_list::iterator ren_it = r_chunks.find(id);
		if (ren_it == r_chunks.end()) { // not found, need to create it
			render_chunk *renderchk = CreateEmptyChunk();

			if(renderchk == 0) {
				MessageBox(0, "RENDER ALLOCATION FAILED", "haha", 0);
				break;
			}

			renderchk->id = id;
			// always insert first
			r_chunks.insert( std::pair<int3, render_chunk *>(id, renderchk) );
			LoadChunk(renderchk, (*map_it).second, 1); // NOT TRYING MULTITHREADING!!!!
		}
		else {
			if (map_chk->modified == 1) {
	
				CalculateVisible(map_chk->id, s_World);
				UpdateVBO(ren_it->second, map_chk);
				
				for (int w=0; w<6; w++) {
					int3 side = id;
					switch (w) {
					case PX: side.x++; break;
					case NX: side.x--; break;
					case PY: side.y++; break;
					case NY: side.y--; break;
					case PZ: side.z++; break;
					case NZ: side.z--; break;
					}

					// using std::maps here might be slow...
					render_list::iterator ren_it = r_chunks.find(side);
					chunk_list::iterator map_it = chunks->find(side);
					
					if (ren_it != r_chunks.end() && map_it != chunks->end()) {
						UpdateVBO(ren_it->second, map_it->second);
					}
				}
				map_chk->modified = 0;
			}
		}
	}
}

void Render::RenderChunk(render_chunk *tmp, float3 pos, float3 dir) {
	if (tmp == 0 || tmp->failed == 1 || tmp->loaded == 0 || tmp->unneeded == 1)
		return;

	// frustum culling
	int3 id = tmp->id;

	// right plane normal
	normalize(dir);
	float3 rhs = float3(dir.y, -dir.x, 0);
	normalize(rhs);
	float r = sqrt(dir.x*dir.x + dir.y*dir.y);
	float3 upside = float3(-dir.z*dir.x/r, -dir.z*dir.y/r, r);

	float3 normal;
	for (int w=0; w<4; w++) {
		float3 n(id.x+1.0f, id.y+1.0f, id.z+1.0f);
		switch (w) {
		case 0: normal = cross_prod( zNear*dir + zNear*rhs, upside);
			break;
		case 1: normal = cross_prod(upside, zNear*dir - rhs*zNear);
			break;
		case 2: normal = cross_prod(rhs, zNear*dir + upside*zNear);
			break;
		case 3: normal = cross_prod(zNear*dir - upside*zNear, rhs);
			break;
		}
		if (normal.x > 0) { n.x--; }
		if (normal.y > 0) { n.y--; }
		if (normal.z > 0) { n.z--; }

		n.x *= CHUNK_W; n.y *= CHUNK_L; n.z *= CHUNK_H;
		n = BLOCK_LEN*n - pos;

		if (dot_prod(normal, n) > 0)
			return;
	}
	
	glEnableClientState(GL_VERTEX_ARRAY);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, tmp->vbo);
		glVertexPointer(3, GL_FLOAT, 0, 0);
		glDrawArrays( GL_QUADS, 0, tmp->vbo_size*4 );
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	glDisableClientState(GL_VERTEX_ARRAY);
}

extern Shader *s_Shader;
extern float fovY;
extern float zFar;
extern float watern;
extern float3 lightpos[4];
/*************************************************************************
	Decides what to draw, how much to draw, from given perspective
*************************************************************************/
void Render::DrawSceneBlue(float3 pos, float3 dir, float dist, float time) {
	float3 raypos = pos/BLOCK_LEN;
	
	float3 unitz(0, 0, 1);
	normalize(dir);
	float3 rhs = cross_prod(dir, unitz);
	normalize(rhs);
	float3 upside = cross_prod(rhs, dir);
	normalize(dir);

	int loc_num = s_Shader->UniformLocation(Shader::BLUE, "NUM");
		glUniform3i(loc_num, NUM_W, NUM_L, NUM_H);
	int loc_chk = s_Shader->UniformLocation(Shader::BLUE, "CHUNK");
		glUniform3i(loc_chk, CHUNK_W, CHUNK_L, CHUNK_H);
	int loc_pos = s_Shader->UniformLocation(Shader::BLUE, "startpos");
		glUniform3f(loc_pos, raypos.x, raypos.y, raypos.z);
	int loc_lightpos = s_Shader->UniformLocation(Shader::BLUE, "lightpos");
		//glUniform3f(loc_lightpos, lightpos1.x/BLOCK_LEN, lightpos1.y/BLOCK_LEN, lightpos1.z/BLOCK_LEN);
		glUniform3fv(loc_lightpos, 4, (GLfloat *)lightpos);

	int loc_watern = s_Shader->UniformLocation(Shader::BLUE, "WATER_N");
		glUniform1f(loc_watern, watern);
	int loc_time = s_Shader->UniformLocation(Shader::BLUE, "time");
		glUniform1f(loc_time, time); // WATERN

	float uratio = tan(fovY*0.5f*PI/180.0f);
	float aspect = (float)_width/(float)_height;

	float3 tl, tr, bl, br;
	tl = zFar*(dir + uratio*upside - uratio*aspect*rhs);
	tr = zFar*(dir + uratio*upside + uratio*aspect*rhs);
	bl = zFar*(dir - uratio*upside - uratio*aspect*rhs);
	br = zFar*(dir - uratio*upside + uratio*aspect*rhs);

	glBegin(GL_QUADS);
		glColor3f(bl.x, bl.y, bl.z); glTexCoord2f(0, 0); glVertex2f(0, 0);
		glColor3f(br.x, br.y, br.z); glTexCoord2f(1, 0); glVertex2f(1, 0);
		glColor3f(tr.x, tr.y, tr.z); glTexCoord2f(1, 1); glVertex2f(1, 1);
		glColor3f(tl.x, tl.y, tl.z); glTexCoord2f(0, 1); glVertex2f(0, 1);
	glEnd();

	glColor3f(1, 1, 1);
}

void Render::DrawScene(float3 pos, float3 dir, float dist, World* world, int look) {

	int loc1 = s_Shader->UniformLocation(Shader::DEPTH, "player_pos");
		glUniform4f(loc1, pos.x, pos.y, pos.z, 0);
	int loc2 = s_Shader->UniformLocation(Shader::DEPTH, "offset");

	glPushMatrix();

	if (look == 1)
		gluLookAt(pos.x, pos.y, pos.z, pos.x+dir.x, pos.y+dir.y, pos.z+dir.z, 0, 0, 1);

	glScalef(BLOCK_LEN, BLOCK_LEN, BLOCK_LEN);
	
	render_list::iterator it;

	for (int i=-NUM_W/2; i<NUM_W/2; i++) {
				for (int j=-NUM_L/2; j<NUM_L/2; j++) {
					render_chunk *renchk = 0;
					it = r_chunks.find(int3(i, j, 0));
					if (it == r_chunks.end()) continue;
					renchk = it->second;
					if (renchk == 0 || renchk->failed == 1 || renchk->loaded == 0) {
						continue;
					}
					int3 id(i, j, 0);
					id.x *= CHUNK_W;
					id.y *= CHUNK_L;
					id.z *= CHUNK_H;

					glTranslatef((GLfloat)id.x, (GLfloat)id.y, (GLfloat)id.z);
						glUniform4f(loc2, id.x*BLOCK_LEN, id.y*BLOCK_LEN, id.z*BLOCK_LEN, 0);
					RenderChunk(renchk, pos, dir);
					glTranslatef(-(GLfloat)id.x, -(GLfloat)id.y, -(GLfloat)id.z);
				}
	}

	glPopMatrix();
}

void Render::RenderChunkThread::threadLoop(void *param) {
	// should get here very early
#ifndef APPLE
	wglMakeCurrent(hDC, hRC2);
#endif
	
	Render::RenderChunkThread *self = (Render::RenderChunkThread *)param;
	while (self->active) {
		// peek jobs
		if (self->jobs.empty()) {
			Sleep(20);
		}
		else {
			// process jobs
			render_pair pair = self->jobs.front();
			self->threadLoadChunk(pair, self);
			self->jobs.pop();
		}
	}
}

extern TextureMgr *s_Texture;
void Render::GenerateVBOArray(GLfloat *vertices, Block *blocks) {
	
	if (vertices == 0) {
		MessageBox(0, "wrong usage", "ha", 0);
		return;
	}
	
	int count = 0;
	// now generate vertex & quads
	for_xyz(i, j, k) {
		int type = blocks[_1D(i, j, k)].type;

		if (type == Block::NUL)
			continue;

		if (blocks[_1D(i, j, k)].outside == 0)
			continue;

		int w = 6;
		while (w--) { // 6 faces
			if ((blocks[_1D(i, j, k)].outside & (1<<w)) == 0) continue;

			// texture coordinates are in the same order in each face except NX
			int offset = count*12;

			// now setup vertex coordinates of each face
			switch (w) {
			case PZ:
				
				vertices[offset + 0] = 0.0f;
				vertices[offset + 1] = 0.0f;
				vertices[offset + 2] = 1.0f;

				vertices[offset + 3] = 1.0f;
				vertices[offset + 4] = 0.0f;
				vertices[offset + 5] = 1.0f;

				vertices[offset + 6] = 1.0f;
				vertices[offset + 7] = 1.0f;
				vertices[offset + 8] = 1.0f;

				vertices[offset + 9] = 0.0f;
				vertices[offset + 10] = 1.0f;
				vertices[offset + 11] = 1.0f;
				break;
			case NZ:
				vertices[offset + 0] = 1.0f;
				vertices[offset + 1] = 0.0f;
				vertices[offset + 2] = 0.0f;

				vertices[offset + 3] = 0.0f;
				vertices[offset + 4] = 0.0f;
				vertices[offset + 5] = 0.0f;

				vertices[offset + 6] = 0.0f;
				vertices[offset + 7] = 1.0f;
				vertices[offset + 8] = 0.0f;

				vertices[offset + 9] = 1.0f;
				vertices[offset + 10] = 1.0f;
				vertices[offset + 11] = 0.0f;
				break;
			case PY:
				vertices[offset + 0] = 1.0f;
				vertices[offset + 1] = 1.0f;
				vertices[offset + 2] = 0.0f;

				vertices[offset + 3] = 0.0f;
				vertices[offset + 4] = 1.0f;
				vertices[offset + 5] = 0.0f;

				vertices[offset + 6] = 0.0f;
				vertices[offset + 7] = 1.0f;
				vertices[offset + 8] = 1.0f;

				vertices[offset + 9] = 1.0f;
				vertices[offset + 10] = 1.0f;
				vertices[offset + 11] = 1.0f;
				break;
			case NY:
				vertices[offset + 0] = 0.0f;
				vertices[offset + 1] = 0.0f;
				vertices[offset + 2] = 0.0f;

				vertices[offset + 3] = 1.0f;
				vertices[offset + 4] = 0.0f;
				vertices[offset + 5] = 0.0f;

				vertices[offset + 6] = 1.0f;
				vertices[offset + 7] = 0.0f;
				vertices[offset + 8] = 1.0f;

				vertices[offset + 9] = 0.0f;
				vertices[offset + 10] = 0.0f;
				vertices[offset + 11] = 1.0f;
				break;
			case PX:
				vertices[offset + 0] = 1.0f;
				vertices[offset + 1] = 0.0f;
				vertices[offset + 2] = 0.0f;

				vertices[offset + 3] = 1.0f;
				vertices[offset + 4] = 1.0f;
				vertices[offset + 5] = 0.0f;

				vertices[offset + 6] = 1.0f;
				vertices[offset + 7] = 1.0f;
				vertices[offset + 8] = 1.0f;

				vertices[offset + 9] = 1.0f;
				vertices[offset + 10] = 0.0f;
				vertices[offset + 11] = 1.0f;
				break;
			case NX:
				vertices[offset + 0] = 0.0f;
				vertices[offset + 1] = 0.0f;
				vertices[offset + 2] = 0.0f;

				vertices[offset + 3] = 0.0f;
				vertices[offset + 4] = 0.0f;
				vertices[offset + 5] = 1.0f;

				vertices[offset + 6] = 0.0f;
				vertices[offset + 7] = 1.0f;
				vertices[offset + 8] = 1.0f;

				vertices[offset + 9] = 0.0f;
				vertices[offset + 10] = 1.0f;
				vertices[offset + 11] = 0.0f;
				break;
			}

			// translate to relative position
			vertices[offset + 0] += (GLfloat)i;
			vertices[offset + 1] += (GLfloat)j;
			vertices[offset + 2] += (GLfloat)k;

			vertices[offset + 3] += (GLfloat)i;
			vertices[offset + 4] += (GLfloat)j;
			vertices[offset + 5] += (GLfloat)k;

			vertices[offset + 6] += (GLfloat)i;
			vertices[offset + 7] += (GLfloat)j;
			vertices[offset + 8] += (GLfloat)k;

			vertices[offset + 9] += (GLfloat)i;
			vertices[offset + 10] += (GLfloat)j;
			vertices[offset + 11] += (GLfloat)k;
			// next face
			count++;
		}
	} end_xyz()
}

void Render::RenderChunkThread::threadLoadChunk(render_pair pair, Render::RenderChunkThread *self) {
	render_chunk *ren_chk = pair.first;
	map_chunk *map_chk = pair.second;

	if (ren_chk == 0)
		return;

	ren_chk->failed = 0;
	ren_chk->loaded = 0;
	ren_chk->unneeded = 0;
	ren_chk->vbo_size = 0;

	if (map_chk == 0 || ren_chk->id != map_chk->id) {
		ren_chk->failed = 1;
		return;
	}

	if (map_chk->loaded == 0 || map_chk->failed == 1 || map_chk->unneeded == 1) {
		ren_chk->failed = 1;
		return;
	}

	glGenBuffersARB(1, &ren_chk->vbo);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, ren_chk->vbo);

	// now upload to graphics card
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, 0, 0, GL_STATIC_DRAW_ARB);
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

	ren_chk->loaded = 1;
}
