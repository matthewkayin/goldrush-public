--- @meta

--- @class scenario
--- @field constants table
scenario = {}

--- @class ivec2
--- @field x number
--- @field y number

scenario.ID_NULL = 4096
scenario.SQUAD_ID_NULL = -1
scenario.CHAT_COLOR_GOLD = 1
scenario.CHAT_COLOR_BLUE = 1
scenario.ALERT_COLOR_GOLD = 1
scenario.CAMERA_MODE_PAN = 2
scenario.upgrade = {}
scenario.upgrade.BAYONETS = 4
scenario.upgrade.PRIVATE_EYE = 8
scenario.upgrade.GETAWAY_BOOTS = 16
scenario.upgrade.IRON_SIGHTS = 32
scenario.upgrade.LAND_MINES = 2
scenario.upgrade.WAGON_ARMOR = 1

scenario.entity_mode = {}
scenario.entity_mode.UNIT_MOVE_FINISHED = 3
scenario.entity_mode.UNIT_SOLDIER_RANGED_ATTACK_WINDUP = 8
scenario.entity_mode.MINE_PRIME = 20
scenario.entity_mode.UNIT_BALLOON_DEATH = 15
scenario.entity_mode.UNIT_BUILD = 4
scenario.entity_mode.MINE_ARM = 19
scenario.entity_mode.GOLDMINE_RIGGED = 23
scenario.entity_mode.UNIT_MOVE = 2
scenario.entity_mode.UNIT_BUILD_ASSIST = 5
scenario.entity_mode.SWITCH_DOWN = 25
scenario.entity_mode.UNIT_IN_MINE = 10
scenario.entity_mode.UNIT_IDLE = 0
scenario.entity_mode.SWITCH_UP = 24
scenario.entity_mode.UNIT_DEATH_FADE = 13
scenario.entity_mode.UNIT_DEATH = 12
scenario.entity_mode.UNIT_BALLOON_DEATH_START = 14
scenario.entity_mode.GOLDMINE = 21
scenario.entity_mode.BUILDING_DESTROYED = 18
scenario.entity_mode.UNIT_REPAIR = 6
scenario.entity_mode.BUILDING_FINISHED = 17
scenario.entity_mode.UNIT_PYRO_THROW = 11
scenario.entity_mode.BUILDING_IN_PROGRESS = 16
scenario.entity_mode.GOLDMINE_COLLAPSED = 22
scenario.entity_mode.UNIT_SOLDIER_CHARGE = 9
scenario.entity_mode.UNIT_ATTACK_WINDUP = 7
scenario.entity_mode.UNIT_BLOCKED = 1

scenario.ALERT_COLOR_PLAYER = 2
scenario.entity_type = {}
scenario.entity_type.BARRACKS = 21
scenario.entity_type.SHERIFFS = 22
scenario.entity_type.CANNON = 11
scenario.entity_type.WAGON = 6
scenario.entity_type.LANDMINE = 23
scenario.entity_type.BALLOON = 13
scenario.entity_type.WORKSHOP = 18
scenario.entity_type.BANDIT = 5
scenario.entity_type.SALOON = 16
scenario.entity_type.BUNKER = 17
scenario.entity_type.PYRO = 9
scenario.entity_type.CRATE = 1
scenario.entity_type.SOLDIER = 10
scenario.entity_type.SWITCH = 2
scenario.entity_type.GOLDMINE = 0
scenario.entity_type.SAPPER = 8
scenario.entity_type.COOP = 20
scenario.entity_type.HOUSE = 15
scenario.entity_type.JOCKEY = 7
scenario.entity_type.COWBOY = 4
scenario.entity_type.HALL = 14
scenario.entity_type.DETECTIVE = 12
scenario.entity_type.SMITH = 19
scenario.entity_type.MINER = 3

scenario.CAMERA_MODE_FREE = 0
scenario.target_type = {}
scenario.target_type.NONE = 0
scenario.target_type.REPAIR = 5
scenario.target_type.CELL = 1
scenario.target_type.PATROL = 10
scenario.target_type.UNLOAD = 6
scenario.target_type.BUILD = 8
scenario.target_type.ENTITY = 2
scenario.target_type.BUILD_ASSIST = 9
scenario.target_type.ATTACK_ENTITY = 4
scenario.target_type.MOLOTOV = 7
scenario.target_type.ATTACK_CELL = 3

