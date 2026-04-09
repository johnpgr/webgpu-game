#include "game/game_entity.h"

#include "../../assets/sprites/atlas.h"
#include "assets/assets_atlas.h"

struct WorldSpriteCandidate {
    vec2 uv_min;
    vec2 uv_max;
    vec2 position;
    vec2 size;
    vec4 tint;
    f32 feet_y;
    u32 render_layer;
    u32 entity_index;
};

internal f32 abs_f32(f32 value) {
    return (value < 0.0f) ? -value : value;
}

internal int compare_world_sprite_candidates(void* lhs, void* rhs) {
    WorldSpriteCandidate* a = (WorldSpriteCandidate*)lhs;
    WorldSpriteCandidate* b = (WorldSpriteCandidate*)rhs;

    if(a->render_layer != b->render_layer) {
        return (a->render_layer < b->render_layer) ? -1 : 1;
    }
    if(a->feet_y != b->feet_y) {
        return (a->feet_y > b->feet_y) ? -1 : 1;
    }
    if(a->entity_index != b->entity_index) {
        return (a->entity_index < b->entity_index) ? -1 : 1;
    }
    return 0;
}

internal void sort_world_sprite_candidates(
    WorldSpriteCandidate* candidates,
    u32 candidate_count
) {
    for(u32 i = 1; i < candidate_count; ++i) {
        WorldSpriteCandidate value = candidates[i];
        u32 insert_index = i;
        while(insert_index > 0) {
            if(compare_world_sprite_candidates(
                   &candidates[insert_index - 1],
                   &value
               ) <= 0) {
                break;
            }

            candidates[insert_index] = candidates[insert_index - 1];
            insert_index--;
        }
        candidates[insert_index] = value;
    }
}

internal void init_free_list(GameWorld* world) {
    ASSERT(world != nullptr, "world must not be null");
    ASSERT(world->entities != nullptr, "entities array must not be null");

    for(u32 i = 1; i < world->capacity - 1; ++i) {
        world->entities[i].next_free_index = i + 1;
        world->entities[i].generation = 0;
    }
    world->entities[world->capacity - 1].next_free_index = 0;
    world->entities[world->capacity - 1].generation = 0;

    world->free_list_head = (world->capacity > 1) ? 1 : 0;
}

internal u32 pop_free_slot(GameWorld* world) {
    if(world->free_list_head == 0) {
        return 0;
    }
    u32 index = world->free_list_head;
    world->free_list_head = world->entities[index].next_free_index;
    return index;
}

internal void push_free_slot(GameWorld* world, u32 index) {
    ASSERT(index > 0 && index < world->capacity, "index out of range");
    world->entities[index].next_free_index = world->free_list_head;
    world->free_list_head = index;
}

internal void detach_from_tree(GameWorld* world, Entity* entity) {
    if(!entity_handle_is_valid(entity->parent)) {
        return;
    }

    Entity* parent = world_get_entity(world, entity->parent);
    if(parent == nullptr) {
        return;
    }

    if(entity_handle_is_valid(entity->prev_sibling)) {
        Entity* prev = world_get_entity(world, entity->prev_sibling);
        if(prev != nullptr) {
            prev->next_sibling = entity->next_sibling;
        }
    } else {
        parent->first_child = entity->next_sibling;
    }

    if(entity_handle_is_valid(entity->next_sibling)) {
        Entity* next = world_get_entity(world, entity->next_sibling);
        if(next != nullptr) {
            next->prev_sibling = entity->prev_sibling;
        }
    } else {
        parent->last_child = entity->prev_sibling;
    }

    entity->parent = EntityHandle{0, 0};
    entity->next_sibling = EntityHandle{0, 0};
    entity->prev_sibling = EntityHandle{0, 0};
}

