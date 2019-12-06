/*
 *  This file is part of Permafrost Engine. 
 *  Copyright (C) 2018 Eduard Permyakov 
 *
 *  Permafrost Engine is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Permafrost Engine is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 *  Linking this software statically or dynamically with other modules is making 
 *  a combined work based on this software. Thus, the terms and conditions of 
 *  the GNU General Public License cover the whole combination. 
 *  
 *  As a special exception, the copyright holders of Permafrost Engine give 
 *  you permission to link Permafrost Engine with independent modules to produce 
 *  an executable, regardless of the license terms of these independent 
 *  modules, and to copy and distribute the resulting executable under 
 *  terms of your choice, provided that you also meet, for each linked 
 *  independent module, the terms and conditions of the license of that 
 *  module. An independent module is a module which is not derived from 
 *  or based on Permafrost Engine. If you modify Permafrost Engine, you may 
 *  extend this exception to your version of Permafrost Engine, but you are not 
 *  obliged to do so. If you do not wish to do so, delete this exception 
 *  statement from your version.
 *
 */

#include "field.h"
#include "nav_private.h"
#include "../entity.h"
#include "../map/public/tile.h"
#include "../game/public/game.h"
#include "../lib/public/pqueue.h"

#include <string.h>
#include <assert.h>
#include <math.h>


#define MIN(a, b)           ((a) < (b) ? (a) : (b))
#define ARR_SIZE(a)         (sizeof(a)/sizeof(a[0]))
#define MAX_ENTS_PER_CHUNK  (4096)

PQUEUE_TYPE(coord, struct coord)
PQUEUE_IMPL(static, coord, struct coord)

struct box_xz{
    float x_min, x_max;
    float z_min, z_max;
};

/*****************************************************************************/
/* GLOBAL VARIABLES                                                          */
/*****************************************************************************/

vec2_t g_flow_dir_lookup[9] = {
    [FD_NONE] = (vec2_t){  0.0f,               0.0f              },
    [FD_NW]   = (vec2_t){  1.0f / sqrt(2.0f), -1.0f / sqrt(2.0f) },
    [FD_N]    = (vec2_t){  0.0f,              -1.0f              },
    [FD_NE]   = (vec2_t){ -1.0f / sqrt(2.0f), -1.0f / sqrt(2.0f) },
    [FD_W]    = (vec2_t){  1.0f,               0.0f              },
    [FD_E]    = (vec2_t){ -1.0f,               0.0f              },
    [FD_SW]   = (vec2_t){  1.0f / sqrt(2.0f),  1.0f / sqrt(2.0f) },
    [FD_S]    = (vec2_t){  0.0f,               1.0f              },
    [FD_SE]   = (vec2_t){ -1.0f / sqrt(2.0f),  1.0f / sqrt(2.0f) },
};

/*****************************************************************************/
/* STATIC FUNCTIONS                                                          */
/*****************************************************************************/

static int neighbours_grid(const struct nav_chunk *chunk,
                           struct coord coord, bool only_passable, 
                           struct coord *out_neighbours, uint8_t *out_costs)
{
    int ret = 0;

    for(int r = -1; r <= 1; r++) {
        for(int c = -1; c <= 1; c++) {

            int abs_r = coord.r + r;
            int abs_c = coord.c + c;

            if(abs_r < 0 || abs_r >= FIELD_RES_R)
                continue;
            if(abs_c < 0 || abs_c >= FIELD_RES_C)
                continue;
            if(r == 0 && c == 0)
                continue;
            if(only_passable 
            && (chunk->cost_base[abs_r][abs_c] == COST_IMPASSABLE || chunk->blockers[abs_r][abs_c] > 0))
                continue;
            if((r == c) || (r == -c)) /* diag */
                continue;

            out_neighbours[ret] = (struct coord){abs_r, abs_c};
            out_costs[ret] = chunk->cost_base[abs_r][abs_c];

            if(chunk->blockers[abs_r][abs_c])
                out_costs[ret] = COST_IMPASSABLE;

            ret++;
        }
    }
    assert(ret < 9);
    return ret;
}