scenario.CHAT_COLOR_WHITE = 0
scenario.global_objective_counter_type = {}
scenario.global_objective_counter_type.COUNTDOWN = 2
scenario.global_objective_counter_type.VARIABLE = 3
scenario.global_objective_counter_type.OFF = 0
scenario.global_objective_counter_type.GOLD = 1

scenario.bot_squad_type = {}
scenario.bot_squad_type.DEFEND = 1
scenario.bot_squad_type.LANDMINES = 3
scenario.bot_squad_type.ATTACK = 0
scenario.bot_squad_type.RESERVES = 2
scenario.bot_squad_type.RETURN = 4
scenario.bot_squad_type.PATROL = 5

scenario.music = {}
scenario.music.MATCH1 = 233
scenario.music.MATCH4 = 236
scenario.music.MATCH2 = 234
scenario.music.MATCH3 = 235

scenario.bot_config_flag = {}
scenario.bot_config_flag.SHOULD_SURRENDER = 32
scenario.bot_config_flag.SHOULD_ATTACK = 2
scenario.bot_config_flag.IS_OMNISCIENT = 256
scenario.bot_config_flag.SHOULD_ATTACK_FIRST = 1
scenario.bot_config_flag.SHOULD_SCOUT = 16
scenario.bot_config_flag.SHOULD_PRODUCE = 64
scenario.bot_config_flag.SHOULD_HARASS = 4
scenario.bot_config_flag.SHOULD_CANCEL_BUILDINGS = 128
scenario.bot_config_flag.SHOULD_RETREAT = 8

scenario.match_input_type = {}
scenario.match_input_type.NONE = 0
scenario.match_input_type.MOVE_ENTITY = 2
scenario.match_input_type.MOVE_ATTACK_CELL = 3
scenario.match_input_type.BUILD = 10
scenario.match_input_type.MOVE_ATTACK_ENTITY = 4
scenario.match_input_type.BUILDING_DEQUEUE = 13
scenario.match_input_type.PATROL = 19
scenario.match_input_type.BUILDING_ENQUEUE = 12
scenario.match_input_type.RALLY = 14
scenario.match_input_type.CAMO = 17
scenario.match_input_type.MOVE_MOLOTOV = 7
scenario.match_input_type.BUILD_CANCEL = 11
scenario.match_input_type.DEFEND = 9
scenario.match_input_type.MOVE_REPAIR = 5
scenario.match_input_type.MOVE_UNLOAD = 6
scenario.match_input_type.SINGLE_UNLOAD = 15
scenario.match_input_type.DECAMO = 18
scenario.match_input_type.UNLOAD = 16
scenario.match_input_type.MOVE_CELL = 1
scenario.match_input_type.STOP = 8

scenario.sound = {}
scenario.sound.BUILDING_DESTROY = 11
scenario.sound.CAMO_OFF = 29
scenario.sound.EXPLOSION = 4
scenario.sound.GUN = 3
scenario.sound.THROW = 16
scenario.sound.GARRISON_IN = 18
scenario.sound.BALLOON_DEATH = 30
scenario.sound.PICKAXE = 5
scenario.sound.AVALANCHE = 35
scenario.sound.GARRISON_OUT = 19
scenario.sound.UI_CLICK = 0
scenario.sound.BUNKER_DESTROY = 12
scenario.sound.ALERT_BUILDING = 21
scenario.sound.DEATH_CHICKEN = 9
scenario.sound.ALERT_RESEARCH = 22
scenario.sound.OBJECTIVE_COMPLETE = 32
scenario.sound.SWORD = 8
scenario.sound.MINE_PRIME = 15
scenario.sound.CANNON = 10
scenario.sound.PEN_SCRATCH = 36
scenario.sound.STINGER_DEFEAT = 38
scenario.sound.ALERT_BELL = 20
scenario.sound.MUSKET = 2
scenario.sound.STINGER_VICTORY = 37
scenario.sound.DEATH = 1
scenario.sound.MINE_DESTROY = 13
scenario.sound.MATCH_START = 33
scenario.sound.RICOCHET = 31
scenario.sound.CAMO_ON = 28
scenario.sound.GOLD_MINE_COLLAPSE = 24
scenario.sound.MOLOTOV_IMPACT = 25
scenario.sound.MINE_INSERT = 14
scenario.sound.FIRE_BURN = 26
scenario.sound.ALERT_UNIT = 23
scenario.sound.BUILDING_PLACE = 7
scenario.sound.PISTOL_SILENCED = 27
scenario.sound.GOLD_PICKUP = 34
scenario.sound.FLAG_THUMP = 17
scenario.sound.HAMMER = 6

