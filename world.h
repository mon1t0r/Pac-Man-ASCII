#pragma once

#include <stdbool.h>
#include "session.h"

typedef enum {
    CELL_EMPTY,
    CELL_BEEPER,
    CELL_ENERGIZER
} CellType;

typedef struct {
    bool exists;
    bool is_vertical;
} WallState;

typedef struct {
    int label_pos_x, label_pos_y;
    
    int beepers_count;

    int width, height;
    CellType **cell_matrix;
    WallState **wall_matrix;

    EntityState player;
    EntityState player_home_state;
    
    EntityState hunters[HUNTERS_COUNT];
    EntityState hunters_home_states[HUNTERS_COUNT];
} World;

void world_init(World *world, const char *world_path);
int world_read_name(char *name_buffer, const char *world_path);
void world_free(World *world);

CellType* world_get_cell(World *world, int x, int y);
WallState* world_get_wall(World *world, int wall_x, int wall_y);
WallState* world_get_wall_dir(World *world, int cell_x, int cell_y, Direction dir);
bool world_get_wall_coords(World *world, int *cell_x, int *cell_y, Direction dir);

bool world_get_cell_neighbour(World *world, int *cell_x, int *cell_y, Direction dir);

bool world_cell_in_bounds(World *world, int x, int y);
bool world_wall_in_bounds(World *world, int x, int y);

float world_cell_distance(int x_1, int y_1, int x_2, int y_2);

bool char_to_dir(int c, Direction *dir);
Direction direction_opposite(Direction dir);

bool entity_front_is_clear(EntityState *entity, World *world);
bool entity_dir_is_clear(EntityState *entity, World *world, Direction dir);
void entity_step(EntityState *entity, World *world);
void entity_reset_prev_pos(EntityState *entity);