#pragma once

typedef int FrameTimer;

//used for array indexing
typedef enum {
    TIMER_HUNTERS_FREEZE,
    TIMER_GAME_FREEZE_STARTUP,
    TIMER_GAME_FREEZE_PLAYER_DEATH,
    TIMER_GAME_FREEZE_HUNTER_DEATH,
    TIMER_GAME_FREEZE_VICTORY,
    TIMER_GAME_FREEZE_GAME_OVER,
    TIMER_HUNTER_MODE_SCATTER,
    TIMER_HUNTER_MODE_CHASE,
    TIMER_HUNTER_MODE_FRIGHTENED,

    TIMER_TYPES_COUNT
} FrameTimerType;

bool timer_is_active(FrameTimer *timer_storage, FrameTimerType type);
void timer_set_active(FrameTimer *timer_storage, FrameTimerType type);
void timer_reset_all(FrameTimer *timer_storage);
int timer_get_full_duration(FrameTimerType type);
bool is_game_freeze_timer_active(FrameTimer *timer_storage);