static int neighbours_grid_LOS(const struct nav_chunk *chunk,
                               const struct LOS_field *los, struct coord coord, 
                               struct coord *out_neighbours, uint8_t *out_costs)
{
    int ret = 0;

    for(int r = -1; r <= 1; r++) {
        for(int c = -1; c <= 1; c++) {

            int abs_r = coord.r + r;
            int abs_c = coord.c + c;

            if(abs_r < 0 || abs_r >= FIELD_RES_R)
                continue;
            if(abs_c < 0 || abs_c >= FIELD_RES_C)
                continue;
            if(r == 0 && c == 0)
                continue;
            if((r == c) || (r == -c)) /* diag */
                continue;
            if(los->field[abs_r][abs_c].wavefront_blocked)
                continue;

            out_neighbours[ret] = (struct coord){abs_r, abs_c};
            out_costs[ret] = chunk->cost_base[abs_r][abs_c];

            if(chunk->blockers[abs_r][abs_c])
                out_costs[ret] = COST_IMPASSABLE;

            ret++;
        }
    }
    assert(ret < 9);
    return ret;
}

static enum flow_dir flow_dir(const float integration_field[FIELD_RES_R][FIELD_RES_C], 
                              struct coord coord)
{
    float min_cost = INFINITY;
    const int r = coord.r;
    const int c = coord.c;

    if(r > 0)
        min_cost = MIN(min_cost, integration_field[r-1][c]);

    if(r < (FIELD_RES_R-1))
        min_cost = MIN(min_cost, integration_field[r+1][c]);

    if(c > 0)
        min_cost = MIN(min_cost, integration_field[r][c-1]);

    if(c < (FIELD_RES_C-1))
        min_cost = MIN(min_cost, integration_field[r][c+1]);

    /* Diagonal directions are allowed only when _both_ the side 
     * tiles sharing an edge with the corner tile are passable. 
     * This is so that the flow vector never causes an entity 
     * to move from a passable region to an impassable one. */

    if(r > 0 && c > 0
    && integration_field[r-1][c] < INFINITY
    && integration_field[r][c-1] < INFINITY)
        min_cost = MIN(min_cost, integration_field[r-1][c-1]);

    if(r > 0 && c < (FIELD_RES_C-1)
    && integration_field[r-1][c] < INFINITY
    && integration_field[r][c+1] < INFINITY)
        min_cost = MIN(min_cost, integration_field[r-1][c+1]);

    if(r < (FIELD_RES_R-1) && c > 0
    && integration_field[r+1][c] < INFINITY
    && integration_field[r][c-1] < INFINITY)
        min_cost = MIN(min_cost, integration_field[r+1][c-1]);

    if(r < (FIELD_RES_R-1) && c < (FIELD_RES_C-1)
    && integration_field[r+1][c] < INFINITY
    && integration_field[r][c+1] < INFINITY)
        min_cost = MIN(min_cost, integration_field[r+1][c+1]);

    assert(min_cost < INFINITY);

    /* Prioritize the cardinal directions over the diagonal ones */
    if(r > 0 
    && integration_field[r-1][c] == min_cost)
        return FD_N; 
    else if(r < (FIELD_RES_R-1) 
    && integration_field[r+1][c] == min_cost)
        return FD_S;
    else if(c < (FIELD_RES_C-1) 
    && integration_field[r][c+1] == min_cost)
        return FD_E;
    else if(c > 0 
    && integration_field[r][c-1] == min_cost)
        return FD_W;
    else if(r > 0 && c > 0 
    && integration_field[r-1][c-1] == min_cost)
        return FD_NW; 
    else if(r > 0 && c < (FIELD_RES_C-1) 
    && integration_field[r-1][c+1] == min_cost)
        return FD_NE;
    else if(r < (FIELD_RES_R-1) && c > 0 
    && integration_field[r+1][c-1] == min_cost)
        return FD_SW;
    else if(r < (FIELD_RES_R-1) && c < (FIELD_RES_R-1) 
    && integration_field[r+1][c+1] == min_cost)
        return FD_SE;
    else
        assert(0);
}

