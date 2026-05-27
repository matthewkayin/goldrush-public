local ivec2 = {}

-- Returns the manhattan distance between two cells
-- @param cell_a ivec2
-- @param cell_b ivec2
-- @return number
function ivec2.manhattan_distance(cell_a, cell_b)
    return math.abs(cell_a.x - cell_b.x) + math.abs(cell_a.y - cell_b.y)
end

-- Returns a new ivec2
-- @param x number
-- @param y number
-- @return ivec2
function ivec2.new(x, y)
    return { x = x, y = y }
end

-- Returns a new ivec2 based on a cdata, where it is assumed that the cdata has an x and y field
-- @param value
-- @return ivec2
function ivec2.from_cdata(value)
    return { x = value.x, y = value.y }
end

-- Returns true if the two ivec2s are equal
-- @param a ivec2
-- @param b ivec2
-- @return boolean
function ivec2.is_equal(a, b)
    return a.x == b.x and a.y == b.y
end

-- Returns a new ivec2 that is the addition of a and b
-- @param a ivec2
-- @param b ivec2
-- @return ivec2
function ivec2.add(a, b)
    return { x = a.x + b.x, y = a.y + b.y }
end

-- Returns a new ivec2 whose x and y have been divided by the scaler
-- @param value ivec2
-- @param scaler number
-- @return ivec2
function ivec2.divide_by_scaler(value, scaler)
    return { x = value.x / scaler, y = value.y / scaler }
end

return ivec2