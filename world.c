#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include "exit_cause.h"
#include "world.h"

//cell info (karel beepers count)
typedef enum {
    CELL_INFO_EMPTY = 1,
    CELL_INFO_ENERGIZER,
    CELL_INFO_HUNTER_SPAWN_POINT,
    CELL_INFO_LABEL
} CellInfoType;

const Direction opposite[] = { DIR_SOUTH, DIR_WEST, DIR_NORTH, DIR_EAST };

static inline int karel_to_pacman_x(World *world, int pos);
static inline int karel_to_pacman_y(World *world, int pos);

void world_init(World *world, const char *world_path)
{
    FILE *file = fopen(world_path, "r");
    if(file == NULL)
        exit(CAUSE_NO_WORLD_FILE);

    //read world type (pacman/karel)
    char world_type;
    if(fscanf(file, "%c\n", &world_type) < 1)
        exit(CAUSE_INVALID_WORLD_INFO);
    
    //skip the rest of line (world name)
    char skip_char;
    while((skip_char = getc(file)) != '\n' && skip_char != EOF);

    bool is_karel_world = world_type == 'K';

    char player_dir;
    int player_x, player_y;
    if(is_karel_world)
    {
        int player_beepers;
        if (fscanf(file, "%d %d %d %d %c %d\n",
                &world->width, &world->height, &player_x, &player_y, &player_dir, &player_beepers) != 6)
            exit(CAUSE_INVALID_WORLD_INFO);
        world->width -= 2;
        world->height -= 2;
        player_x = karel_to_pacman_x(world, player_x);
        player_y = karel_to_pacman_y(world, player_y);
    }
    else
    {
        if (fscanf(file, "%d %d %d %d %c\n",
                &world->width, &world->height, &player_x, &player_y, &player_dir) != 5)
            exit(CAUSE_INVALID_WORLD_INFO);
    }

    world->player_home_state.pos_x = world->player_home_state.prev_pos_x = player_x;
    world->player_home_state.pos_y = world->player_home_state.prev_pos_y = player_y;

    if(!char_to_dir(player_dir, &world->player_home_state.dir))
        exit(CAUSE_INVALID_WORLD_INFO);

    if(!world_cell_in_bounds(world, world->player_home_state.pos_x, world->player_home_state.pos_y))
        exit(CAUSE_INVALID_WORLD_INFO);

    world->beepers_count = world->width * world->height;

    //allocate memory for cell matrix
    world->cell_matrix = (CellType **) malloc(world->width * sizeof(CellType *));
    for (int i = 0; i < world->width; ++i)
    {
        world->cell_matrix[i] = (CellType *) calloc(world->height, sizeof(CellType));
        for(int j = 0; j < world->height; ++j)
            world->cell_matrix[i][j] = CELL_BEEPER;
    }

    //allocate memory for wall matrix
    world->wall_matrix = (WallState **) malloc((world->width * 2 + 1) * sizeof(WallState *));
    for (int i = 0; i < world->width * 2 + 1; ++i)
        world->wall_matrix[i] = (WallState *) calloc((world->height * 2 + 1), sizeof(WallState));

    int x, y;
    char line_type;
    int hunters_home_state_count = 0;

    while(fscanf(file, "%c", &line_type) != EOF)
    {
        line_type = toupper(line_type);
        switch (line_type)
        {
        case 'W':
            {
                char wall_char_dir;
                if (fscanf(file, "%d %d %c\n", &x, &y, &wall_char_dir) != 3)
                    exit(CAUSE_INVALID_LINE_INFO);

                if(is_karel_world)
                {
                    x = karel_to_pacman_x(world, x);
                    y = karel_to_pacman_y(world, y);
                }

                Direction wall_dir;
                if(!char_to_dir(wall_char_dir, &wall_dir))
                    exit(CAUSE_INVALID_WALL_DIRECTION);
                
                WallState *wall;
                if(world_get_wall_coords(world, &x, &y, wall_dir) && (wall = world_get_wall(world, x, y)))
                    wall->exists = true;
                else
                    exit(CAUSE_INVALID_WALL_INFO);
            }
            break;

        case 'B':
            {
                int beepers_count;
                if (fscanf(file, "%d %d %d\n", &x, &y, &beepers_count) != 3)
                    exit(CAUSE_INVALID_LINE_INFO);
                
                if(is_karel_world)
                {
                    x = karel_to_pacman_x(world, x);
                    y = karel_to_pacman_y(world, y);
                }

                if (x < 0 || y < 0 || x >= world->width || y >= world->height)
                    exit(CAUSE_INVALID_BEEPER_INFO);

                int cell_type = CELL_BEEPER;
                switch (beepers_count)
                {
                case CELL_INFO_EMPTY:
                    cell_type = CELL_EMPTY;
                    break;

                case CELL_INFO_ENERGIZER:
                    cell_type = CELL_ENERGIZER;
                    break;

                case CELL_INFO_HUNTER_SPAWN_POINT:
                {
                    cell_type = CELL_EMPTY;
                    
                    if(hunters_home_state_count >= HUNTERS_COUNT)
                        break;

                    EntityState *hunter_home_state = world->hunters_home_states + hunters_home_state_count++;

                    hunter_home_state->pos_x = hunter_home_state->prev_pos_x = x;
                    hunter_home_state->pos_y = hunter_home_state->prev_pos_y = y;
                    hunter_home_state->dir = DIR_EAST;
                    break;
                }

                case CELL_INFO_LABEL:
                    cell_type = CELL_EMPTY;
                    world->label_pos_x = x;
                    world->label_pos_y = y;
                    break;
                }

                if(cell_type == CELL_EMPTY)
                    world->beepers_count--;

                world->cell_matrix[x][y] = (CellType) cell_type;
            }
            break;
        
        default:
            exit(CAUSE_INVALID_LINE_CHARACTER);
        }
    }

    fclose(file);
}