static bool is_LOS_corner(struct coord cell, const uint8_t cost_field[FIELD_RES_R][FIELD_RES_C],
                          const uint8_t blockers_field[FIELD_RES_R][FIELD_RES_C])
{
    if(cell.r > 0 && cell.r < FIELD_RES_R-1) {

        bool left_blocked  = cost_field    [cell.r - 1][cell.c] == COST_IMPASSABLE
                          || blockers_field[cell.r - 1][cell.c] > 0;
        bool right_blocked = cost_field    [cell.r + 1][cell.c] == COST_IMPASSABLE
                          || blockers_field[cell.r + 1][cell.c] > 0;
        if(left_blocked ^ right_blocked)
            return true;
    }

    if(cell.c > 0 && cell.c < FIELD_RES_C-1) {

        bool top_blocked = cost_field    [cell.r][cell.c - 1] == COST_IMPASSABLE
                        || blockers_field[cell.r][cell.c - 1] > 0;
        bool bot_blocked = cost_field    [cell.r][cell.c + 1] == COST_IMPASSABLE
                        || blockers_field[cell.r][cell.c + 1] > 0;
        if(top_blocked ^ bot_blocked)
            return true;
    }
    
    return false;
}

static void create_wavefront_blocked_line(struct tile_desc target, struct tile_desc corner, 
                                          const struct nav_private *priv, vec3_t map_pos, 
                                          struct LOS_field *out_los)
{
    struct map_resolution res = {
        priv->width, priv->height,
        FIELD_RES_C, FIELD_RES_R
    };

    /* First determine the slope of the LOS blocker line in the XZ plane */
    struct box target_bounds = M_Tile_Bounds(res, map_pos, target);
    struct box corner_bounds = M_Tile_Bounds(res, map_pos, corner);

    vec2_t target_center = (vec2_t){
        target_bounds.x - target_bounds.width / 2.0f,
        target_bounds.z + target_bounds.height / 2.0f
    };
    vec2_t corner_center = (vec2_t){
        corner_bounds.x - corner_bounds.width / 2.0f,
        corner_bounds.z + corner_bounds.height / 2.0f
    };

    vec2_t slope;
    PFM_Vec2_Sub(&target_center, &corner_center, &slope);
    PFM_Vec2_Normal(&slope, &slope);

    /* Now use Bresenham's line drawing algorithm to follow a line 
     * of the computed slope starting at the 'corner' until we hit the 
     * edge of the field. 
     * Multiply by 1_000 to convert slope to integer deltas, but keep 
     * 3 digits of precision after the decimal.*/
    int dx =  abs(slope.raw[0] * 1000);
    int dy = -abs(slope.raw[1] * 1000);
    int sx = slope.raw[0] > 0.0f ? 1 : -1;
    int sy = slope.raw[1] < 0.0f ? 1 : -1;
    int err = dx + dy, e2;

    struct coord curr = (struct coord){corner.tile_r, corner.tile_c};
    do {

        out_los->field[curr.r][curr.c].wavefront_blocked = 1;

        e2 = 2 * err;
        if(e2 >= dy) {
            err += dy;
            curr.c += sx;
        }
        if(e2 <= dx) {
            err += dx;
            curr.r += sy;
        }

    }while(curr.r >= 0 && curr.r < FIELD_RES_R && curr.c >= 0 && curr.c < FIELD_RES_C);
}

static void pad_wavefront(struct LOS_field *out_los)
{
    for(int r = 0; r < FIELD_RES_R; r++) {
    for(int c = 0; c < FIELD_RES_C; c++) {

        if(out_los->field[r][c].wavefront_blocked) {
        
            for(int rr = r-1; rr <= r+1; rr++) {
            for(int cc = c-1; cc <= c+1; cc++) {
            
                if(rr < 0 || rr > FIELD_RES_R-1)
                    continue;
                if(cc < 0 || cc > FIELD_RES_C-1)
                    continue;
                out_los->field[rr][cc].visible = 0;
            }}
        }
    }}
}

