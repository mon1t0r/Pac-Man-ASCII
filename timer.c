#include <stdbool.h>
#include "timer.h"

//duration in frame counts
const int timer_durations[TIMER_TYPES_COUNT] = {
    100, //TIMER_HUNTERS_FREEZE
    10,  //TIMER_GAME_FREEZE_STARTUP
    7,   //TIMER_GAME_FREEZE_PLAYER_DEATH
    4,   //TIMER_GAME_FREEZE_HUNTER_DEATH
    9,   //TIMER_GAME_FREEZE_VICTORY
    25,  //TIMER_GAME_FREEZE_GAME_OVER
    17,  //TIMER_HUNTER_MODE_SCATTER
    50,  //TIMER_HUNTER_MODE_CHASE
    35   //TIMER_HUNTER_MODE_FRIGHTENED
};

bool timer_is_active(FrameTimer *timer_storage, FrameTimerType type)
{
    return timer_storage[type] > 0;
}

void timer_set_active(FrameTimer *timer_storage, FrameTimerType type)
{
    timer_storage[type] = timer_durations[type];
}

void timer_reset_all(FrameTimer *timer_storage)
{
    for(int i = 0; i < TIMER_TYPES_COUNT; ++i)
        timer_storage[i] = 0;
}

int timer_get_full_duration(FrameTimerType type)
{
    return timer_durations[type];
}

bool is_game_freeze_timer_active(FrameTimer *timer_storage)
{
    for(int i = TIMER_GAME_FREEZE_STARTUP; i <= TIMER_GAME_FREEZE_GAME_OVER; ++i)
        if(timer_is_active(timer_storage, i))
            return true;
    return false;
}