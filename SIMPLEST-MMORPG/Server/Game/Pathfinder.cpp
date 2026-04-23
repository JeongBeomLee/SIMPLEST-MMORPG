#include "Pathfinder.h"
#include "GameWorld.h"
#include "Constants.h"
#include <queue>
#include <unordered_set>
#include <cstdlib>

namespace 
{
	struct Node
	{
		int16_t x, y;
		int g;           // 시작점부터 비용
		int f;           // g + h (우선순위 기준)
		int parentIdx;   // m_nodes 안에서의 부모 인덱스 (-1 = 시작)
	};

	// priority_queue 비교 함수
	struct NodeCmp
	{
		bool operator()(const Node& a, const Node& b) const
		{
			if (a.f != b.f)
			{
				return a.f > b.f;
			}
			return a.g < b.g;
		}
	};

	// x, y 를 하나의 key 로 인코딩 (방문 체크용)
	inline uint32_t Encode(int16_t x, int16_t y)
	{
		return static_cast<uint32_t>(y) * MAP_WIDTH + static_cast<uint32_t>(x);
	}

	// 맨해튼 거리 (휴리스틱)
	inline int Manhattan(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
	{
		return std::abs(x1 - x2) + std::abs(y1 - y2);
	}
}

std::vector<Position> Pathfinder::FindPath(
	int16_t startX, int16_t startY,
	int16_t goalX, int16_t goalY,
	int maxNodes)
{
	std::vector<Position> result;

	// 목표가 장애물이면 즉시 실패
	const Map& map = GameWorld::GetInstance().GetMap();
	if (!map.IsWalkable(goalX, goalY))
	{
		return result;
	}

	// 시작 = 목표
	if (startX == goalX && startY == goalY)
	{
		return result;
	}

	std::vector<Node> nodes;
	nodes.reserve(maxNodes * 4);

	// 우선순위 큐: f 작은 노드부터
	std::priority_queue<Node, std::vector<Node>, NodeCmp> open;
	std::unordered_set<uint32_t> closed;

	// 시작 노드
	Node startNode{ startX, startY, 0, Manhattan(startX, startY, goalX, goalY), -1 };
	nodes.push_back(startNode);
	open.push(startNode);

	int nodesProcessed = 0;

	while (!open.empty() && nodesProcessed < maxNodes)
	{
		Node current = open.top();
		open.pop();

		uint32_t key = Encode(current.x, current.y);

		// 이미 더 짧은 경로로 방문함
		if (closed.count(key))
		{
			continue;
		}
		closed.insert(key);

		// 목표 도달 — 경로 역추적
		if (current.x == goalX && current.y == goalY)
		{
			// current 를 nodes 에 추가
			int endIdx = static_cast<int>(nodes.size());
			nodes.push_back(current);

			// 역추적
			std::vector<Position> reverse_path;
			int idx = endIdx;
			while (idx != -1 && idx != 0)
			{
				reverse_path.push_back({ nodes[idx].x, nodes[idx].y });
				idx = nodes[idx].parentIdx;
			}

			// 뒤집어서 반환
			for (auto it = reverse_path.rbegin(); it != reverse_path.rend(); ++it)
			{
				result.push_back(*it);
			}

			return result;
		}

		// current 인덱스 (방금 확정됨)
		int currentIdx = -1;
		for (int i = static_cast<int>(nodes.size()) - 1; i >= 0; --i)
		{
			if (nodes[i].x == current.x && nodes[i].y == current.y && nodes[i].g == current.g)
			{
				currentIdx = i;
				break;
			}
		}
		if (currentIdx == -1)
		{
			// nodes 에 없으면 추가
			currentIdx = static_cast<int>(nodes.size());
			nodes.push_back(current);
		}

		// 4방향 이웃
		for (int d = 0; d < 4; ++d)
		{
			int16_t nx = current.x + DX[d];
			int16_t ny = current.y + DY[d];

			if (!map.IsWalkable(nx, ny))
			{
				continue;
			}
			if (closed.count(Encode(nx, ny)))
			{
				continue;
			}

			int g_new = current.g + 1;
			int h = Manhattan(nx, ny, goalX, goalY);
			Node neighbor{ nx, ny, g_new, g_new + h, currentIdx };
			open.push(neighbor);
		}

		nodesProcessed++;
	}

	return result;
}