static void build_integration_field(pq_coord_t *frontier, const struct nav_chunk *chunk, 
                                    float inout[FIELD_RES_R][FIELD_RES_C])
{
    while(pq_size(frontier) > 0) {

        struct coord curr;
        pq_coord_pop(frontier, &curr);

        struct coord neighbours[8];
        uint8_t neighbour_costs[8];
        int num_neighbours = neighbours_grid(chunk, curr, true, neighbours, neighbour_costs);

        for(int i = 0; i < num_neighbours; i++) {

            float total_cost = inout[curr.r][curr.c] + neighbour_costs[i];
            if(total_cost < inout[neighbours[i].r][neighbours[i].c]) {

                inout[neighbours[i].r][neighbours[i].c] = total_cost;
                if(!pq_coord_contains(frontier, neighbours[i]))
                    pq_coord_push(frontier, total_cost, neighbours[i]);
            }
        }
    }
}

static void build_flow_field(float intf[FIELD_RES_R][FIELD_RES_C], struct flow_field *inout_flow)
{

    /* Build the flow field from the integration field. Don't touch any impassable tiles
     * as they may have already been set in the case that a single chunk is divided into
     * multiple passable 'islands', but a computed path takes us through more than one of
     * these 'islands'. */
    for(int r = 0; r < FIELD_RES_R; r++) {
        for(int c = 0; c < FIELD_RES_C; c++) {

            if(intf[r][c] == INFINITY)
                continue;

            if(intf[r][c] == 0.0f) {

                inout_flow->field[r][c].dir_idx = FD_NONE;
                continue;
            }

            inout_flow->field[r][c].dir_idx = flow_dir(intf, (struct coord){r, c});
        }
    }
}

static void fixup_portal_edges(float intf[FIELD_RES_R][FIELD_RES_C], struct flow_field *inout_flow,
                               const struct portal *port)
{
    bool up    = port->connected->chunk.r < port->chunk.r;
    bool down  = port->connected->chunk.r > port->chunk.r;
    bool left  = port->connected->chunk.c < port->chunk.c;
    bool right = port->connected->chunk.c > port->chunk.c;
    assert(up ^ down ^ left ^ right);

    for(int r = 0; r < FIELD_RES_R; r++) {
        for(int c = 0; c < FIELD_RES_C; c++) {

            if(intf[r][c] == 0.0f) {

                if(up)
                    inout_flow->field[r][c].dir_idx = FD_N;
                else if(down)
                    inout_flow->field[r][c].dir_idx = FD_S;
                else if(left)
                    inout_flow->field[r][c].dir_idx = FD_W;
                else if(right)
                    inout_flow->field[r][c].dir_idx = FD_E;
                else
                    assert(0);
            }
        }
    }
}

static void chunk_bounds(vec3_t map_pos, struct coord chunk_coord, struct box_xz *out)
{
    size_t chunk_x_dim = TILES_PER_CHUNK_WIDTH * X_COORDS_PER_TILE;
    size_t chunk_z_dim = TILES_PER_CHUNK_HEIGHT * Z_COORDS_PER_TILE;

    int x_offset = -(chunk_coord.c * chunk_x_dim);
    int z_offset =  (chunk_coord.r * chunk_z_dim);

    out->x_max = map_pos.x + x_offset;
    out->x_min = out->x_max - chunk_x_dim;

    out->z_min = map_pos.z + z_offset;
    out->z_max = out->z_min + chunk_z_dim;
}

static struct coord tile_for_pos(const struct box_xz *bounds, vec2_t xz_pos)
{
    assert(xz_pos.x >= bounds->x_min && xz_pos.x <= bounds->x_max);
    assert(xz_pos.z >= bounds->z_min && xz_pos.z <= bounds->z_max);

