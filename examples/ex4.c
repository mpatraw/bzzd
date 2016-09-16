
#include <stdio.h>

#include <BearLibTerminal.h>
#include <butterfly.h>

#define WIDTH 120
#define HEIGHT 40

enum {
	WALL,
	CAVE_WALL,
	FLOOR,
	CAVE_FLOOR,
	RIVER
};

struct tile_def {
	char glyph;
	const char *fg;
};

static struct tile_def tile_defs[] = {
	{'#', "grey"},
	{'#', "darkest amber"},
	{'.', "grey"},
	{'.', "amber"},
	{'~', "darker blue"}
};

int main(void)
{
	int spots[HEIGHT][WIDTH] = {{WALL}};
	struct bf_config room_config = {
		.error_on_looking_at_safe = 1,
		.error_on_looking_outside_farm = 1
	};
	struct bf_config tunnel_config = {
		.error_on_looking_at_safe = 1,
		.error_on_looking_outside_farm = 1,
		.cycle_looking = 1
	};
	struct bf_config cave_config = {
		.enable_neighbor_look_8 = 1,
		.neighbor_look_8 = CAVE_WALL
	};
	struct bf_farm farm = {
		.spots = (void *)spots,
		.width = WIDTH,
		.height = HEIGHT,
		.seed = 0,
		.last_dangerous = CAVE_WALL
	};
	struct bf_instinct room[] = {
		{.event = BF_MORPH, .action = BF_MORPH_AT_RANDOM_SPOT},
		{.event = BF_LOOK, .action = BF_LOOK_RECT_AREA, .args = {FLOOR, 2, 2}},
		{.event = BF_LOOK, .action = BF_LOOK_RECT_AREA, .args = {FLOOR, 3, 3}},
		{.event = BF_LOOK, .action = BF_LOOK_CIRCLE_AREA, .args = {FLOOR, 3}},
		{.event = BF_LOOK, .action = BF_LOOK_CIRCLE_AREA, .args = {FLOOR, 4}},
		{.event = BF_LOOK, .action = BF_LOOK_CIRCLE_AREA, .args = {FLOOR, 6}},
		{.event = BF_DIE, .action = BF_DIE_AFTER_N, .args = {1}},
	};
	struct bf_instinct tunnel[] = {
		{.event = BF_MORPH, .action = BF_MORPH_AT_LAST_DEATH_SPOT},
		{.event = BF_GOAL, .action = BF_GOAL_RANDOM_SAFE_SPOT},
		{.event = BF_FLUTTER, .action = BF_FLUTTER_TUNNEL},
		{.event = BF_LOOK, .action = BF_LOOK_1_AREA, .args = {FLOOR}},
		{.event = BF_LOOK, .action = BF_LOOK_1_AREA, .args = {FLOOR}},
		{.event = BF_LOOK, .action = BF_LOOK_1_AREA, .args = {FLOOR}},
		{.event = BF_LOOK, .action = BF_LOOK_1_AREA, .args = {FLOOR}},
		{.event = BF_LOOK, .action = BF_LOOK_1_AREA, .args = {FLOOR}},
		{.event = BF_LOOK, .action = BF_LOOK_1_AREA, .args = {FLOOR}},
		{.event = BF_LOOK, .action = BF_LOOK_DIAMOND_AREA, .args = {FLOOR, 3}},
		{.event = BF_DIE, .action = BF_DIE_AT_SAFE_SPOT},
	};
	struct bf_instinct cave[] = {
		{.event = BF_MORPH, .action = BF_MORPH_AT_RANDOM_SPOT},
		{.event = BF_GOAL, .action = BF_GOAL_RANDOM_SAFE_SPOT},
		{.event = BF_FLUTTER, .action = BF_FLUTTER_WEIGHTED_4, {60}},
		{.event = BF_LOOK, .action = BF_LOOK_PLUS_AREA, .args = {CAVE_FLOOR}},
		{.event = BF_DIE, .action = BF_DIE_AT_SAFE_SPOT},
	};
	struct bf_instinct river[] = {
		{.event = BF_MORPH, .action = BF_MORPH_AT_RANDOM_EDGE_SPOT},
		{.event = BF_GOAL, .action = BF_GOAL_RANDOM_EDGE_SPOT},
		{.event = BF_FLUTTER, .action = BF_FLUTTER_WEIGHTED_4, {100}},
		{.event = BF_LOOK, .action = BF_LOOK_1_AREA, .args = {RIVER}},
		{.event = BF_DIE, .action = BF_DIE_AT_GOAL},
	};

	BF_SPAWN_ARR(&farm, room, NULL);
	bf_commit(&farm);
	printf("seed: %d\n", farm.seed);
	for (int i = 0; i < 120; ++i) {
		if (bf_random(&farm) < 0.90) {
			if (BF_SPAWN_ARR(&farm, room, &room_config)) {
				continue;
			}
			BF_SPAWN_ARR(&farm, tunnel, &tunnel_config);
		} else {
			BF_SPAWN_ARR(&farm, cave, &cave_config);
		}
		bf_commit(&farm);
	}
	BF_SPAWN_ARR(&farm, river, NULL);
	bf_commit(&farm);

	terminal_open();
	terminal_set("window: size=120x40");


	for (int y = 0; y < HEIGHT; ++y) {
		for (int x = 0; x < WIDTH; ++x) {
			struct tile_def t = tile_defs[spots[y][x]];
			terminal_color(color_from_name(t.fg));
			terminal_put(x, y, t.glyph);
		}
	}

	terminal_refresh();
	while (terminal_read() != TK_CLOSE) {
		terminal_refresh();
	}

	terminal_close();
	return 0;
}
