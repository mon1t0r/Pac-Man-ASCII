#pragma once

#include "global_constants.h"
#include "timer.h"

typedef enum {
    DIR_NORTH,
    DIR_EAST,
    DIR_SOUTH,
    DIR_WEST
} Direction;

typedef struct {
    int pos_x, pos_y;
    int prev_pos_x, prev_pos_y;
    Direction dir;
} EntityState;

typedef enum {
    MODE_SCATTER,
    MODE_CHASE,
    MODE_FRIGHTENED
} HunterMode;

typedef struct {
    bool is_frightened;
    bool freeze_next_frame;
    bool incollidable_next_frame;
} HunterBehaviourInfo;

typedef struct {
    int score;
    int lives;
    int beepers_collected;
    FrameTimer timer_storage[TIMER_TYPES_COUNT];
    HunterBehaviourInfo hunters_info[HUNTERS_COUNT];
    int mode_change_count;
    int frightened_kill_count;
} SessionInfo;