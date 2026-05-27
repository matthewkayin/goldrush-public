local list_util = {}

function list_util.filter(list, filter)
    local new_list = {}
    for index = 1,#list do
        if filter(list[index]) then
            table.insert(new_list, list[index])
        end
    end

    return new_list
end

function list_util.copy(list)
    local new_list = {}
    for index = 1,#list do
        table.insert(new_list, list[index])
    end

    return new_list
end

return list_util