scenario.PLAYER_NONE = 4
scenario.ALERT_COLOR_WHITE = 0
scenario.CAMERA_MODE_HELD = 3
scenario.PLAYER_ID = 0
scenario.CAMERA_MODE_MINIMAP_DRAG = 1

--- Send a debug log. If debug logging is disabled, this function does nothing.
--- @param ... any Values to print
function scenario.log(...) end

--- Plays a sound effect.
--- @param sound number
function scenario.play_sound(sound) end

--- Sets the next music track
--- @param track number
function scenario.set_next_music_track(track) end

--- Returns the time in seconds since the scenario started.
--- @return number
function scenario.get_time() end

--- Creates an alert on the minimap
--- @param alert_color number
--- @param cell ivec2
--- @param cell_size number
function scenario.create_alert(alert_color, cell, cell_size) end

--- End the match in victory.
function scenario.set_match_over_victory() end

--- End the match in defeat.
function scenario.set_match_over_defeat() end

--- Checks if the player has been defeated.
--- @param player_id number
--- @return boolean
function scenario.is_player_defeated(player_id) end

--- Returns the specified player's gold count
--- @param player_id number
--- @return number
function scenario.get_player_gold(player_id) end

--- Returns the total number of gold mined by the specified player this match
--- @param player_id number
--- @return number
function scenario.get_player_gold_mined_total(player_id) end

--- Grants the player the specified upgrade
--- @param player_id number
--- @param upgrade number
function scenario.grant_player_upgrade(player_id, upgrade) end

--- Returns the player's population
--- @param player_id number
--- @return number
function scenario.get_player_population(player_id) end

--- Returns the number of entities controlled by the player of a given type.
--- @param player_id number
--- @param entity_type number
--- @return number
function scenario.get_player_entity_count(player_id, entity_type) end

--- Sends a chat message
--- @param message string
function scenario.chat(message) end

--- Sends a chat message with a colored prefix
--- @param prefix_color number
--- @param prefix string
--- @param message string
function scenario.chat_prefixed(prefix_color, prefix, message) end

--- Sends a hint message.
--- @param message string
function scenario.hint(message) end

--- Reveals fog at the specified cell. If no player ID is specified, it will default to scenario.PLAYER_ID
--- @param params { player_id: number|nil cell: ivec2, cell_size: number, sight: number, duration: number }
function scenario.fog_reveal(params) end

--- Returns the cell the camera is currently centered on
--- @return ivec2
function scenario.get_camera_centered_cell() end

--- Gradually pans the camera to center on the specified cell.
--- @param cell ivec2 The cell to pan the camera to
--- @param duration number The duration in seconds of the camera pan
function scenario.begin_camera_pan(cell, duration) end

--- Pans the camera to the specified cell while shaking for the specified duration
--- @param cell ivec2 The cell to pan the camera to
--- @param duration number The duration in seconds of the camera pan
function scenario.begin_camera_pan_and_shake(cell, duration) end

--- Shakes the camera for the specified duration
--- @param duration number
function scenario.begin_camera_shake(duration) end

--- Removes camera movement from the player and holds the camera in place
function scenario.hold_camera() end

--- Returns camera movement to the player
function scenario.release_camera() end

--- Returns the current camera mode.
--- @return number
function scenario.get_camera_mode() end

--- Returns the index of the created objective.
--- @param params { description: string, entity_type: number|nil, counter_target: number|nil }
--- @return number
function scenario.add_objective(params) end

--- Sets the specified objectives variable counter
--- @param objective_index number
--- @param counter_value number
function scenario.set_objective_variable_counter(objective_index, counter_value) end

--- Marks the specified objective as complete.
--- @param objective_index number
function scenario.complete_objective(objective_index) end

--- Returns true if the specified objective is complete.
--- @param objective_index number
--- @return boolean
function scenario.is_objective_complete(objective_index) end

--- Returns true if all objectives are complete.
--- @return boolean
function scenario.are_objectives_complete() end

--- Clears the objectives list.
function scenario.clear_objectives() end

--- Sets the global objective counter
--- @param params table
function scenario.set_global_objective_counter(params) end

--- Updates the global objective counter. Should only be called on variable counter.
--- @param value number
function scenario.set_global_objective_counter_variable_value(value) end

--- Returns true if the specified entity is visible to the player.
--- @param entity_id number
--- @return boolean
function scenario.is_entity_visible_to_player(entity_id) end