int world_read_name(char *name_buffer, const char *world_path)
{
    FILE *file = fopen(world_path, "r");
    if(file == NULL)
        return -1;

    char c;
    if(fscanf(file, "%c ", &c) != 1)
    {
        fclose(file);
        return -1;
    }

    //read world name until new line char by char
    int line_size = 0;
    while((c = getc(file)) != '\n' && c != EOF)
        name_buffer[line_size++] = c;
    name_buffer[line_size] = 0;
    
    fclose(file);
    return line_size;
}

void world_free(World *world)
{
    if(world->cell_matrix)
    {
        for (int i = 0; i < world->width; ++i)
            free(world->cell_matrix[i]);
        free(world->cell_matrix);
        world->cell_matrix = NULL;
    }

    if(world->wall_matrix)
    {
        for (int i = 0; i <= world->width; ++i)
            free(world->wall_matrix[i]);
        free(world->wall_matrix);
        world->wall_matrix = NULL;
    }
}

CellType* world_get_cell(World *world, int x, int y)
{
    if(!world_cell_in_bounds(world, x, y))
        return NULL;
    return &world->cell_matrix[x][y];
}

WallState* world_get_wall(World *world, int wall_x, int wall_y)
{
    if(!world_wall_in_bounds(world, wall_x, wall_y))
        return NULL;
    return &world->wall_matrix[wall_x][wall_y];
}

WallState* world_get_wall_dir(World *world, int cell_x, int cell_y, Direction dir)
{
    if(!world_cell_in_bounds(world, cell_x, cell_y) || !world_get_wall_coords(world, &cell_x, &cell_y, dir))
        return NULL;
    
    return world_get_wall(world, cell_x, cell_y);
}

bool world_get_wall_coords(World *world, int *cell_x, int *cell_y, Direction dir)
{
    switch (dir)
    {
    case DIR_NORTH:
        *cell_x = *cell_x * 2 + 1;
        *cell_y = *cell_y * 2;
        break;
    case DIR_EAST:
        *cell_x = *cell_x * 2 + 2;
        *cell_y = *cell_y * 2 + 1;
        break;
    case DIR_SOUTH:
        *cell_x = *cell_x * 2 + 1;
        *cell_y = *cell_y * 2 + 2;
        break;
    case DIR_WEST:
        *cell_x = *cell_x * 2;
        *cell_y = *cell_y * 2 + 1;
        break;
    default:
        return false;
    }

    return world_wall_in_bounds(world, *cell_x, *cell_y);
}

bool world_get_cell_neighbour(World *world, int *cell_x, int *cell_y, Direction dir)
{
    switch (dir)
    {
    case DIR_NORTH:
        (*cell_y)--;
        break;
    case DIR_EAST:
        (*cell_x)++;
        break;
    case DIR_SOUTH:
        (*cell_y)++;
        break;
    case DIR_WEST:
        (*cell_x)--;
        break;
    default:
        return false;
    }

    return world_cell_in_bounds(world, *cell_x, *cell_y);
}

bool world_cell_in_bounds(World *world, int x, int y)
{
    return x >= 0 && y >= 0 && x < world->width && y < world->height;
}

bool world_wall_in_bounds(World *world, int x, int y)
{
    return x >= 0 && y >= 0 && x < world->width * 2 + 1 && y < world->height * 2 + 1;
}

float world_cell_distance(int x_1, int y_1, int x_2, int y_2)
{
    return sqrtf(powf(x_1 - x_2, 2) + powf(y_1 - y_2, 2));
}

bool char_to_dir(int c, Direction *dir)
{
    switch (toupper(c))
    {
    case 'N':
        *dir = DIR_NORTH;
        return true;
    case 'E':
        *dir =  DIR_EAST;
        return true;
    case 'S':
        *dir =  DIR_SOUTH;
        return true;
    case 'W':
        *dir =  DIR_WEST;
        return true;
    }
    return false;
}

Direction direction_opposite(Direction dir)
{
    return opposite[dir];
}

bool entity_front_is_clear(EntityState *entity, World *world)
{
    return entity_dir_is_clear(entity, world, entity->dir);
}

bool entity_dir_is_clear(EntityState *entity, World *world, Direction dir)
{
    WallState *wall = world_get_wall_dir(world, entity->pos_x, entity->pos_y, dir);
    return !wall || !wall->exists;
}

void entity_step(EntityState *entity, World *world)
{
    int new_x = entity->pos_x;
    int new_y = entity->pos_y;
    bool in_portal = !world_get_cell_neighbour(world, &new_x, &new_y, entity->dir);

    //handle world borders step
    if(in_portal)
    {
        if(new_x >= world->width)
            new_x = 0;
        else if(new_x < 0)
            new_x = world->width - 1;
        
        if(new_y >= world->height)
            new_y = 0;
        else if(new_y < 0)
            new_y = world->height - 1;
    }

    if(entity_front_is_clear(entity, world))
    {
        if(in_portal)
        {
            entity->prev_pos_x = new_x;
            entity->prev_pos_y = new_y;
        }
        else
        {
            entity->prev_pos_x = entity->pos_x;
            entity->prev_pos_y = entity->pos_y;
        }
        
        entity->pos_x = new_x;
        entity->pos_y = new_y;
    }
}

void entity_reset_prev_pos(EntityState *entity)
{
    entity->prev_pos_x = entity->pos_x;
    entity->prev_pos_y = entity->pos_y;
}

static inline int karel_to_pacman_x(World *world, int pos)
{
    return pos - 2;
}

static inline int karel_to_pacman_y(World *world, int pos)
{
    return world->height - pos + 1;
}