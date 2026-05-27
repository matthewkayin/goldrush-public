local next_toggle_time
local is_snowing = false

function scenario_init()
    next_toggle_time = scenario.get_time() + 5
end

function scenario_update()
    if scenario.get_time() >= next_toggle_time then
        is_snowing = not is_snowing
        if is_snowing then
            scenario.begin_snow()
        else
            scenario.end_snow()
        end
        next_toggle_time = scenario.get_time() + 15
    end
end

-- hi good morning
-- look i made a thing
-- only i'm leaving the hard work to you:

-- we need a pick up and fade out animation for the snowstorm

-- and also maybe make the snowstorm animation better, like more streaky and volatile? and less "moving claw marks"