    assert(FIELD_RES_R % TILES_PER_CHUNK_HEIGHT == 0);
    assert(FIELD_RES_C % TILES_PER_CHUNK_WIDTH == 0);

    size_t nav_tile_width = X_COORDS_PER_TILE / (FIELD_RES_C / TILES_PER_CHUNK_WIDTH);
    size_t nav_tile_height = Z_COORDS_PER_TILE / (FIELD_RES_R / TILES_PER_CHUNK_HEIGHT);

    struct coord ret = (struct coord){
        (xz_pos.z - bounds->z_min) / nav_tile_height,
        FIELD_RES_C - (xz_pos.x - bounds->x_min) / nav_tile_width,
    };
    ret.r = MIN(ret.r, FIELD_RES_R-1);
    ret.c = MIN(ret.c, FIELD_RES_C-1);

    return ret;
}

static bool enemy_ent(int faction_id, const struct entity *ent)
{
    if(ent->faction_id == faction_id)
        return false;
    if(!(ent->flags & ENTITY_FLAG_COMBATABLE))
        return false;

    enum diplomacy_state ds;
    bool result = G_GetDiplomacyState(faction_id, ent->faction_id, &ds);
    assert(result);
    return (ds == DIPLOMACY_STATE_WAR);
}

/*****************************************************************************/
/* EXTERN FUNCTIONS                                                          */
/*****************************************************************************/

ff_id_t N_FlowField_ID(struct coord chunk, struct field_target target)
{
    if(target.type == TARGET_PORTAL) {

        return (((uint64_t)target.type)                 << 56)
             | (((uint64_t)target.port->endpoints[0].r) << 40)
             | (((uint64_t)target.port->endpoints[0].c) << 32)
             | (((uint64_t)target.port->endpoints[1].r) << 24)
             | (((uint64_t)target.port->endpoints[1].c) << 16)
             | (((uint64_t)chunk.r)                     <<  8)
             | (((uint64_t)chunk.c)                     <<  0);

    }else if(target.type == TARGET_TILE){

        return (((uint64_t)target.type)                 << 56)
             | (((uint64_t)target.tile.r)               << 24)
             | (((uint64_t)target.tile.c)               << 16)
             | (((uint64_t)chunk.r)                     <<  8)
             | (((uint64_t)chunk.c)                     <<  0);

    }else if(target.type == TARGET_ENEMIES){

        return (((uint64_t)target.type)                 << 56)
             | (((uint64_t)target.enemies.faction_id)   << 24)
             | (((uint64_t)chunk.r)                     <<  8)
             | (((uint64_t)chunk.c)                     <<  0);
    }else
        assert(0);
}

void N_FlowFieldInit(struct coord chunk_coord, const void *nav_private, struct flow_field *out)
{
    for(int r = 0; r < FIELD_RES_R; r++) {
        for(int c = 0; c < FIELD_RES_C; c++) {

            out->field[r][c].dir_idx = FD_NONE;
        }
    }
    out->chunk = chunk_coord;
}

void N_FlowFieldUpdate(const struct nav_chunk *chunk, struct field_target target, 
                       struct flow_field *inout_flow)
{
    pq_coord_t frontier;
    pq_coord_init(&frontier);

    float integration_field[FIELD_RES_R][FIELD_RES_C];
    for(int r = 0; r < FIELD_RES_R; r++)
        for(int c = 0; c < FIELD_RES_C; c++)
            integration_field[r][c] = INFINITY;

