
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "butterfly.h"
#include "internal.h"

static int ensure_farm_is_init(struct bf_farm *farm)
{
	struct point point;
	int x, y;

	if (farm->is_init) {
		return 0;
	}

	farm->rng_state = malloc(sizeof(struct random_state));
	if (!farm->rng_state) {
		goto rng_state_failure;
	}
	if (!farm->seed) {
		farm->seed = time(NULL);
	}
	random_seed(farm->rng_state, farm->seed);

	farm->safe_spots = malloc(sizeof(struct pointset));
	if (!farm->safe_spots) {
		goto safe_spots_alloc_failure;
	}
	if (ps_init(farm->safe_spots, farm->width, farm->height)) {
		goto safe_spots_init_failure;
	}

	farm->dangerous_spots = malloc(sizeof(struct pointset));
	if (!farm->dangerous_spots) {
		goto dangerous_spots_alloc_failure;
	}
	if (ps_init(farm->dangerous_spots, farm->width, farm->height)) {
		goto dangerous_spots_init_failure;
	}

	for (x = 0; x < farm->width; ++x) {
		for (y = 0; y < farm->height; ++y) {
			point = (struct point){x, y};
			if (SPOT_AT(farm->spots, farm->width, x, y) > 0) {
				ps_add(farm->safe_spots, point);
			} else {
				ps_add(farm->dangerous_spots, point);
			}
		}
	}

	farm->is_init = 1;
	return 0;

dangerous_spots_init_failure:
	free(farm->dangerous_spots);
dangerous_spots_alloc_failure:
	ps_uninit(farm->safe_spots);
safe_spots_init_failure:
	free(farm->safe_spots);
safe_spots_alloc_failure:
	free(farm->rng_state);
rng_state_failure:
	return -1;
}

static void copy_instincts_with_event(
	struct bf_instinct *dst,
	size_t *dstcount,
	struct bf_instinct *src,
	size_t srccount,
	int event)
{
	size_t i, j;
	*dstcount = 0;

	for (i = 0; i < srccount; ++i) {
		if (src[i].event == event) {
			dst[*dstcount].event = src[i].event;
			dst[*dstcount].action = src[i].action;
			for (j = 0; j < BF_NARGS; ++j) {
				dst[*dstcount].args[j] =
					src[i].args[j];
			}
			++(*dstcount);
		}
	}
}

static void do_morph_actions(
	struct butterfly *bf,
	struct bf_farm *farm,
	struct bf_instinct *instincts,
	size_t count)
{
	struct bf_instinct morphs[count];
	size_t nmorphs;
	size_t r;

	copy_instincts_with_event(
		morphs, &nmorphs,
		instincts, count,
		BF_MORPH);

	r = random_next_index(farm->rng_state, nmorphs);
	morph(bf, farm, &morphs[r]);
}

static void do_goal_actions(
	struct butterfly *bf,
	struct bf_farm *farm,
	struct bf_instinct *instincts,
	size_t count)
{
	struct bf_instinct goals[count];
	size_t ngoals;
	size_t r;

	copy_instincts_with_event(
		goals, &ngoals,
		instincts, count,
		BF_GOAL);

	r = random_next_index(farm->rng_state, ngoals);
	goal(bf, farm, &goals[r]);
}

static bool check_if_should_die(
	struct butterfly *bf,
	struct bf_farm *farm,
	struct bf_instinct *instincts,
	size_t count)
{
	struct bf_instinct goals[count];
	size_t ngoals;
	size_t i;
	bool should_die = false;

	copy_instincts_with_event(
		goals, &ngoals,
		instincts, count,
		BF_DIE);

	for (i = 0; i < ngoals; ++i) {
		/* ensure each check is run */
		should_die = should_die || die(bf, farm, &goals[i]);
	}

	return should_die;
}

static void do_look_actions(
	struct butterfly *bf,
	struct bf_farm *farm,
	struct bf_instinct *instincts,
	size_t count)
{
	struct bf_instinct looks[count];
	size_t nlooks;
	size_t r;

	copy_instincts_with_event(
		looks, &nlooks,
		instincts, count,
		BF_LOOK);

	r = random_next_index(farm->rng_state, nlooks);
	look(bf, farm, &looks[r]);
}

static void do_flutter_actions(
	struct butterfly *bf,
	struct bf_farm *farm,
	struct bf_instinct *instincts,
	size_t count)
{
	struct bf_instinct flutters[count];
	size_t nflutters;
	size_t r;

	copy_instincts_with_event(
		flutters, &nflutters,
		instincts, count,
		BF_FLUTTER);

	r = random_next_index(farm->rng_state, nflutters);
	flutter(bf, farm, &flutters[r]);
}

static void commit_new_spots(struct butterfly *bf, struct bf_farm *farm)
{
	struct point point;
	int x, y;
	int ns, s;

	for (x = 0; x < farm->width; ++x) {
		for (y = 0; y < farm->height; ++y) {
			ns = SPOT_AT(bf->new_spots, farm->width, x, y);
			s = SPOT_AT(farm->spots, farm->width, x, y);
			if (ns == s) {
				continue;
			} else if (ns > 0 && s <= 0) {
				point = (struct point){x, y};
				ps_rem(farm->dangerous_spots, point);
				ps_add(farm->safe_spots, point);
			} else if (ns <= 0 && s > 0) {
				point = (struct point){x, y};
				ps_rem(farm->safe_spots, point);
				ps_add(farm->dangerous_spots, point);
			}
			SPOT_AT(farm->spots, farm->width, x, y) = ns;
		}
	}
}

int bf_spawn(
	struct bf_farm *farm,
	struct bf_instinct *instincts,
	size_t count)
{
	if (ensure_farm_is_init(farm)) {
		return -1;
	}

	struct butterfly bf;
	memset(&bf, 0, sizeof(bf));
	bf.new_spots = malloc(
		sizeof(*bf.new_spots) * farm->width * farm->height);
	if (!bf.new_spots) {
		return -1;
	}
	memcpy(
		bf.new_spots,
		farm->spots,
		sizeof(*bf.new_spots) * farm->width * farm->height);

	do_morph_actions(&bf, farm, instincts, count);
	do_goal_actions(&bf, farm, instincts, count);

	int i = 0;
	while (!check_if_should_die(&bf, farm, instincts, count)) {
		do_look_actions(&bf, farm, instincts, count);
		printf("here %d\n", i++);
		do_flutter_actions(&bf, farm, instincts, count);
	}

	commit_new_spots(&bf, farm);

	free(bf.new_spots);

	return 0;
}


void bf_cleanup(struct bf_farm *farm)
{
	if (!farm->is_init) {
		return;
	}
	free(farm->rng_state);
	ps_uninit(farm->safe_spots);
	free(farm->safe_spots);
	ps_uninit(farm->dangerous_spots);
	free(farm->dangerous_spots);
	farm->is_init = 0;
}