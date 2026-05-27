local ffi = require("ffi")

ffi.cdef([[
typedef struct ivec2 {
    int x;
    int y;
} ivec2;

typedef struct fvec2 {
    int x;
    int y;
} fvec2;

typedef enum {
    ANIMATION_UI_MOVE_CELL,
    ANIMATION_UI_MOVE_ENTITY,
    ANIMATION_UI_HIGHLIGHT_ENTITY,
    ANIMATION_UI_MOVE_ATTACK_ENTITY,
    ANIMATION_UNIT_IDLE,
    ANIMATION_UNIT_MOVE,
    ANIMATION_UNIT_MOVE_SLOW,
    ANIMATION_UNIT_MOVE_CANNON,
    ANIMATION_UNIT_ATTACK,
    ANIMATION_SOLDIER_RANGED_ATTACK,
    ANIMATION_SOLDIER_CHARGE,
    ANIMATION_CANNON_ATTACK,
    ANIMATION_UNIT_MINE,
    ANIMATION_UNIT_BUILD,
    ANIMATION_UNIT_DEATH,
    ANIMATION_UNIT_DEATH_FADE,
    ANIMATION_CANNON_DEATH,
    ANIMATION_CANNON_DEATH_FADE,
    ANIMATION_BALLOON_MOVE,
    ANIMATION_BALLOON_DEATH_START,
    ANIMATION_BALLOON_DEATH,
    ANIMATION_BALLOON_DEATH_FADE,
    ANIMATION_RALLY_FLAG,
    ANIMATION_PARTICLE_SPARKS,
    ANIMATION_PARTICLE_BUNKER_COWBOY,
    ANIMATION_PARTICLE_EXPLOSION,
    ANIMATION_PARTICLE_CANNON_EXPLOSION,
    ANIMATION_PARTICLE_BLEED,
    ANIMATION_SMITH_BEGIN,
    ANIMATION_SMITH_LOOP,
    ANIMATION_SMITH_END,
    ANIMATION_WORKSHOP,
    ANIMATION_FIRE_START,
    ANIMATION_FIRE_BURN,
    ANIMATION_MINE_PRIME,
    ANIMATION_NAME_COUNT
} AnimationName;

typedef struct Animation {
    AnimationName name;
    uint32_t timer;
    uint32_t frame_index;
    ivec2 frame;
    int loops_remaining;
} Animation;

typedef uint16_t EntityId;

typedef enum {
    ENTITY_GOLDMINE,
    ENTITY_CRATE,
    ENTITY_SWITCH,
    ENTITY_MINER,
    ENTITY_COWBOY,
    ENTITY_BANDIT,
    ENTITY_WAGON,
    ENTITY_JOCKEY,
    ENTITY_SAPPER,
    ENTITY_PYRO,
    ENTITY_SOLDIER,
    ENTITY_CANNON,
    ENTITY_DETECTIVE,
    ENTITY_BALLOON,
    ENTITY_HALL,
    ENTITY_HOUSE,
    ENTITY_SALOON,
    ENTITY_BUNKER,
    ENTITY_WORKSHOP,
    ENTITY_SMITH,
    ENTITY_COOP,
    ENTITY_BARRACKS,
    ENTITY_SHERIFFS,
    ENTITY_LANDMINE,
    ENTITY_TYPE_COUNT
} EntityType;

typedef enum {
    MODE_UNIT_IDLE,
    MODE_UNIT_BLOCKED,
    MODE_UNIT_MOVE,
    MODE_UNIT_MOVE_FINISHED,
    MODE_UNIT_BUILD,
    MODE_UNIT_BUILD_ASSIST,
    MODE_UNIT_REPAIR,
    MODE_UNIT_ATTACK_WINDUP,
    MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP,
    MODE_UNIT_SOLDIER_CHARGE,
    MODE_UNIT_IN_MINE,
    MODE_UNIT_PYRO_THROW,
    MODE_UNIT_DEATH,
    MODE_UNIT_DEATH_FADE,
    MODE_UNIT_BALLOON_DEATH_START,
    MODE_UNIT_BALLOON_DEATH,
    MODE_BUILDING_IN_PROGRESS,
    MODE_BUILDING_FINISHED,
    MODE_BUILDING_DESTROYED,
    MODE_MINE_ARM,
    MODE_MINE_PRIME,
    MODE_GOLDMINE,
    MODE_GOLDMINE_COLLAPSED,
    MODE_GOLDMINE_RIGGED,
    MODE_SWITCH_UP,
    MODE_SWITCH_DOWN,
    MODE_COUNT
} EntityMode;

typedef enum {
    TARGET_NONE,
    TARGET_CELL,
    TARGET_ENTITY,
    TARGET_ATTACK_CELL,
    TARGET_ATTACK_ENTITY,
    TARGET_REPAIR,
    TARGET_UNLOAD,
    TARGET_MOLOTOV,
    TARGET_BUILD,
    TARGET_BUILD_ASSIST,
    TARGET_PATROL,
    TARGET_TYPE_COUNT
} TargetType;

typedef struct TargetBuild {
    ivec2 unit_cell;
    ivec2 building_cell;
    EntityType building_type;
} TargetBuild;

typedef struct TargetPatrol {
    ivec2 cell_a;
    ivec2 cell_b;
    uint32_t padding;
} TargetPatrol;

typedef struct Target {
    TargetType type;
    EntityId id;
    uint8_t padding;
    uint8_t padding2;
    ivec2 cell;
    union {
        TargetBuild build;
        TargetPatrol patrol;
    };
} Target;

typedef enum {
    BUILDING_QUEUE_ITEM_UNIT,
    BUILDING_QUEUE_ITEM_UPGRADE
} BuildingQueueItemType;

typedef struct BuildingQueueItem {
    BuildingQueueItemType type;
    union {
        EntityType unit_type;
        uint32_t upgrade;
    };
} BuildingQueueItem;

typedef enum {
    DIRECTION_NORTH,
    DIRECTION_NORTHEAST,
    DIRECTION_EAST,
    DIRECTION_SOUTHEAST,
    DIRECTION_SOUTH,
    DIRECTION_SOUTHWEST,
    DIRECTION_WEST,
    DIRECTION_NORTHWEST,
    DIRECTION_COUNT
} Direction;

typedef struct Entity {
    EntityType type;
    EntityMode mode;
    uint8_t player_id;
    uint8_t padding[3];
    uint32_t flags;

    ivec2 cell;
    fvec2 position;
    Direction direction;

    int health;
    uint32_t energy;
    uint32_t timer;
    uint32_t energy_regen_timer;
    uint32_t health_regen_timer;

    Animation animation;
    EntityId garrisoned_units[4];
    uint32_t garrisoned_units_size;
    EntityId garrison_id;

    EntityId goldmine_id;
    uint32_t gold_held;

    Target target;
    uint32_t target_queue_index;

    EntityId moving_shot_target_id;
    uint16_t moving_shot_attack_windup_timer;

    uint32_t path_index;
    uint32_t pathfind_attempts;

    // This is a FixedVector because players can remove items from the middle of a building queue
    BuildingQueueItem building_queue[5];
    uint32_t building_queue_size;
    ivec2 rally_point;

    uint32_t cooldown_timer;
    ivec2 attack_move_cell;

    uint32_t taking_damage_counter;
    uint32_t taking_damage_timer;
    uint32_t fire_damage_timer;
} Entity;

const Entity* get_by_id(EntityId entity_id);
const Entity* get_by_index(uint32_t entity_index);
uint32_t get_count();
EntityId get_id_of(uint32_t entity_index);
uint32_t get_index_of(EntityId entity_id);
]])

local entities = ffi.C
return entities