    switch(target.type) {
    case TARGET_PORTAL: {
        
        for(int r = target.port->endpoints[0].r; r <= target.port->endpoints[1].r; r++) {
            for(int c = target.port->endpoints[0].c; c <= target.port->endpoints[1].c; c++) {

                pq_coord_push(&frontier, 0.0f, (struct coord){r, c});
                integration_field[r][c] = 0.0f;
            }
        }
        break;
    }
    case TARGET_TILE: {

        pq_coord_push(&frontier, 0.0f, target.tile);
        integration_field[target.tile.r][target.tile.c] = 0.0f;
        break;
    }
    case TARGET_ENEMIES: {

        struct box_xz bounds;
        chunk_bounds(target.enemies.map_pos, target.enemies.chunk, &bounds);

        struct entity *ents[MAX_ENTS_PER_CHUNK];
        size_t num_ents = G_Pos_EntsInRect(
            (vec2_t){bounds.x_min, bounds.z_min},
            (vec2_t){bounds.x_max, bounds.z_max},
            ents, ARR_SIZE(ents)
        );
        assert(num_ents);

        bool has_enemy[FIELD_RES_R][FIELD_RES_C] = {0};
        for(int i = 0; i < num_ents; i++) {
        
            struct entity *curr = ents[i];
            if(!enemy_ent(target.enemies.faction_id, curr))
                continue;

            struct coord tile = tile_for_pos(&bounds, G_Pos_GetXZ(curr->uid));
            has_enemy[tile.r][tile.c] = true;
        }

        for(int r = 0; r < FIELD_RES_R; r++) {
            for(int c = 0; c < FIELD_RES_C; c++) {
            
                if(!has_enemy[r][c])
                    continue;
                pq_coord_push(&frontier, 0.0f, (struct coord){r,c}); 
                integration_field[r][c] = 0.0f;
            }
        }
        break;
    }
    default: assert(0);
    }

    assert(pq_size(&frontier) > 0);
    build_integration_field(&frontier, chunk, integration_field);
    build_flow_field(integration_field, inout_flow);

    if(target.type == TARGET_PORTAL)
        fixup_portal_edges(integration_field, inout_flow, target.port); 

    pq_coord_destroy(&frontier);
}

void N_LOSFieldCreate(dest_id_t id, struct coord chunk_coord, struct tile_desc target,
                      const struct nav_private *priv, vec3_t map_pos, 
                      struct LOS_field *out_los, const struct LOS_field *prev_los)
{
    out_los->chunk = chunk_coord;
    memset(out_los->field, 0x00, sizeof(out_los->field));

    pq_coord_t frontier;
    pq_coord_init(&frontier);
    const struct nav_chunk *chunk = &priv->chunks[chunk_coord.r * priv->width + chunk_coord.c];

    float integration_field[FIELD_RES_R][FIELD_RES_C];
    for(int r = 0; r < FIELD_RES_R; r++)
        for(int c = 0; c < FIELD_RES_C; c++)
            integration_field[r][c] = INFINITY;

