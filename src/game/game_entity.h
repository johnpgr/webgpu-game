#pragma once

#include "base/base_mod.h"
#include "draw/draw_mod.h"

// #######################################################
//                  Entity Handle
// #######################################################

// Compact, stable handle to reference entities across frames.
// Zero is invalid (ZII-friendly).

struct EntityHandle {
    u32 index;
    u32 generation;
};

inline EntityHandle make_entity_handle(u32 index, u32 generation) {
    return EntityHandle{index, generation};
}

inline bool entity_handle_is_valid(EntityHandle handle) {
    return handle.index != 0 && handle.generation != 0;
}

inline bool entity_handle_equals(EntityHandle a, EntityHandle b) {
    return a.index == b.index && a.generation == b.generation;
}
// #######################################################
//              Entity Properties
// #######################################################

// Property bitflags for feature composition. Each bit maps to one codepath.

enum EntityProperty : u64 {
    ENTITY_PROPERTY_WORLD_SPRITE = (1ULL << 0),
    ENTITY_PROPERTY_ANIMATED = (1ULL << 1),
    ENTITY_PROPERTY_MOVES = (1ULL << 2),
    ENTITY_PROPERTY_CAMERA_TARGET = (1ULL << 3),
    ENTITY_PROPERTY_PLAYER_CONTROL = (1ULL << 4),
    ENTITY_PROPERTY_DIRECTIONAL_ANIMATION = (1ULL << 5),
};

inline bool entity_has_property(u64 properties, EntityProperty prop) {
    return (properties & (u64)prop) != 0;
}

inline u64 entity_set_property(u64 properties, EntityProperty prop) {
    return properties | (u64)prop;
}

inline u64 entity_clear_property(u64 properties, EntityProperty prop) {
    return properties & ~(u64)prop;
}

enum EntityRenderLayer : u32 {
    ENTITY_RENDER_LAYER_BACK_PROP,
    ENTITY_RENDER_LAYER_SORTED,
    ENTITY_RENDER_LAYER_FRONT_PROP,
};

enum EntityVisualDirection : u8 {
    ENTITY_VISUAL_DIRECTION_SIDE,
    ENTITY_VISUAL_DIRECTION_UP,
    ENTITY_VISUAL_DIRECTION_DOWN,
};

// #######################################################
//                  Entity
// #######################################################

// One flat struct for all entity data. Zero is valid/default state (ZII).
// Most fields are zero for most entities; only relevant ones are read.

struct Entity {
    // Lifecycle fields
    u32 generation;      // increments on reuse; 0 = slot is free
    u32 next_free_index; // linked list for free slots; 0 = end of list

    // Property flags
    u64 properties;

    // Spatial
    vec2 position;
    vec2 velocity;
    vec2 size;
    vec2 sprite_anchor;
    f32 depth;
    f32 feet_y;
    u32 render_layer;
    u8 visual_direction;
    bool flip_x;

    // Visuals
    vec4 tint;
    u32 animation_id;   // animation to play (if ENTITY_PROPERTY_ANIMATED)
    f32 animation_time; // current time in animation (seconds)
    f32 animation_rate; // playback rate (1.0 = normal)
    u32 idle_animation_side;
    u32 idle_animation_up;
    u32 idle_animation_down;
    u32 move_animation_side;
    u32 move_animation_up;
    u32 move_animation_down;

    // Tree structure
    EntityHandle parent;
    EntityHandle first_child;
    EntityHandle last_child;
    EntityHandle next_sibling;
    EntityHandle prev_sibling;
};

// #######################################################
//                  Game World
// #######################################################

// Container for all entities. Fixed capacity, arena-backed.
struct GameWorld {
    Arena* arena;
    Entity* entities; // entity storage (index 0 is reserved/invalid)
    u32 capacity;
    u32 free_list_head; // index of first free slot (0 = none available)
    u32 active_count;   // number of alive entities
    EntityHandle camera_target;
    f64 time;
};

// #######################################################
//                      API
// #######################################################

// Create a new world with capacity for max_entities entities.
// Capacity is fixed; entities beyond this cannot be spawned.
GameWorld* create_game_world(Arena* arena, u32 max_entities);

// Spawn a new entity, returning a handle. Returns zero handle if at capacity.
EntityHandle world_spawn_entity(GameWorld* world);

// Mark an entity as destroyed. Handle becomes invalid immediately.
void world_destroy_entity(GameWorld* world, EntityHandle handle);

// Get pointer to entity from handle. Returns null if handle is stale/invalid.
Entity* world_get_entity(GameWorld* world, EntityHandle handle);

// Returns true if the handle refers to a currently-alive entity.
bool world_entity_is_alive(GameWorld* world, EntityHandle handle);

// Update all entities for one frame (movement, animation, etc.)
// Also advances world.time by dt.
void world_update(GameWorld* world, f64 dt);

// Extract render data from entities into the render frame.
void world_extract_render(GameWorld* world, RenderFrame* frame);

// #######################################################
//                      Prefabs
// #######################################################

// Helper functions to spawn common entity configurations.
EntityHandle spawn_animated_prop(
    GameWorld* world,
    vec2 position,
    u32 animation_id,
    vec2 size,
    f32 depth
);

EntityHandle spawn_moving_entity(
    GameWorld* world,
    vec2 position,
    vec2 velocity,
    u32 animation_id,
    vec2 size,
    f32 depth
);

EntityHandle spawn_player_entity(
    GameWorld* world,
    vec2 position,
    u32 animation_id,
    vec2 size
);
