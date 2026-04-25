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
    -- Pawn: 평화로운 잡몹, 초보자 영역
    {
        name = "Pawn",
        level = 1,
        hp = 50,
        behavior = "peace",
        movement = "roaming",
        spawns = generate_grid_spawns(30, 30, 5, 5, 8),   -- 25 마리
    },

    -- Knight: 추격형, 중간 영역
    {
        name = "Knight",
        level = 3,
        hp = 120,
        behavior = "agro",
        movement = "roaming",
        spawns = generate_grid_spawns(100, 100, 5, 5, 10), -- 25 마리
    },

    -- Rook: 보초병, 고정
    {
        name = "Rook",
        level = 5,
        hp = 250,
        behavior = "agro",
        movement = "fixed",
        spawns = generate_grid_spawns(200, 200, 4, 4, 5),  -- 16 마리
    },

    -- Bishop: 강력 추격, 깊은 지역
    {
        name = "Bishop",
        level = 8,
        hp = 500,
        behavior = "agro",
        movement = "roaming",
        spawns = { {500, 500}, {520, 500}, {510, 520} },   -- 3 마리
    },

    -- Queen: 보스 (몇 마리만)
    {
        name = "Queen",
        level = 10,
        hp = 1000,
        behavior = "agro",
        movement = "fixed",
        spawns = { {800, 800}, {1500, 1500} },             -- 2 마리
    },
}