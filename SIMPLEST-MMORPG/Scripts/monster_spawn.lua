-- ============================================
-- monster_spawn.lua
-- 몬스터 자동 생성 스크립트
-- ============================================

math.randomseed(42)

local MAP_W   = 2000
local MAP_H   = 2000
local CELLS_X = 400
local CELLS_Y = 500
local CELL_W  = MAP_W / CELLS_X   -- 5
local CELL_H  = MAP_H / CELLS_Y   -- 4

-- Pawn 60%, Knight 20%, Rook 12%, Bishop 7%, Queen 1%
local function pick_type()
    local r = math.random(1, 100)
    if     r <= 60 then return "Pawn"
    elseif r <= 80 then return "Knight"
    elseif r <= 92 then return "Rook"
    elseif r <= 99 then return "Bishop"
    else                return "Queen" end
end

local pawn_spawns   = {}
local knight_spawns = {}
local rook_spawns   = {}
local bishop_spawns = {}
local queen_spawns  = {}

for cy = 0, CELLS_Y - 1 do
    local y_min = cy * CELL_H
    local y_max = (cy + 1) * CELL_H - 1
    for cx = 0, CELLS_X - 1 do
        local x_min = cx * CELL_W
        local x_max = (cx + 1) * CELL_W - 1

        local x = math.random(x_min, x_max)
        local y = math.random(y_min, y_max)

        local t = pick_type()
        local pos = { x, y }
        if     t == "Pawn"   then table.insert(pawn_spawns,   pos)
        elseif t == "Knight" then table.insert(knight_spawns, pos)
        elseif t == "Rook"   then table.insert(rook_spawns,   pos)
        elseif t == "Bishop" then table.insert(bishop_spawns, pos)
        else                      table.insert(queen_spawns,  pos)
        end
    end
end

-- ============================================
-- 몬스터 종류별 정의
-- ============================================
monster_spawns = {
    {
        name     = "Pawn",
        level    = 2,
        hp       = 50,
        behavior = "peace",
        movement = "roaming",
        spawns   = pawn_spawns,    -- ~120,000
    },
    {
        name     = "Knight",
        level    = 5,
        hp       = 120,
        behavior = "agro",
        movement = "roaming",
        spawns   = knight_spawns,  -- ~40,000
    },
    {
        name     = "Rook",
        level    = 8,
        hp       = 1000,
        behavior = "peace",
        movement = "fixed",
        spawns   = rook_spawns,    -- ~24,000
    },
    {
        name     = "Bishop",
        level    = 8,
        hp       = 500,
        behavior = "agro",
        movement = "roaming",
        spawns   = bishop_spawns,  -- ~14,000
    },
    {
        name     = "Queen",
        level    = 10,
        hp       = 1000,
        behavior = "agro",
        movement = "roaming",
        spawns   = queen_spawns,   -- ~2,000
    },
}
