-- ============================================
-- monster_spawn.lua
-- 몬스터 배치 정의
-- ============================================

function generate_grid_spawns(sx, sy, cx, cy, spacing)
	local result = {}
	for dy = 0, cy - 1 do
		for dx = 0, cx - 1 do
			table.insert(result, {sx + dx * spacing, sy + dy * spacing})
		end
	end
	return result
end

-- ============================================
-- 몬스터 종류별 배치
-- ============================================

monster_spawns = {
    -- 슬라임: 평화 + 배회
    {
        name = "Slime",
        level = 1,
        hp = 50,
        behavior = "peace",
        movement = "roaming",
        spawns = {
            { 30, 30 },
            { 35, 32 },
            { 40, 28 },
            { 25, 35 },
            { 45, 40 },
        }
    },

    -- 고블린: 공격 + 배회
    {
        name = "Goblin",
        level = 3,
        hp = 120,
        behavior = "agro",
        movement = "roaming",
        spawns = generate_grid_spawns(100, 100, 5, 5, 8),
    },

    -- 보초 오크: 공격 + 고정
    {
        name = "Guard Orc",
        level = 5,
        hp = 250,
        behavior = "agro",
        movement = "fixed",
        spawns = {
            { 200, 200 },
            { 205, 200 },
            { 200, 205 },
            { 205, 205 },
        }
    },
}