    /* Case 1: LOS for the destination chunk */
    if(chunk_coord.r == target.chunk_r && chunk_coord.c == target.chunk_c) {

        pq_coord_push(&frontier, 0.0f, (struct coord){target.tile_r, target.tile_c});
        integration_field[target.tile_r][target.tile_c] = 0.0f;
        assert(NULL == prev_los);

    /* Case 2: LOS for a chunk other than the destination chunk 
     * In this case, carry over the 'visible' and 'wavefront blocked' flags from 
     * the shared edge with the previous chunk. Then treat each tile with the 
     * 'wavefront blocked' flag as a LOS corner. This will make the LOS seamless
     * accross chunk borders. */
    }else{
        
        assert(prev_los);
        if(prev_los->chunk.r < chunk_coord.r) {

            for(int c = 0; c < FIELD_RES_C; c++) {

                out_los->field[0][c] = prev_los->field[FIELD_RES_R-1][c];
                if(out_los->field[0][c].wavefront_blocked) {

                    struct tile_desc src_desc = (struct tile_desc) {chunk_coord.r, chunk_coord.c, 0, c};
                    create_wavefront_blocked_line(target, src_desc, priv, map_pos, out_los);
                }
                if(out_los->field[0][c].visible) {

                    pq_coord_push(&frontier, 0.0f, (struct coord){0, c});
                    integration_field[0][c] = 0.0f;
                }
            }
        }else if(prev_los->chunk.r > chunk_coord.r) {

            for(int c = 0; c < FIELD_RES_C; c++) {

                out_los->field[FIELD_RES_R-1][c] = prev_los->field[0][c];
                if(out_los->field[FIELD_RES_R-1][c].wavefront_blocked) {

                    struct tile_desc src_desc = (struct tile_desc) {chunk_coord.r, chunk_coord.c, FIELD_RES_R-1, c};
                    create_wavefront_blocked_line(target, src_desc, priv, map_pos, out_los);
                }
                if(out_los->field[FIELD_RES_R-1][c].visible) {

                    pq_coord_push(&frontier, 0.0f, (struct coord){FIELD_RES_R-1, c});
                    integration_field[FIELD_RES_R-1][c] = 0.0f;
                }
            }
        }else if(prev_los->chunk.c < chunk_coord.c) {

            for(int r = 0; r < FIELD_RES_R; r++) {

                out_los->field[r][0] = prev_los->field[r][FIELD_RES_C-1];
                if(out_los->field[r][0].wavefront_blocked) {

                    struct tile_desc src_desc = (struct tile_desc) {chunk_coord.r, chunk_coord.c, r, 0};
                    create_wavefront_blocked_line(target, src_desc, priv, map_pos, out_los);
                }
                if(out_los->field[r][0].visible) {

                    pq_coord_push(&frontier, 0.0f, (struct coord){r, 0});
                    integration_field[r][0] = 0.0f;
                }
            }
        }else if(prev_los->chunk.c > chunk_coord.c) {

            for(int r = 0; r < FIELD_RES_R; r++) {

                out_los->field[r][FIELD_RES_C-1] = prev_los->field[r][0];
                if(out_los->field[r][FIELD_RES_C-1].wavefront_blocked) {

                    struct tile_desc src_desc = (struct tile_desc) {chunk_coord.r, chunk_coord.c, r, FIELD_RES_C-1};
                    create_wavefront_blocked_line(target, src_desc, priv, map_pos, out_los);
                }
                if(out_los->field[r][FIELD_RES_C-1].visible) {

                    pq_coord_push(&frontier, 0.0f, (struct coord){r, FIELD_RES_C-1});
                    integration_field[r][FIELD_RES_C-1] = 0.0f;
                }
            }
        }else{
            assert(0);
        }
    }

    while(pq_size(&frontier) > 0) {

        struct coord curr;
        pq_coord_pop(&frontier, &curr);

        struct coord neighbours[8];
        uint8_t neighbour_costs[8];
        int num_neighbours = neighbours_grid_LOS(chunk, out_los, curr, neighbours, neighbour_costs);

        for(int i = 0; i < num_neighbours; i++) {

            int nr = neighbours[i].r, nc = neighbours[i].c;
            if(neighbour_costs[i] > 1) {
                
                if(!is_LOS_corner(neighbours[i], chunk->cost_base, chunk->blockers))
                    continue;

                struct tile_desc src_desc = (struct tile_desc) {
                    .chunk_r = chunk_coord.r,
                    .chunk_c = chunk_coord.c,
                    .tile_r = neighbours[i].r,
                    .tile_c = neighbours[i].c
                };
                create_wavefront_blocked_line(target, src_desc, priv, map_pos, out_los);
            }else{

                float new_cost = integration_field[curr.r][curr.c] + 1;
                out_los->field[nr][nc].visible = 1;

                if(new_cost < integration_field[neighbours[i].r][neighbours[i].c]) {

                    integration_field[nr][nc] = new_cost;
                    if(!pq_coord_contains(&frontier, neighbours[i]))
                        pq_coord_push(&frontier, new_cost, neighbours[i]);
                }
            }
        }
    }
    pq_coord_destroy(&frontier);

    /* Add a single tile-wide padding of invisible tiles around the wavefront. This is 
     * because we want to be conservative and not mark any tiles visible from which we
     * can't raycast to the destination point from any point within the tile without 
     * the ray going over impassable terrain. This is a nice property for the movement
     * code. */
    pad_wavefront(out_los);
}