internal void update_visual_animation_state(GameWorld* world) {
    for(u32 i = 1; i < world->capacity; ++i) {
        Entity* entity = &world->entities[i];
        if(entity->generation == 0) {
            continue;
        }
        if(!entity_has_property(
               entity->properties,
               ENTITY_PROPERTY_DIRECTIONAL_ANIMATION
           )) {
            continue;
        }

        f32 abs_x = abs_f32(entity->velocity.x);
        f32 abs_y = abs_f32(entity->velocity.y);
        bool moving = abs_x > 0.01f || abs_y > 0.01f;

        if(moving) {
            if(abs_x >= abs_y) {
                entity->visual_direction = ENTITY_VISUAL_DIRECTION_SIDE;
                if(abs_x > 0.01f) {
                    entity->flip_x = entity->velocity.x < 0.0f;
                }
            } else {
                entity->visual_direction = (entity->velocity.y > 0.0f)
                                               ? ENTITY_VISUAL_DIRECTION_UP
                                               : ENTITY_VISUAL_DIRECTION_DOWN;
            }
        }

        u32 next_animation = entity->animation_id;
        if(moving) {
            switch(entity->visual_direction) {
                case ENTITY_VISUAL_DIRECTION_SIDE:
                    next_animation = entity->move_animation_side;
                    break;
                case ENTITY_VISUAL_DIRECTION_UP:
                    next_animation = entity->move_animation_up;
                    break;
                case ENTITY_VISUAL_DIRECTION_DOWN:
                    next_animation = entity->move_animation_down;
                    break;
            }
        } else {
            switch(entity->visual_direction) {
                case ENTITY_VISUAL_DIRECTION_SIDE:
                    next_animation = entity->idle_animation_side;
                    break;
                case ENTITY_VISUAL_DIRECTION_UP:
                    next_animation = entity->idle_animation_up;
                    break;
                case ENTITY_VISUAL_DIRECTION_DOWN:
                    next_animation = entity->idle_animation_down;
                    break;
            }
        }

        if(next_animation != entity->animation_id) {
            entity->animation_id = next_animation;
            entity->animation_time = 0.0f;
        }
    }
}

internal void update_movement(GameWorld* world, f64 dt) {
    for(u32 i = 1; i < world->capacity; ++i) {
        Entity* entity = &world->entities[i];
        if(entity->generation == 0) {
            continue;
        }
        if(!entity_has_property(entity->properties, ENTITY_PROPERTY_MOVES)) {
            continue;
        }

        entity->position.x += entity->velocity.x * (f32)dt;
        entity->position.y += entity->velocity.y * (f32)dt;
    }
}

internal void update_animation_time(GameWorld* world, f64 dt) {
    for(u32 i = 1; i < world->capacity; ++i) {
        Entity* entity = &world->entities[i];
        if(entity->generation == 0) {
            continue;
        }
        if(!entity_has_property(entity->properties, ENTITY_PROPERTY_ANIMATED)) {
            continue;
        }

        entity->animation_time += (f32)(dt * entity->animation_rate);
    }
}

GameWorld* create_game_world(Arena* arena, u32 max_entities) {
    ASSERT(arena != nullptr, "arena must not be null");
    ASSERT(
        max_entities > 1,
        "max_entities must be at least 2 (for reserved slot 0)"
    );

    GameWorld* world = push_struct(arena, GameWorld);
    world->arena = arena;
    world->capacity = max_entities;
    world->entities = push_array(arena, Entity, max_entities);
    world->free_list_head = 0;
    world->active_count = 0;
    world->camera_target = EntityHandle{0, 0};
    world->time = 0.0;

    world->entities[0].generation = 0;
    world->entities[0].next_free_index = 0;

    init_free_list(world);

    return world;
}

