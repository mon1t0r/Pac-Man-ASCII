#pragma once

#include "world.h"

//time delay for each frame
#define FRAME_TIME 0.4f
//how slow hunters move in frightened mode
#define HUNTER_FRIGHTENED_SLOWDOWN_MODIFIER 2

extern World world;
extern SessionInfo session;
extern bool force_render_next_frame;
extern unsigned int frame_count;

void game_init();
int game_update();
void game_free();
void set_world_path(char *path);

int hunters_get_slowdown_stage(int time);
HunterMode hunter_get_mode(int type);