--- Highlights the specified entity.
--- @param entity_id number
function scenario.highlight_entity(entity_id) end

--- Returns the gold cost of the specified entity type
--- @param entity_type number
--- @return number
function scenario.get_entity_gold_cost(entity_type) end

--- Returns the building type which trains the specified entity
--- @param entity_type number
--- @return number
function scenario.get_building_which_trains(entity_type) end

--- Accepts a table of entity types and returns a parallel table where each entry in the result is either an ivec2 or nil if no spawn location could be found
--- @param spawn_cell ivec2
--- @param entity_types table
--- @return table
function scenario.find_entity_spawn_cells(spawn_cell, entity_types) end

--- Creates a new entity. Returns the ID of the newly created entity
--- @param entity_type number
--- @param cell ivec2
--- @param player_id number
--- @return number
function scenario.create_entity(entity_type, cell, player_id) end

--- Removes an entity
--- @param entity_id number
function scenario.remove_entity(entity_id) end

--- Sets values on an entity based on the passed in props table
--- @param entity_id number
--- @param props table
function scenario.edit_entity(entity_id, props) end

--- Returns the ID of the hall which surrounds the specified goldmine
--- @param goldmine_id number
--- @return number | nil
function scenario.get_hall_surrounding_goldmine(goldmine_id) end

--- Spawns an enemy squad. The entities table should be an array of entity types.
--- Returns the squad ID of the created squad, or SQUAD_ID_NULL if no squad was created.
--- @param params { player_id: number, type: number, target_cell: ivec2, entity_list: table }
--- @return number
function scenario.bot_add_squad(params) end

--- Checks if the squad exists.
--- @param player_id number
--- @param squad_id number
--- @return boolean
function scenario.bot_squad_exists(player_id, squad_id) end

--- Returns the number of squads controlled by the specified bot
--- @param player_id number
--- @return number
function scenario.bot_get_squad_count(player_id) end

--- Returns a table of information about the squad matching the provided index
--- @param player_id number
--- @param index number
--- @return table
function scenario.bot_get_squad_by_index(player_id, index) end

--- Returns a table of information about the squad matching the provided ID
--- @param player_id number
--- @param squad_id number
--- @return table
function scenario.bot_get_squad_by_id(player_id, squad_id) end

--- Sets the target cell of the specified squad
--- @param player_id number
--- @param squad_id number
--- @param target_cell ivec2
function scenario.bot_set_squad_target_cell(player_id, squad_id, target_cell) end

--- Add entities to an existing squad
--- @param player_id number
--- @param squad_id number
--- @param entity_list table
function scenario.bot_add_entities_to_squad(player_id, squad_id, entity_list) end

--- Gets the Squad ID of the squad that the entity is a part of
--- Returns nil if the entity is not part of a squad
--- @param entity_id number
--- @return number | nil
function scenario.bot_get_entity_squad_id(entity_id) end

--- Sets a bot config flag to the specified value
--- @param player_id number
--- @param flag number
--- @param value boolean
function scenario.bot_set_config_flag(player_id, flag, value) end

--- Returns a list of entities that are allowed to be produced by this bot
--- @param player_id number
function scenario.bot_get_allowed_entities(player_id) end

--- Sets the list of entities allowed by this bot
--- @param player_id number
--- @param entity_types table - A list of allowed entities types
function scenario.bot_set_allowed_entities(player_id, entity_types) end

--- Returns true if the specified entity is reserved by the bot
--- @param player_id number
--- @param entity_id number
function scenario.bot_is_entity_reserved(player_id, entity_id) end

--- Tells the specified bot to reserve the specified entity
--- @param player_id number
--- @param entity_id number
function scenario.bot_reserve_entity(player_id, entity_id) end

--- Tells the specified bot o release the specified entity
--- @param player_id number
--- @param entity_id number
function scenario.bot_release_entity(player_id, entity_id) end

--- MOVE { target_cell: ivec2|nil, target_id: number|nil }
--- BUILD { building_type: number, building_cell: ivec2 }
--- @param params { player_id: number, type: number, entity_id: number|nil, entity_ids: table|nil }
function scenario.queue_match_input(params) end

--- Creates an avalanche at the specified position
--- @param position ivec2
function scenario.create_avalanche_column(position) end

--- Explodes a rigged goldmine
--- @param goldmine_id number
function scenario.explode_rigged_goldmine(goldmine_id) end