EntityHandle world_spawn_entity(GameWorld* world) {
    ASSERT(world != nullptr, "world must not be null");

    u32 index = pop_free_slot(world);
    if(index == 0) {
        LOG_ERROR("Entity pool exhausted (capacity=%u)", world->capacity);
        return EntityHandle{0, 0};
    }

    Entity* entity = &world->entities[index];
    u32 generation = entity->generation;
    if(generation == 0) {
        generation = 1;
    }

    *entity = Entity{};
    entity->generation = generation;
    entity->next_free_index = 0;
    entity->render_layer = ENTITY_RENDER_LAYER_SORTED;
    entity->visual_direction = ENTITY_VISUAL_DIRECTION_SIDE;
    entity->tint = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    entity->animation_rate = 1.0f;

    world->active_count++;

    return EntityHandle{index, entity->generation};
}

void world_destroy_entity(GameWorld* world, EntityHandle handle) {
    ASSERT(world != nullptr, "world must not be null");

    if(!entity_handle_is_valid(handle)) {
        return;
    }
    if(handle.index >= world->capacity) {
        return;
    }

    Entity* entity = &world->entities[handle.index];
    if(entity->generation != handle.generation) {
        return;
    }

    detach_from_tree(world, entity);

    entity->generation++;
    if(entity->generation == 0) {
        entity->generation = 1;
    }

    push_free_slot(world, handle.index);
    world->active_count--;
}

Entity* world_get_entity(GameWorld* world, EntityHandle handle) {
    ASSERT(world != nullptr, "world must not be null");

    if(!entity_handle_is_valid(handle)) {
        return nullptr;
    }
    if(handle.index >= world->capacity) {
        return nullptr;
    }

    Entity* entity = &world->entities[handle.index];
    if(entity->generation != handle.generation) {
        return nullptr;
    }

    return entity;
}

bool world_entity_is_alive(GameWorld* world, EntityHandle handle) {
    return world_get_entity(world, handle) != nullptr;
}

void world_update(GameWorld* world, f64 dt) {
    ASSERT(world != nullptr, "world must not be null");

    world->time += dt;

    update_movement(world, dt);
    update_visual_animation_state(world);
    update_animation_time(world, dt);
}

void world_extract_render(GameWorld* world, RenderFrame* frame) {
    ASSERT(world != nullptr, "world must not be null");
    ASSERT(frame != nullptr, "frame must not be null");

    if(entity_handle_is_valid(world->camera_target)) {
        Entity* target = world_get_entity(world, world->camera_target);
        if(target != nullptr) {
            frame->world_sprites.camera.position.x = target->position.x;
        }
    }

    Temp temp = temp_begin(world->arena);
    WorldSpriteCandidate* candidates = push_array_no_zero(
        world->arena,
        WorldSpriteCandidate,
        world->active_count
    );
    u32 candidate_count = 0;

    for(u32 i = 1; i < world->capacity; ++i) {
        Entity* entity = &world->entities[i];
        if(entity->generation == 0) {
            continue;
        }
        if(!entity_has_property(
               entity->properties,
               ENTITY_PROPERTY_WORLD_SPRITE
           )) {
            continue;
        }

        AtlasFrame atlas_frame = {};
        u32 frame_count = atlas_animation_frame_count(entity->animation_id);
        if(frame_count > 0) {
            u32 frame_index = 0;
            if(entity_has_property(
                   entity->properties,
                   ENTITY_PROPERTY_ANIMATED
               ) &&
               frame_count > 1) {
                frame_index =
                    (u32)(entity->animation_time * 8.0f) % frame_count;
            }
            atlas_frame =
                atlas_animation_frame(entity->animation_id, frame_index);
        }

        if(atlas_frame.width_px <= 0.0f || atlas_frame.height_px <= 0.0f) {
            continue;
        }

        WorldSpriteCandidate* candidate = &candidates[candidate_count++];
        candidate->uv_min = atlas_frame.uv_min;
        candidate->uv_max = atlas_frame.uv_max;
        if(entity->flip_x) {
            f32 temp_u = candidate->uv_min.x;
            candidate->uv_min.x = candidate->uv_max.x;
            candidate->uv_max.x = temp_u;
        }
        candidate->position = entity->position - entity->sprite_anchor;
        candidate->size = entity->size;
        candidate->tint = entity->tint;
        candidate->feet_y = entity->position.y + entity->depth;
        candidate->render_layer = entity->render_layer;
        candidate->entity_index = i;
        entity->feet_y = entity->position.y;
    }

    if(candidate_count > 1) {
        sort_world_sprite_candidates(candidates, candidate_count);
    }

    f32 far_depth = 0.75f;
    f32 near_depth = 0.05f;
    for(u32 i = 0; i < candidate_count; ++i) {
        f32 t =
            (candidate_count > 1) ? (f32)i / (f32)(candidate_count - 1) : 0.0f;
        f32 depth = far_depth + (near_depth - far_depth) * t;
        WorldSpriteCandidate* candidate = &candidates[i];
        push_world_sprite(
            &frame->world_sprites,
            candidate->uv_min,
            candidate->uv_max,
            candidate->position,
            candidate->size,
            depth,
            candidate->tint
        );
    }

    temp_end(temp);
}

EntityHandle spawn_animated_prop(
    GameWorld* world,
    vec2 position,
    u32 animation_id,
    vec2 size,
    f32 depth
) {
    EntityHandle handle = world_spawn_entity(world);
    if(!entity_handle_is_valid(handle)) {
        return handle;
    }

    Entity* entity = world_get_entity(world, handle);
    entity->properties =
        entity_set_property(entity->properties, ENTITY_PROPERTY_WORLD_SPRITE);
    entity->properties =
        entity_set_property(entity->properties, ENTITY_PROPERTY_ANIMATED);

    entity->position = position;
    entity->size = size;
    entity->sprite_anchor = vec2(0.0f, 0.0f);
    entity->depth = depth;
    entity->animation_id = animation_id;

    return handle;
}

EntityHandle spawn_moving_entity(
    GameWorld* world,
    vec2 position,
    vec2 velocity,
    u32 animation_id,
    vec2 size,
    f32 depth
) {
    EntityHandle handle = world_spawn_entity(world);
    if(!entity_handle_is_valid(handle)) {
        return handle;
    }

    Entity* entity = world_get_entity(world, handle);
    entity->properties =
        entity_set_property(entity->properties, ENTITY_PROPERTY_WORLD_SPRITE);
    entity->properties =
        entity_set_property(entity->properties, ENTITY_PROPERTY_ANIMATED);
    entity->properties =
        entity_set_property(entity->properties, ENTITY_PROPERTY_MOVES);

    entity->position = position;
    entity->velocity = velocity;
    entity->size = size;
    entity->sprite_anchor = vec2(size.x * 0.5f, 6.0f);
    entity->depth = depth;
    entity->animation_id = animation_id;

    return handle;
}

EntityHandle spawn_player_entity(
    GameWorld* world,
    vec2 position,
    u32 animation_id,
    vec2 size
) {
    EntityHandle handle = world_spawn_entity(world);
    if(!entity_handle_is_valid(handle)) {
        return handle;
    }

    Entity* entity = world_get_entity(world, handle);
    entity->properties =
        entity_set_property(entity->properties, ENTITY_PROPERTY_WORLD_SPRITE);
    entity->properties =
        entity_set_property(entity->properties, ENTITY_PROPERTY_ANIMATED);
    entity->properties =
        entity_set_property(entity->properties, ENTITY_PROPERTY_MOVES);
    entity->properties =
        entity_set_property(entity->properties, ENTITY_PROPERTY_CAMERA_TARGET);
    entity->properties =
        entity_set_property(entity->properties, ENTITY_PROPERTY_PLAYER_CONTROL);

    entity->position = position;
    entity->size = size;
    entity->sprite_anchor = vec2(size.x * 0.5f, 6.0f);
    entity->animation_id = animation_id;

    world->camera_target = handle;
    return handle;
}
