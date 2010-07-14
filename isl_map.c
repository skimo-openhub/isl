/*
 * Copyright 2008-2009 Katholieke Universiteit Leuven
 * Copyright 2010      INRIA Saclay
 *
 * Use of this software is governed by the GNU LGPLv2.1 license
 *
 * Written by Sven Verdoolaege, K.U.Leuven, Departement
 * Computerwetenschappen, Celestijnenlaan 200A, B-3001 Leuven, Belgium
 * and INRIA Saclay - Ile-de-France, Parc Club Orsay Universite,
 * ZAC des vignes, 4 rue Jacques Monod, 91893 Orsay, France 
 */

#include <string.h>
#include <strings.h>
#include "isl_ctx.h"
#include "isl_blk.h"
#include "isl_dim_private.h"
#include "isl_equalities.h"
#include "isl_list.h"
#include "isl_lp.h"
#include "isl_seq.h"
#include "isl_set.h"
#include "isl_map.h"
#include "isl_map_private.h"
#include "isl_map_piplib.h"
#include "isl_sample.h"
#include "isl_tab.h"
#include "isl_vec.h"

/* Maps dst positions to src positions */
struct isl_dim_map {
	unsigned len;
	int pos[1];
};

static struct isl_dim_map *isl_dim_map_alloc(struct isl_ctx *ctx, unsigned len)
{
	int i;
	struct isl_dim_map *dim_map;
	dim_map = isl_alloc(ctx, struct isl_dim_map,
				sizeof(struct isl_dim_map) + len * sizeof(int));
	if (!dim_map)
		return NULL;
	dim_map->len = 1 + len;
	dim_map->pos[0] = 0;
	for (i = 0; i < len; ++i)
		dim_map->pos[1 + i] = -1;
	return dim_map;
}

static unsigned n(struct isl_dim *dim, enum isl_dim_type type)
{
	switch (type) {
	case isl_dim_param:	return dim->nparam;
	case isl_dim_in:	return dim->n_in;
	case isl_dim_out:	return dim->n_out;
	case isl_dim_all:	return dim->nparam + dim->n_in + dim->n_out;
	default:		return 0;
	}
}

static unsigned pos(struct isl_dim *dim, enum isl_dim_type type)
{
	switch (type) {
	case isl_dim_param:	return 1;
	case isl_dim_in:	return 1 + dim->nparam;
	case isl_dim_out:	return 1 + dim->nparam + dim->n_in;
	default:		return 0;
	}
}

static void isl_dim_map_dim_range(struct isl_dim_map *dim_map,
	struct isl_dim *dim, enum isl_dim_type type,
	unsigned first, unsigned n, unsigned dst_pos)
{
	int i;
	unsigned src_pos;

	if (!dim_map || !dim)
		return;
	
	src_pos = pos(dim, type);
	for (i = 0; i < n; ++i)
		dim_map->pos[1 + dst_pos + i] = src_pos + first + i;
}

static void isl_dim_map_dim(struct isl_dim_map *dim_map, struct isl_dim *dim,
		enum isl_dim_type type, unsigned dst_pos)
{
	isl_dim_map_dim_range(dim_map, dim, type, 0, n(dim, type), dst_pos);
}

static void isl_dim_map_div(struct isl_dim_map *dim_map,
		struct isl_basic_map *bmap, unsigned dst_pos)
{
	int i;
	unsigned src_pos;

	if (!dim_map || !bmap)
		return;
	
	src_pos = 1 + isl_dim_total(bmap->dim);
	for (i = 0; i < bmap->n_div; ++i)
		dim_map->pos[1 + dst_pos + i] = src_pos + i;
}

static void isl_dim_map_dump(struct isl_dim_map *dim_map)
{
	int i;

	for (i = 0; i < dim_map->len; ++i)
		fprintf(stderr, "%d -> %d; ", i, dim_map->pos[i]);
	fprintf(stderr, "\n");
}

unsigned isl_basic_map_dim(const struct isl_basic_map *bmap,
				enum isl_dim_type type)
{
	if (!bmap)
		return 0;
	switch (type) {
	case isl_dim_param:
	case isl_dim_in:
	case isl_dim_out:	return isl_dim_size(bmap->dim, type);
	case isl_dim_div:	return bmap->n_div;
	case isl_dim_all:	return isl_basic_map_total_dim(bmap);
	default:		return 0;
	}
}

unsigned isl_map_dim(const struct isl_map *map, enum isl_dim_type type)
{
	return map ? n(map->dim, type) : 0;
}

unsigned isl_set_dim(const struct isl_set *set, enum isl_dim_type type)
{
	return set ? n(set->dim, type) : 0;
}

unsigned isl_basic_map_offset(struct isl_basic_map *bmap,
					enum isl_dim_type type)
{
	struct isl_dim *dim = bmap->dim;
	switch (type) {
	case isl_dim_param:	return 1;
	case isl_dim_in:	return 1 + dim->nparam;
	case isl_dim_out:	return 1 + dim->nparam + dim->n_in;
	case isl_dim_div:	return 1 + dim->nparam + dim->n_in + dim->n_out;
	default:		return 0;
	}
}

static unsigned map_offset(struct isl_map *map, enum isl_dim_type type)
{
	return pos(map->dim, type);
}

unsigned isl_basic_set_dim(const struct isl_basic_set *bset,
				enum isl_dim_type type)
{
	return isl_basic_map_dim((const struct isl_basic_map*)bset, type);
}

unsigned isl_basic_set_n_dim(const struct isl_basic_set *bset)
{
	return isl_basic_set_dim(bset, isl_dim_set);
}

unsigned isl_basic_set_n_param(const struct isl_basic_set *bset)
{
	return isl_basic_set_dim(bset, isl_dim_param);
}

unsigned isl_basic_set_total_dim(const struct isl_basic_set *bset)
{
	return isl_dim_total(bset->dim) + bset->n_div;
}

unsigned isl_set_n_dim(const struct isl_set *set)
{
	return isl_set_dim(set, isl_dim_set);
}

unsigned isl_set_n_param(const struct isl_set *set)
{
	return isl_set_dim(set, isl_dim_param);
}

unsigned isl_basic_map_n_in(const struct isl_basic_map *bmap)
{
	return bmap->dim->n_in;
}

unsigned isl_basic_map_n_out(const struct isl_basic_map *bmap)
{
	return bmap->dim->n_out;
}

unsigned isl_basic_map_n_param(const struct isl_basic_map *bmap)
{
	return bmap->dim->nparam;
}

unsigned isl_basic_map_n_div(const struct isl_basic_map *bmap)
{
	return bmap->n_div;
}

unsigned isl_basic_map_total_dim(const struct isl_basic_map *bmap)
{
	return bmap ? isl_dim_total(bmap->dim) + bmap->n_div : 0;
}

unsigned isl_map_n_in(const struct isl_map *map)
{
	return map->dim->n_in;
}

unsigned isl_map_n_out(const struct isl_map *map)
{
	return map->dim->n_out;
}

unsigned isl_map_n_param(const struct isl_map *map)
{
	return map->dim->nparam;
}

int isl_map_compatible_domain(struct isl_map *map, struct isl_set *set)
{
	return map->dim->n_in == set->dim->n_out &&
	       map->dim->nparam == set->dim->nparam;
}

int isl_basic_map_compatible_domain(struct isl_basic_map *bmap,
		struct isl_basic_set *bset)
{
	return bmap->dim->n_in == bset->dim->n_out &&
	       bmap->dim->nparam == bset->dim->nparam;
}

int isl_basic_map_compatible_range(struct isl_basic_map *bmap,
		struct isl_basic_set *bset)
{
	return bmap->dim->n_out == bset->dim->n_out &&
	       bmap->dim->nparam == bset->dim->nparam;
}

struct isl_dim *isl_basic_map_get_dim(struct isl_basic_map *bmap)
{
	if (!bmap)
		return NULL;
	return isl_dim_copy(bmap->dim);
}

struct isl_dim *isl_basic_set_get_dim(struct isl_basic_set *bset)
{
	if (!bset)
		return NULL;
	return isl_dim_copy(bset->dim);
}

struct isl_dim *isl_map_get_dim(struct isl_map *map)
{
	if (!map)
		return NULL;
	return isl_dim_copy(map->dim);
}

struct isl_dim *isl_set_get_dim(struct isl_set *set)
{
	if (!set)
		return NULL;
	return isl_dim_copy(set->dim);
}

__isl_give isl_basic_map *isl_basic_map_set_dim_name(
	__isl_take isl_basic_map *bmap,
	enum isl_dim_type type, unsigned pos, const char *s)
{
	if (!bmap)
		return NULL;
	bmap->dim = isl_dim_set_name(bmap->dim, type, pos, s);
	if (!bmap->dim)
		goto error;
	return bmap;
error:
	isl_basic_map_free(bmap);
	return NULL;
}

__isl_give isl_map *isl_map_set_dim_name(__isl_take isl_map *map,
	enum isl_dim_type type, unsigned pos, const char *s)
{
	int i;

	if (!map)
		return NULL;

	map->dim = isl_dim_set_name(map->dim, type, pos, s);
	if (!map->dim)
		goto error;

	for (i = 0; i < map->n; ++i) {
		map->p[i] = isl_basic_map_set_dim_name(map->p[i], type, pos, s);
		if (!map->p[i])
			goto error;
	}

	return map;
error:
	isl_map_free(map);
	return NULL;
}

__isl_give isl_basic_set *isl_basic_set_set_dim_name(
	__isl_take isl_basic_set *bset,
	enum isl_dim_type type, unsigned pos, const char *s)
{
	return (isl_basic_set *)isl_basic_map_set_dim_name(
		(isl_basic_map *)bset, type, pos, s);
}

__isl_give isl_set *isl_set_set_dim_name(__isl_take isl_set *set,
	enum isl_dim_type type, unsigned pos, const char *s)
{
	return (isl_set *)isl_map_set_dim_name((isl_map *)set, type, pos, s);
}

int isl_basic_map_is_rational(__isl_keep isl_basic_map *bmap)
{
	if (!bmap)
		return -1;
	return ISL_F_ISSET(bmap, ISL_BASIC_MAP_RATIONAL);
}

static struct isl_basic_map *basic_map_init(struct isl_ctx *ctx,
		struct isl_basic_map *bmap, unsigned extra,
		unsigned n_eq, unsigned n_ineq)
{
	int i;
	size_t row_size = 1 + isl_dim_total(bmap->dim) + extra;

	bmap->ctx = ctx;
	isl_ctx_ref(ctx);

	bmap->block = isl_blk_alloc(ctx, (n_ineq + n_eq) * row_size);
	if (isl_blk_is_error(bmap->block))
		goto error;

	bmap->ineq = isl_alloc_array(ctx, isl_int *, n_ineq + n_eq);
	if (!bmap->ineq)
		goto error;

	if (extra == 0) {
		bmap->block2 = isl_blk_empty();
		bmap->div = NULL;
	} else {
		bmap->block2 = isl_blk_alloc(ctx, extra * (1 + row_size));
		if (isl_blk_is_error(bmap->block2))
			goto error;

		bmap->div = isl_alloc_array(ctx, isl_int *, extra);
		if (!bmap->div)
			goto error;
	}

	for (i = 0; i < n_ineq + n_eq; ++i)
		bmap->ineq[i] = bmap->block.data + i * row_size;

	for (i = 0; i < extra; ++i)
		bmap->div[i] = bmap->block2.data + i * (1 + row_size);

	bmap->ref = 1;
	bmap->flags = 0;
	bmap->c_size = n_eq + n_ineq;
	bmap->eq = bmap->ineq + n_ineq;
	bmap->extra = extra;
	bmap->n_eq = 0;
	bmap->n_ineq = 0;
	bmap->n_div = 0;
	bmap->sample = NULL;

	return bmap;
error:
	isl_basic_map_free(bmap);
	return NULL;
}

struct isl_basic_set *isl_basic_set_alloc(struct isl_ctx *ctx,
		unsigned nparam, unsigned dim, unsigned extra,
		unsigned n_eq, unsigned n_ineq)
{
	struct isl_basic_map *bmap;
	bmap = isl_basic_map_alloc(ctx, nparam, 0, dim, extra, n_eq, n_ineq);
	return (struct isl_basic_set *)bmap;
}

struct isl_basic_set *isl_basic_set_alloc_dim(struct isl_dim *dim,
		unsigned extra, unsigned n_eq, unsigned n_ineq)
{
	struct isl_basic_map *bmap;
	if (!dim)
		return NULL;
	isl_assert(dim->ctx, dim->n_in == 0, return NULL);
	bmap = isl_basic_map_alloc_dim(dim, extra, n_eq, n_ineq);
	return (struct isl_basic_set *)bmap;
}

struct isl_basic_map *isl_basic_map_alloc_dim(struct isl_dim *dim,
		unsigned extra, unsigned n_eq, unsigned n_ineq)
{
	struct isl_basic_map *bmap;

	if (!dim)
		return NULL;
	bmap = isl_calloc_type(dim->ctx, struct isl_basic_map);
	if (!bmap)
		goto error;
	bmap->dim = dim;

	return basic_map_init(dim->ctx, bmap, extra, n_eq, n_ineq);
error:
	isl_dim_free(dim);
	return NULL;
}

struct isl_basic_map *isl_basic_map_alloc(struct isl_ctx *ctx,
		unsigned nparam, unsigned in, unsigned out, unsigned extra,
		unsigned n_eq, unsigned n_ineq)
{
	struct isl_basic_map *bmap;
	struct isl_dim *dim;

	dim = isl_dim_alloc(ctx, nparam, in, out);
	if (!dim)
		return NULL;

	bmap = isl_basic_map_alloc_dim(dim, extra, n_eq, n_ineq);
	return bmap;
}

static void dup_constraints(
		struct isl_basic_map *dst, struct isl_basic_map *src)
{
	int i;
	unsigned total = isl_basic_map_total_dim(src);

	for (i = 0; i < src->n_eq; ++i) {
		int j = isl_basic_map_alloc_equality(dst);
		isl_seq_cpy(dst->eq[j], src->eq[i], 1+total);
	}

	for (i = 0; i < src->n_ineq; ++i) {
		int j = isl_basic_map_alloc_inequality(dst);
		isl_seq_cpy(dst->ineq[j], src->ineq[i], 1+total);
	}

	for (i = 0; i < src->n_div; ++i) {
		int j = isl_basic_map_alloc_div(dst);
		isl_seq_cpy(dst->div[j], src->div[i], 1+1+total);
	}
	ISL_F_SET(dst, ISL_BASIC_SET_FINAL);
}

struct isl_basic_map *isl_basic_map_dup(struct isl_basic_map *bmap)
{
	struct isl_basic_map *dup;

	if (!bmap)
		return NULL;
	dup = isl_basic_map_alloc_dim(isl_dim_copy(bmap->dim),
			bmap->n_div, bmap->n_eq, bmap->n_ineq);
	if (!dup)
		return NULL;
	dup_constraints(dup, bmap);
	dup->flags = bmap->flags;
	dup->sample = isl_vec_copy(bmap->sample);
	return dup;
}

struct isl_basic_set *isl_basic_set_dup(struct isl_basic_set *bset)
{
	struct isl_basic_map *dup;

	dup = isl_basic_map_dup((struct isl_basic_map *)bset);
	return (struct isl_basic_set *)dup;
}

struct isl_basic_set *isl_basic_set_copy(struct isl_basic_set *bset)
{
	if (!bset)
		return NULL;

	if (ISL_F_ISSET(bset, ISL_BASIC_SET_FINAL)) {
		bset->ref++;
		return bset;
	}
	return isl_basic_set_dup(bset);
}

struct isl_set *isl_set_copy(struct isl_set *set)
{
	if (!set)
		return NULL;

	set->ref++;
	return set;
}

struct isl_basic_map *isl_basic_map_copy(struct isl_basic_map *bmap)
{
	if (!bmap)
		return NULL;

	if (ISL_F_ISSET(bmap, ISL_BASIC_SET_FINAL)) {
		bmap->ref++;
		return bmap;
	}
	bmap = isl_basic_map_dup(bmap);
	if (bmap)
		ISL_F_SET(bmap, ISL_BASIC_SET_FINAL);
	return bmap;
}

struct isl_map *isl_map_copy(struct isl_map *map)
{
	if (!map)
		return NULL;

	map->ref++;
	return map;
}

void isl_basic_map_free(struct isl_basic_map *bmap)
{
	if (!bmap)
		return;

	if (--bmap->ref > 0)
		return;

	isl_ctx_deref(bmap->ctx);
	free(bmap->div);
	isl_blk_free(bmap->ctx, bmap->block2);
	free(bmap->ineq);
	isl_blk_free(bmap->ctx, bmap->block);
	isl_vec_free(bmap->sample);
	isl_dim_free(bmap->dim);
	free(bmap);
}

void isl_basic_set_free(struct isl_basic_set *bset)
{
	isl_basic_map_free((struct isl_basic_map *)bset);
}

static int room_for_con(struct isl_basic_map *bmap, unsigned n)
{
	return bmap->n_eq + bmap->n_ineq + n <= bmap->c_size;
}

int isl_basic_map_alloc_equality(struct isl_basic_map *bmap)
{
	struct isl_ctx *ctx;
	if (!bmap)
		return -1;
	ctx = bmap->ctx;
	isl_assert(ctx, room_for_con(bmap, 1), return -1);
	isl_assert(ctx, (bmap->eq - bmap->ineq) + bmap->n_eq <= bmap->c_size,
			return -1);
	ISL_F_CLR(bmap, ISL_BASIC_MAP_NORMALIZED);
	ISL_F_CLR(bmap, ISL_BASIC_MAP_NO_REDUNDANT);
	ISL_F_CLR(bmap, ISL_BASIC_MAP_NO_IMPLICIT);
	ISL_F_CLR(bmap, ISL_BASIC_MAP_ALL_EQUALITIES);
	ISL_F_CLR(bmap, ISL_BASIC_MAP_NORMALIZED_DIVS);
	if ((bmap->eq - bmap->ineq) + bmap->n_eq == bmap->c_size) {
		isl_int *t;
		int j = isl_basic_map_alloc_inequality(bmap);
		if (j < 0)
			return -1;
		t = bmap->ineq[j];
		bmap->ineq[j] = bmap->ineq[bmap->n_ineq - 1];
		bmap->ineq[bmap->n_ineq - 1] = bmap->eq[-1];
		bmap->eq[-1] = t;
		bmap->n_eq++;
		bmap->n_ineq--;
		bmap->eq--;
		return 0;
	}
	isl_seq_clr(bmap->eq[bmap->n_eq] + 1 + isl_basic_map_total_dim(bmap),
		      bmap->extra - bmap->n_div);
	return bmap->n_eq++;
}

int isl_basic_set_alloc_equality(struct isl_basic_set *bset)
{
	return isl_basic_map_alloc_equality((struct isl_basic_map *)bset);
}

int isl_basic_map_free_equality(struct isl_basic_map *bmap, unsigned n)
{
	if (!bmap)
		return -1;
	isl_assert(bmap->ctx, n <= bmap->n_eq, return -1);
	bmap->n_eq -= n;
	return 0;
}

int isl_basic_set_free_equality(struct isl_basic_set *bset, unsigned n)
{
	return isl_basic_map_free_equality((struct isl_basic_map *)bset, n);
}

int isl_basic_map_drop_equality(struct isl_basic_map *bmap, unsigned pos)
{
	isl_int *t;
	if (!bmap)
		return -1;
	isl_assert(bmap->ctx, pos < bmap->n_eq, return -1);

	if (pos != bmap->n_eq - 1) {
		t = bmap->eq[pos];
		bmap->eq[pos] = bmap->eq[bmap->n_eq - 1];
		bmap->eq[bmap->n_eq - 1] = t;
	}
	bmap->n_eq--;
	return 0;
}

int isl_basic_set_drop_equality(struct isl_basic_set *bset, unsigned pos)
{
	return isl_basic_map_drop_equality((struct isl_basic_map *)bset, pos);
}

void isl_basic_map_inequality_to_equality(
		struct isl_basic_map *bmap, unsigned pos)
{
	isl_int *t;

	t = bmap->ineq[pos];
	bmap->ineq[pos] = bmap->ineq[bmap->n_ineq - 1];
	bmap->ineq[bmap->n_ineq - 1] = bmap->eq[-1];
	bmap->eq[-1] = t;
	bmap->n_eq++;
	bmap->n_ineq--;
	bmap->eq--;
	ISL_F_CLR(bmap, ISL_BASIC_MAP_NO_REDUNDANT);
	ISL_F_CLR(bmap, ISL_BASIC_MAP_NORMALIZED);
	ISL_F_CLR(bmap, ISL_BASIC_MAP_NORMALIZED_DIVS);
	ISL_F_CLR(bmap, ISL_BASIC_MAP_ALL_EQUALITIES);
}

static int room_for_ineq(struct isl_basic_map *bmap, unsigned n)
{
	return bmap->n_ineq + n <= bmap->eq - bmap->ineq;
}

int isl_basic_map_alloc_inequality(struct isl_basic_map *bmap)
{
	struct isl_ctx *ctx;
	if (!bmap)
		return -1;
	ctx = bmap->ctx;
	isl_assert(ctx, room_for_ineq(bmap, 1), return -1);
	ISL_F_CLR(bmap, ISL_BASIC_MAP_NO_IMPLICIT);
	ISL_F_CLR(bmap, ISL_BASIC_MAP_NO_REDUNDANT);
	ISL_F_CLR(bmap, ISL_BASIC_MAP_NORMALIZED);
	ISL_F_CLR(bmap, ISL_BASIC_MAP_ALL_EQUALITIES);
	isl_seq_clr(bmap->ineq[bmap->n_ineq] +
		      1 + isl_basic_map_total_dim(bmap),
		      bmap->extra - bmap->n_div);
	return bmap->n_ineq++;
}

int isl_basic_set_alloc_inequality(struct isl_basic_set *bset)
{
	return isl_basic_map_alloc_inequality((struct isl_basic_map *)bset);
}

int isl_basic_map_free_inequality(struct isl_basic_map *bmap, unsigned n)
{
	if (!bmap)
		return -1;
	isl_assert(bmap->ctx, n <= bmap->n_ineq, return -1);
	bmap->n_ineq -= n;
	return 0;
}

int isl_basic_set_free_inequality(struct isl_basic_set *bset, unsigned n)
{
	return isl_basic_map_free_inequality((struct isl_basic_map *)bset, n);
}

int isl_basic_map_drop_inequality(struct isl_basic_map *bmap, unsigned pos)
{
	isl_int *t;
	if (!bmap)
		return -1;
	isl_assert(bmap->ctx, pos < bmap->n_ineq, return -1);

	if (pos != bmap->n_ineq - 1) {
		t = bmap->ineq[pos];
		bmap->ineq[pos] = bmap->ineq[bmap->n_ineq - 1];
		bmap->ineq[bmap->n_ineq - 1] = t;
		ISL_F_CLR(bmap, ISL_BASIC_MAP_NORMALIZED);
	}
	bmap->n_ineq--;
	return 0;
}

int isl_basic_set_drop_inequality(struct isl_basic_set *bset, unsigned pos)
{
	return isl_basic_map_drop_inequality((struct isl_basic_map *)bset, pos);
}

__isl_give isl_basic_map *isl_basic_map_add_eq(__isl_take isl_basic_map *bmap,
	isl_int *eq)
{
	int k;

	bmap = isl_basic_map_extend_constraints(bmap, 1, 0);
	if (!bmap)
		return NULL;
	k = isl_basic_map_alloc_equality(bmap);
	if (k < 0)
		goto error;
	isl_seq_cpy(bmap->eq[k], eq, 1 + isl_basic_map_total_dim(bmap));
	return bmap;
error:
	isl_basic_map_free(bmap);
	return NULL;
}

__isl_give isl_basic_set *isl_basic_set_add_eq(__isl_take isl_basic_set *bset,
	isl_int *eq)
{
	return (isl_basic_set *)
		isl_basic_map_add_eq((isl_basic_map *)bset, eq);
}

__isl_give isl_basic_map *isl_basic_map_add_ineq(__isl_take isl_basic_map *bmap,
	isl_int *ineq)
{
	int k;

	bmap = isl_basic_map_extend_constraints(bmap, 0, 1);
	if (!bmap)
		return NULL;
	k = isl_basic_map_alloc_inequality(bmap);
	if (k < 0)
		goto error;
	isl_seq_cpy(bmap->ineq[k], ineq, 1 + isl_basic_map_total_dim(bmap));
	return bmap;
error:
	isl_basic_map_free(bmap);
	return NULL;
}

__isl_give isl_basic_set *isl_basic_set_add_ineq(__isl_take isl_basic_set *bset,
	isl_int *ineq)
{
	return (isl_basic_set *)
		isl_basic_map_add_ineq((isl_basic_map *)bset, ineq);
}

int isl_basic_map_alloc_div(struct isl_basic_map *bmap)
{
	if (!bmap)
		return -1;
	isl_assert(bmap->ctx, bmap->n_div < bmap->extra, return -1);
	isl_seq_clr(bmap->div[bmap->n_div] +
		      1 + 1 + isl_basic_map_total_dim(bmap),
		      bmap->extra - bmap->n_div);
	ISL_F_CLR(bmap, ISL_BASIC_MAP_NORMALIZED_DIVS);
	return bmap->n_div++;
}

int isl_basic_set_alloc_div(struct isl_basic_set *bset)
{
	return isl_basic_map_alloc_div((struct isl_basic_map *)bset);
}

int isl_basic_map_free_div(struct isl_basic_map *bmap, unsigned n)
{
	if (!bmap)
		return -1;
	isl_assert(bmap->ctx, n <= bmap->n_div, return -1);
	bmap->n_div -= n;
	return 0;
}

int isl_basic_set_free_div(struct isl_basic_set *bset, unsigned n)
{
	return isl_basic_map_free_div((struct isl_basic_map *)bset, n);
}

/* Copy constraint from src to dst, putting the vars of src at offset
 * dim_off in dst and the divs of src at offset div_off in dst.
 * If both sets are actually map, then dim_off applies to the input
 * variables.
 */
static void copy_constraint(struct isl_basic_map *dst_map, isl_int *dst,
			    struct isl_basic_map *src_map, isl_int *src,
			    unsigned in_off, unsigned out_off, unsigned div_off)
{
	unsigned src_nparam = isl_basic_map_n_param(src_map);
	unsigned dst_nparam = isl_basic_map_n_param(dst_map);
	unsigned src_in = isl_basic_map_n_in(src_map);
	unsigned dst_in = isl_basic_map_n_in(dst_map);
	unsigned src_out = isl_basic_map_n_out(src_map);
	unsigned dst_out = isl_basic_map_n_out(dst_map);
	isl_int_set(dst[0], src[0]);
	isl_seq_cpy(dst+1, src+1, isl_min(dst_nparam, src_nparam));
	if (dst_nparam > src_nparam)
		isl_seq_clr(dst+1+src_nparam,
				dst_nparam - src_nparam);
	isl_seq_clr(dst+1+dst_nparam, in_off);
	isl_seq_cpy(dst+1+dst_nparam+in_off,
		    src+1+src_nparam,
		    isl_min(dst_in-in_off, src_in));
	if (dst_in-in_off > src_in)
		isl_seq_clr(dst+1+dst_nparam+in_off+src_in,
				dst_in - in_off - src_in);
	isl_seq_clr(dst+1+dst_nparam+dst_in, out_off);
	isl_seq_cpy(dst+1+dst_nparam+dst_in+out_off,
		    src+1+src_nparam+src_in,
		    isl_min(dst_out-out_off, src_out));
	if (dst_out-out_off > src_out)
		isl_seq_clr(dst+1+dst_nparam+dst_in+out_off+src_out,
				dst_out - out_off - src_out);
	isl_seq_clr(dst+1+dst_nparam+dst_in+dst_out, div_off);
	isl_seq_cpy(dst+1+dst_nparam+dst_in+dst_out+div_off,
		    src+1+src_nparam+src_in+src_out,
		    isl_min(dst_map->extra-div_off, src_map->n_div));
	if (dst_map->n_div-div_off > src_map->n_div)
		isl_seq_clr(dst+1+dst_nparam+dst_in+dst_out+
				div_off+src_map->n_div,
				dst_map->n_div - div_off - src_map->n_div);
}

static void copy_div(struct isl_basic_map *dst_map, isl_int *dst,
		     struct isl_basic_map *src_map, isl_int *src,
		     unsigned in_off, unsigned out_off, unsigned div_off)
{
	isl_int_set(dst[0], src[0]);
	copy_constraint(dst_map, dst+1, src_map, src+1, in_off, out_off, div_off);
}

static struct isl_basic_map *add_constraints(struct isl_basic_map *bmap1,
		struct isl_basic_map *bmap2, unsigned i_pos, unsigned o_pos)
{
	int i;
	unsigned div_off;

	if (!bmap1 || !bmap2)
		goto error;

	div_off = bmap1->n_div;

	for (i = 0; i < bmap2->n_eq; ++i) {
		int i1 = isl_basic_map_alloc_equality(bmap1);
		if (i1 < 0)
			goto error;
		copy_constraint(bmap1, bmap1->eq[i1], bmap2, bmap2->eq[i],
				i_pos, o_pos, div_off);
	}

	for (i = 0; i < bmap2->n_ineq; ++i) {
		int i1 = isl_basic_map_alloc_inequality(bmap1);
		if (i1 < 0)
			goto error;
		copy_constraint(bmap1, bmap1->ineq[i1], bmap2, bmap2->ineq[i],
				i_pos, o_pos, div_off);
	}

	for (i = 0; i < bmap2->n_div; ++i) {
		int i1 = isl_basic_map_alloc_div(bmap1);
		if (i1 < 0)
			goto error;
		copy_div(bmap1, bmap1->div[i1], bmap2, bmap2->div[i],
			 i_pos, o_pos, div_off);
	}

	isl_basic_map_free(bmap2);

	return bmap1;

error:
	isl_basic_map_free(bmap1);
	isl_basic_map_free(bmap2);
	return NULL;
}

static void copy_constraint_dim_map(isl_int *dst, isl_int *src,
					struct isl_dim_map *dim_map)
{
	int i;

	for (i = 0; i < dim_map->len; ++i) {
		if (dim_map->pos[i] < 0)
			isl_int_set_si(dst[i], 0);
		else
			isl_int_set(dst[i], src[dim_map->pos[i]]);
	}
}

static void copy_div_dim_map(isl_int *dst, isl_int *src,
					struct isl_dim_map *dim_map)
{
	isl_int_set(dst[0], src[0]);
	copy_constraint_dim_map(dst+1, src+1, dim_map);
}

static struct isl_basic_map *add_constraints_dim_map(struct isl_basic_map *dst,
		struct isl_basic_map *src, struct isl_dim_map *dim_map)
{
	int i;

	if (!src || !dst || !dim_map)
		goto error;

	for (i = 0; i < src->n_eq; ++i) {
		int i1 = isl_basic_map_alloc_equality(dst);
		if (i1 < 0)
			goto error;
		copy_constraint_dim_map(dst->eq[i1], src->eq[i], dim_map);
	}

	for (i = 0; i < src->n_ineq; ++i) {
		int i1 = isl_basic_map_alloc_inequality(dst);
		if (i1 < 0)
			goto error;
		copy_constraint_dim_map(dst->ineq[i1], src->ineq[i], dim_map);
	}

	for (i = 0; i < src->n_div; ++i) {
		int i1 = isl_basic_map_alloc_div(dst);
		if (i1 < 0)
			goto error;
		copy_div_dim_map(dst->div[i1], src->div[i], dim_map);
	}

	free(dim_map);
	isl_basic_map_free(src);

	return dst;
error:
	free(dim_map);
	isl_basic_map_free(src);
	isl_basic_map_free(dst);
	return NULL;
}

struct isl_basic_set *isl_basic_set_add_constraints(struct isl_basic_set *bset1,
		struct isl_basic_set *bset2, unsigned pos)
{
	return (struct isl_basic_set *)
		add_constraints((struct isl_basic_map *)bset1,
				(struct isl_basic_map *)bset2, 0, pos);
}

struct isl_basic_map *isl_basic_map_extend_dim(struct isl_basic_map *base,
		struct isl_dim *dim, unsigned extra,
		unsigned n_eq, unsigned n_ineq)
{
	struct isl_basic_map *ext;
	unsigned flags;
	int dims_ok;

	if (!dim)
		goto error;

	if (!base)
		goto error;

	dims_ok = isl_dim_equal(base->dim, dim) &&
		  base->extra >= base->n_div + extra;

	if (dims_ok && room_for_con(base, n_eq + n_ineq) &&
		       room_for_ineq(base, n_ineq)) {
		isl_dim_free(dim);
		return base;
	}

	isl_assert(base->ctx, base->dim->nparam <= dim->nparam, goto error);
	isl_assert(base->ctx, base->dim->n_in <= dim->n_in, goto error);
	isl_assert(base->ctx, base->dim->n_out <= dim->n_out, goto error);
	extra += base->extra;
	n_eq += base->n_eq;
	n_ineq += base->n_ineq;

	ext = isl_basic_map_alloc_dim(dim, extra, n_eq, n_ineq);
	dim = NULL;
	if (!ext)
		goto error;

	if (dims_ok)
		ext->sample = isl_vec_copy(base->sample);
	flags = base->flags;
	ext = add_constraints(ext, base, 0, 0);
	if (ext) {
		ext->flags = flags;
		ISL_F_CLR(ext, ISL_BASIC_SET_FINAL);
	}

	return ext;

error:
	isl_dim_free(dim);
	isl_basic_map_free(base);
	return NULL;
}

struct isl_basic_set *isl_basic_set_extend_dim(struct isl_basic_set *base,
		struct isl_dim *dim, unsigned extra,
		unsigned n_eq, unsigned n_ineq)
{
	return (struct isl_basic_set *)
		isl_basic_map_extend_dim((struct isl_basic_map *)base, dim,
							extra, n_eq, n_ineq);
}

struct isl_basic_map *isl_basic_map_extend_constraints(
		struct isl_basic_map *base, unsigned n_eq, unsigned n_ineq)
{
	if (!base)
		return NULL;
	return isl_basic_map_extend_dim(base, isl_dim_copy(base->dim),
					0, n_eq, n_ineq);
}

struct isl_basic_map *isl_basic_map_extend(struct isl_basic_map *base,
		unsigned nparam, unsigned n_in, unsigned n_out, unsigned extra,
		unsigned n_eq, unsigned n_ineq)
{
	struct isl_basic_map *bmap;
	struct isl_dim *dim;

	if (!base)
		return NULL;
	dim = isl_dim_alloc(base->ctx, nparam, n_in, n_out);
	if (!dim)
		goto error;

	bmap = isl_basic_map_extend_dim(base, dim, extra, n_eq, n_ineq);
	return bmap;
error:
	isl_basic_map_free(base);
	return NULL;
}

struct isl_basic_set *isl_basic_set_extend(struct isl_basic_set *base,
		unsigned nparam, unsigned dim, unsigned extra,
		unsigned n_eq, unsigned n_ineq)
{
	return (struct isl_basic_set *)
		isl_basic_map_extend((struct isl_basic_map *)base,
					nparam, 0, dim, extra, n_eq, n_ineq);
}

struct isl_basic_set *isl_basic_set_extend_constraints(
		struct isl_basic_set *base, unsigned n_eq, unsigned n_ineq)
{
	return (struct isl_basic_set *)
		isl_basic_map_extend_constraints((struct isl_basic_map *)base,
						    n_eq, n_ineq);
}

struct isl_basic_set *isl_basic_set_cow(struct isl_basic_set *bset)
{
	return (struct isl_basic_set *)
		isl_basic_map_cow((struct isl_basic_map *)bset);
}

struct isl_basic_map *isl_basic_map_cow(struct isl_basic_map *bmap)
{
	if (!bmap)
		return NULL;

	if (bmap->ref > 1) {
		bmap->ref--;
		bmap = isl_basic_map_dup(bmap);
	}
	if (bmap)
		ISL_F_CLR(bmap, ISL_BASIC_SET_FINAL);
	return bmap;
}

struct isl_set *isl_set_cow(struct isl_set *set)
{
	if (!set)
		return NULL;

	if (set->ref == 1)
		return set;
	set->ref--;
	return isl_set_dup(set);
}

struct isl_map *isl_map_cow(struct isl_map *map)
{
	if (!map)
		return NULL;

	if (map->ref == 1)
		return map;
	map->ref--;
	return isl_map_dup(map);
}

static void swap_vars(struct isl_blk blk, isl_int *a,
			unsigned a_len, unsigned b_len)
{
	isl_seq_cpy(blk.data, a+a_len, b_len);
	isl_seq_cpy(blk.data+b_len, a, a_len);
	isl_seq_cpy(a, blk.data, b_len+a_len);
}

struct isl_basic_set *isl_basic_set_swap_vars(
		struct isl_basic_set *bset, unsigned n)
{
	int i;
	struct isl_blk blk;
	unsigned dim;
	unsigned nparam;

	if (!bset)
		goto error;

	nparam = isl_basic_set_n_param(bset);
	dim = isl_basic_set_n_dim(bset);
	isl_assert(bset->ctx, n <= dim, goto error);

	if (n == dim)
		return bset;

	bset = isl_basic_set_cow(bset);
	if (!bset)
		return NULL;

	blk = isl_blk_alloc(bset->ctx, dim);
	if (isl_blk_is_error(blk))
		goto error;

	for (i = 0; i < bset->n_eq; ++i)
		swap_vars(blk,
			  bset->eq[i]+1+nparam, n, dim - n);

	for (i = 0; i < bset->n_ineq; ++i)
		swap_vars(blk,
			  bset->ineq[i]+1+nparam, n, dim - n);

	for (i = 0; i < bset->n_div; ++i)
		swap_vars(blk,
			  bset->div[i]+1+1+nparam, n, dim - n);

	isl_blk_free(bset->ctx, blk);

	ISL_F_CLR(bset, ISL_BASIC_SET_NORMALIZED);
	return isl_basic_set_gauss(bset, NULL);
error:
	isl_basic_set_free(bset);
	return NULL;
}

struct isl_set *isl_set_swap_vars(struct isl_set *set, unsigned n)
{
	int i;
	set = isl_set_cow(set);
	if (!set)
		return NULL;

	for (i = 0; i < set->n; ++i) {
		set->p[i] = isl_basic_set_swap_vars(set->p[i], n);
		if (!set->p[i]) {
			isl_set_free(set);
			return NULL;
		}
	}
	ISL_F_CLR(set, ISL_SET_NORMALIZED);
	return set;
}

struct isl_basic_map *isl_basic_map_set_to_empty(struct isl_basic_map *bmap)
{
	int i = 0;
	unsigned total;
	if (!bmap)
		goto error;
	total = isl_basic_map_total_dim(bmap);
	isl_basic_map_free_div(bmap, bmap->n_div);
	isl_basic_map_free_inequality(bmap, bmap->n_ineq);
	if (bmap->n_eq > 0)
		isl_basic_map_free_equality(bmap, bmap->n_eq-1);
	else {
		i = isl_basic_map_alloc_equality(bmap);
		if (i < 0)
			goto error;
	}
	isl_int_set_si(bmap->eq[i][0], 1);
	isl_seq_clr(bmap->eq[i]+1, total);
	ISL_F_SET(bmap, ISL_BASIC_MAP_EMPTY);
	isl_vec_free(bmap->sample);
	bmap->sample = NULL;
	return isl_basic_map_finalize(bmap);
error:
	isl_basic_map_free(bmap);
	return NULL;
}

struct isl_basic_set *isl_basic_set_set_to_empty(struct isl_basic_set *bset)
{
	return (struct isl_basic_set *)
		isl_basic_map_set_to_empty((struct isl_basic_map *)bset);
}

void isl_basic_map_swap_div(struct isl_basic_map *bmap, int a, int b)
{
	int i;
	unsigned off = isl_dim_total(bmap->dim);
	isl_int *t = bmap->div[a];
	bmap->div[a] = bmap->div[b];
	bmap->div[b] = t;

	for (i = 0; i < bmap->n_eq; ++i)
		isl_int_swap(bmap->eq[i][1+off+a], bmap->eq[i][1+off+b]);

	for (i = 0; i < bmap->n_ineq; ++i)
		isl_int_swap(bmap->ineq[i][1+off+a], bmap->ineq[i][1+off+b]);

	for (i = 0; i < bmap->n_div; ++i)
		isl_int_swap(bmap->div[i][1+1+off+a], bmap->div[i][1+1+off+b]);
	ISL_F_CLR(bmap, ISL_BASIC_MAP_NORMALIZED);
}

/* Eliminate the specified n dimensions starting at first from the
 * constraints using Fourier-Motzkin.  The dimensions themselves
 * are not removed.
 */
__isl_give isl_map *isl_map_eliminate(__isl_take isl_map *map,
	enum isl_dim_type type, unsigned first, unsigned n)
{
	int i;

	if (!map)
		return NULL;
	if (n == 0)
		return map;

	map = isl_map_cow(map);
	if (!map)
		return NULL;
	isl_assert(map->ctx, first + n <= isl_map_dim(map, type), goto error);
	first += pos(map->dim, type) - 1;
	
	for (i = 0; i < map->n; ++i) {
		map->p[i] = isl_basic_map_eliminate_vars(map->p[i], first, n);
		if (!map->p[i])
			goto error;
	}
	return map;
error:
	isl_map_free(map);
	return NULL;
}

/* Eliminate the specified n dimensions starting at first from the
 * constraints using Fourier-Motzkin.  The dimensions themselves
 * are not removed.
 */
__isl_give isl_set *isl_set_eliminate(__isl_take isl_set *set,
	enum isl_dim_type type, unsigned first, unsigned n)
{
	return (isl_set *)isl_map_eliminate((isl_map *)set, type, first, n);
}

/* Eliminate the specified n dimensions starting at first from the
 * constraints using Fourier-Motzkin.  The dimensions themselves
 * are not removed.
 */
__isl_give isl_set *isl_set_eliminate_dims(__isl_take isl_set *set,
	unsigned first, unsigned n)
{
	return isl_set_eliminate(set, isl_dim_set, first, n);
}

/* Project out n dimensions starting at first using Fourier-Motzkin */
struct isl_set *isl_set_remove_dims(struct isl_set *set,
	unsigned first, unsigned n)
{
	set = isl_set_eliminate_dims(set, first, n);
	set = isl_set_drop_dims(set, first, n);
	return set;
}

struct isl_basic_set *isl_basic_set_remove_divs(struct isl_basic_set *bset)
{
	bset = isl_basic_set_eliminate_vars(bset, isl_dim_total(bset->dim),
						bset->n_div);
	if (!bset)
		return NULL;
	bset->n_div = 0;
	return bset;
}

struct isl_set *isl_set_remove_divs(struct isl_set *set)
{
	int i;

	if (!set)
		return NULL;
	if (set->n == 0)
		return set;

	set = isl_set_cow(set);
	if (!set)
		return NULL;
	
	for (i = 0; i < set->n; ++i) {
		set->p[i] = isl_basic_set_remove_divs(set->p[i]);
		if (!set->p[i])
			goto error;
	}
	return set;
error:
	isl_set_free(set);
	return NULL;
}

struct isl_basic_map *isl_basic_map_remove(struct isl_basic_map *bmap,
	enum isl_dim_type type, unsigned first, unsigned n)
{
	if (!bmap)
		return NULL;
	isl_assert(bmap->ctx, first + n <= isl_basic_map_dim(bmap, type),
			goto error);
	if (n == 0)
		return bmap;
	bmap = isl_basic_map_eliminate_vars(bmap,
			isl_basic_map_offset(bmap, type) - 1 + first, n);
	if (!bmap)
		return bmap;
	if (ISL_F_ISSET(bmap, ISL_BASIC_MAP_EMPTY) && type == isl_dim_div)
		return bmap;
	bmap = isl_basic_map_drop(bmap, type, first, n);
	return bmap;
error:
	isl_basic_map_free(bmap);
	return NULL;
}

__isl_give isl_basic_set *isl_basic_set_remove(__isl_take isl_basic_set *bset,
	enum isl_dim_type type, unsigned first, unsigned n)
{
	return (isl_basic_set *)
		isl_basic_map_remove((isl_basic_map *)bset, type, first, n);
}

struct isl_map *isl_map_remove(struct isl_map *map,
	enum isl_dim_type type, unsigned first, unsigned n)
{
	int i;

	if (n == 0)
		return map;

	map = isl_map_cow(map);
	if (!map)
		return NULL;
	isl_assert(map->ctx, first + n <= isl_map_dim(map, type), goto error);
	
	for (i = 0; i < map->n; ++i) {
		map->p[i] = isl_basic_map_eliminate_vars(map->p[i],
			isl_basic_map_offset(map->p[i], type) - 1 + first, n);
		if (!map->p[i])
			goto error;
	}
	map = isl_map_drop(map, type, first, n);
	return map;
error:
	isl_map_free(map);
	return NULL;
}

__isl_give isl_set *isl_set_remove(__isl_take isl_set *bset,
	enum isl_dim_type type, unsigned first, unsigned n)
{
	return (isl_set *)isl_map_remove((isl_map *)bset, type, first, n);
}

/* Project out n inputs starting at first using Fourier-Motzkin */
struct isl_map *isl_map_remove_inputs(struct isl_map *map,
	unsigned first, unsigned n)
{
	return isl_map_remove(map, isl_dim_in, first, n);
}

/* Project out n dimensions starting at first using Fourier-Motzkin */
struct isl_basic_set *isl_basic_set_remove_dims(struct isl_basic_set *bset,
	unsigned first, unsigned n)
{
	unsigned nparam = isl_basic_set_n_param(bset);
	bset = isl_basic_set_eliminate_vars(bset, nparam + first, n);
	bset = isl_basic_set_drop_dims(bset, first, n);
	return bset;
}

static void dump_term(struct isl_basic_map *bmap,
			isl_int c, int pos, FILE *out)
{
	const char *name;
	unsigned in = isl_basic_map_n_in(bmap);
	unsigned dim = in + isl_basic_map_n_out(bmap);
	unsigned nparam = isl_basic_map_n_param(bmap);
	if (!pos)
		isl_int_print(out, c, 0);
	else {
		if (!isl_int_is_one(c))
			isl_int_print(out, c, 0);
		if (pos < 1 + nparam) {
			name = isl_dim_get_name(bmap->dim,
						isl_dim_param, pos - 1);
			if (name)
				fprintf(out, "%s", name);
			else
				fprintf(out, "p%d", pos - 1);
		} else if (pos < 1 + nparam + in)
			fprintf(out, "i%d", pos - 1 - nparam);
		else if (pos < 1 + nparam + dim)
			fprintf(out, "o%d", pos - 1 - nparam - in);
		else
			fprintf(out, "e%d", pos - 1 - nparam - dim);
	}
}

static void dump_constraint_sign(struct isl_basic_map *bmap, isl_int *c,
				int sign, FILE *out)
{
	int i;
	int first;
	unsigned len = 1 + isl_basic_map_total_dim(bmap);
	isl_int v;

	isl_int_init(v);
	for (i = 0, first = 1; i < len; ++i) {
		if (isl_int_sgn(c[i]) * sign <= 0)
			continue;
		if (!first)
			fprintf(out, " + ");
		first = 0;
		isl_int_abs(v, c[i]);
		dump_term(bmap, v, i, out);
	}
	isl_int_clear(v);
	if (first)
		fprintf(out, "0");
}

static void dump_constraint(struct isl_basic_map *bmap, isl_int *c,
				const char *op, FILE *out, int indent)
{
	int i;

	fprintf(out, "%*s", indent, "");

	dump_constraint_sign(bmap, c, 1, out);
	fprintf(out, " %s ", op);
	dump_constraint_sign(bmap, c, -1, out);

	fprintf(out, "\n");

	for (i = bmap->n_div; i < bmap->extra; ++i) {
		if (isl_int_is_zero(c[1+isl_dim_total(bmap->dim)+i]))
			continue;
		fprintf(out, "%*s", indent, "");
		fprintf(out, "ERROR: unused div coefficient not zero\n");
		abort();
	}
}

static void dump_constraints(struct isl_basic_map *bmap,
				isl_int **c, unsigned n,
				const char *op, FILE *out, int indent)
{
	int i;

	for (i = 0; i < n; ++i)
		dump_constraint(bmap, c[i], op, out, indent);
}

static void dump_affine(struct isl_basic_map *bmap, isl_int *exp, FILE *out)
{
	int j;
	int first = 1;
	unsigned total = isl_basic_map_total_dim(bmap);

	for (j = 0; j < 1 + total; ++j) {
		if (isl_int_is_zero(exp[j]))
			continue;
		if (!first && isl_int_is_pos(exp[j]))
			fprintf(out, "+");
		dump_term(bmap, exp[j], j, out);
		first = 0;
	}
}

static void dump(struct isl_basic_map *bmap, FILE *out, int indent)
{
	int i;

	dump_constraints(bmap, bmap->eq, bmap->n_eq, "=", out, indent);
	dump_constraints(bmap, bmap->ineq, bmap->n_ineq, ">=", out, indent);

	for (i = 0; i < bmap->n_div; ++i) {
		fprintf(out, "%*s", indent, "");
		fprintf(out, "e%d = [(", i);
		dump_affine(bmap, bmap->div[i]+1, out);
		fprintf(out, ")/");
		isl_int_print(out, bmap->div[i][0], 0);
		fprintf(out, "]\n");
	}
}

void isl_basic_set_dump(struct isl_basic_set *bset, FILE *out, int indent)
{
	if (!bset) {
		fprintf(out, "null basic set\n");
		return;
	}

	fprintf(out, "%*s", indent, "");
	fprintf(out, "ref: %d, nparam: %d, dim: %d, extra: %d, flags: %x\n",
			bset->ref, bset->dim->nparam, bset->dim->n_out,
			bset->extra, bset->flags);
	dump((struct isl_basic_map *)bset, out, indent);
}

void isl_basic_map_dump(struct isl_basic_map *bmap, FILE *out, int indent)
{
	if (!bmap) {
		fprintf(out, "null basic map\n");
		return;
	}

	fprintf(out, "%*s", indent, "");
	fprintf(out, "ref: %d, nparam: %d, in: %d, out: %d, extra: %d, "
			"flags: %x, n_name: %d\n",
		bmap->ref,
		bmap->dim->nparam, bmap->dim->n_in, bmap->dim->n_out,
		bmap->extra, bmap->flags, bmap->dim->n_name);
	dump(bmap, out, indent);
}

int isl_inequality_negate(struct isl_basic_map *bmap, unsigned pos)
{
	unsigned total;
	if (!bmap)
		return -1;
	total = isl_basic_map_total_dim(bmap);
	isl_assert(bmap->ctx, pos < bmap->n_ineq, return -1);
	isl_seq_neg(bmap->ineq[pos], bmap->ineq[pos], 1 + total);
	isl_int_sub_ui(bmap->ineq[pos][0], bmap->ineq[pos][0], 1);
	ISL_F_CLR(bmap, ISL_BASIC_MAP_NORMALIZED);
	return 0;
}

struct isl_set *isl_set_alloc_dim(struct isl_dim *dim, int n, unsigned flags)
{
	struct isl_set *set;

	if (!dim)
		return NULL;
	isl_assert(dim->ctx, dim->n_in == 0, return NULL);
	isl_assert(dim->ctx, n >= 0, return NULL);
	set = isl_alloc(dim->ctx, struct isl_set,
			sizeof(struct isl_set) +
			(n - 1) * sizeof(struct isl_basic_set *));
	if (!set)
		goto error;

	set->ctx = dim->ctx;
	isl_ctx_ref(set->ctx);
	set->ref = 1;
	set->size = n;
	set->n = 0;
	set->dim = dim;
	set->flags = flags;
	return set;
error:
	isl_dim_free(dim);
	return NULL;
}

struct isl_set *isl_set_alloc(struct isl_ctx *ctx,
		unsigned nparam, unsigned dim, int n, unsigned flags)
{
	struct isl_set *set;
	struct isl_dim *dims;

	dims = isl_dim_alloc(ctx, nparam, 0, dim);
	if (!dims)
		return NULL;

	set = isl_set_alloc_dim(dims, n, flags);
	return set;
}

/* Make sure "map" has room for at least "n" more basic maps.
 */
struct isl_map *isl_map_grow(struct isl_map *map, int n)
{
	int i;
	struct isl_map *grown = NULL;

	if (!map)
		return NULL;
	isl_assert(map->ctx, n >= 0, goto error);
	if (map->n + n <= map->size)
		return map;
	grown = isl_map_alloc_dim(isl_map_get_dim(map), map->n + n, map->flags);
	if (!grown)
		goto error;
	for (i = 0; i < map->n; ++i) {
		grown->p[i] = isl_basic_map_copy(map->p[i]);
		if (!grown->p[i])
			goto error;
		grown->n++;
	}
	isl_map_free(map);
	return grown;
error:
	isl_map_free(grown);
	isl_map_free(map);
	return NULL;
}

/* Make sure "set" has room for at least "n" more basic sets.
 */
struct isl_set *isl_set_grow(struct isl_set *set, int n)
{
	return (struct isl_set *)isl_map_grow((struct isl_map *)set, n);
}

struct isl_set *isl_set_dup(struct isl_set *set)
{
	int i;
	struct isl_set *dup;

	if (!set)
		return NULL;

	dup = isl_set_alloc_dim(isl_dim_copy(set->dim), set->n, set->flags);
	if (!dup)
		return NULL;
	for (i = 0; i < set->n; ++i)
		dup = isl_set_add_basic_set(dup, isl_basic_set_copy(set->p[i]));
	return dup;
}

struct isl_set *isl_set_from_basic_set(struct isl_basic_set *bset)
{
	struct isl_set *set;

	if (!bset)
		return NULL;

	set = isl_set_alloc_dim(isl_dim_copy(bset->dim), 1, ISL_MAP_DISJOINT);
	if (!set) {
		isl_basic_set_free(bset);
		return NULL;
	}
	return isl_set_add_basic_set(set, bset);
}

struct isl_map *isl_map_from_basic_map(struct isl_basic_map *bmap)
{
	struct isl_map *map;

	if (!bmap)
		return NULL;

	map = isl_map_alloc_dim(isl_dim_copy(bmap->dim), 1, ISL_MAP_DISJOINT);
	if (!map) {
		isl_basic_map_free(bmap);
		return NULL;
	}
	return isl_map_add_basic_map(map, bmap);
}

__isl_give isl_set *isl_set_add_basic_set(__isl_take isl_set *set,
						__isl_take isl_basic_set *bset)
{
	return (struct isl_set *)isl_map_add_basic_map((struct isl_map *)set,
						(struct isl_basic_map *)bset);
}

void isl_set_free(struct isl_set *set)
{
	int i;

	if (!set)
		return;

	if (--set->ref > 0)
		return;

	isl_ctx_deref(set->ctx);
	for (i = 0; i < set->n; ++i)
		isl_basic_set_free(set->p[i]);
	isl_dim_free(set->dim);
	free(set);
}

void isl_set_dump(struct isl_set *set, FILE *out, int indent)
{
	int i;

	if (!set) {
		fprintf(out, "null set\n");
		return;
	}

	fprintf(out, "%*s", indent, "");
	fprintf(out, "ref: %d, n: %d, nparam: %d, dim: %d, flags: %x\n",
			set->ref, set->n, set->dim->nparam, set->dim->n_out,
			set->flags);
	for (i = 0; i < set->n; ++i) {
		fprintf(out, "%*s", indent, "");
		fprintf(out, "basic set %d:\n", i);
		isl_basic_set_dump(set->p[i], out, indent+4);
	}
}

void isl_map_dump(struct isl_map *map, FILE *out, int indent)
{
	int i;

	if (!map) {
		fprintf(out, "null map\n");
		return;
	}

	fprintf(out, "%*s", indent, "");
	fprintf(out, "ref: %d, n: %d, nparam: %d, in: %d, out: %d, "
		     "flags: %x, n_name: %d\n",
			map->ref, map->n, map->dim->nparam, map->dim->n_in,
			map->dim->n_out, map->flags, map->dim->n_name);
	for (i = 0; i < map->n; ++i) {
		fprintf(out, "%*s", indent, "");
		fprintf(out, "basic map %d:\n", i);
		isl_basic_map_dump(map->p[i], out, indent+4);
	}
}

struct isl_basic_map *isl_basic_map_intersect_domain(
		struct isl_basic_map *bmap, struct isl_basic_set *bset)
{
	struct isl_basic_map *bmap_domain;
	struct isl_dim *dim;

	if (!bmap || !bset)
		goto error;

	isl_assert(bset->ctx, isl_dim_match(bmap->dim, isl_dim_param,
					bset->dim, isl_dim_param), goto error);

	if (isl_dim_size(bset->dim, isl_dim_set) != 0)
		isl_assert(bset->ctx,
		    isl_basic_map_compatible_domain(bmap, bset), goto error);

	bmap = isl_basic_map_cow(bmap);
	if (!bmap)
		goto error;
	bmap = isl_basic_map_extend_dim(bmap, isl_dim_copy(bmap->dim),
			bset->n_div, bset->n_eq, bset->n_ineq);
	dim = isl_dim_reverse(isl_dim_copy(bset->dim));
	bmap_domain = isl_basic_map_from_basic_set(bset, dim);
	bmap = add_constraints(bmap, bmap_domain, 0, 0);

	bmap = isl_basic_map_simplify(bmap);
	return isl_basic_map_finalize(bmap);
error:
	isl_basic_map_free(bmap);
	isl_basic_set_free(bset);
	return NULL;
}

struct isl_basic_map *isl_basic_map_intersect_range(
		struct isl_basic_map *bmap, struct isl_basic_set *bset)
{
	struct isl_basic_map *bmap_range;

	if (!bmap || !bset)
		goto error;

	isl_assert(bset->ctx, isl_dim_match(bmap->dim, isl_dim_param,
					bset->dim, isl_dim_param), goto error);

	if (isl_dim_size(bset->dim, isl_dim_set) != 0)
		isl_assert(bset->ctx,
		    isl_basic_map_compatible_range(bmap, bset), goto error);

	bmap = isl_basic_map_cow(bmap);
	if (!bmap)
		goto error;
	bmap = isl_basic_map_extend_dim(bmap, isl_dim_copy(bmap->dim),
			bset->n_div, bset->n_eq, bset->n_ineq);
	bmap_range = isl_basic_map_from_basic_set(bset, isl_dim_copy(bset->dim));
	bmap = add_constraints(bmap, bmap_range, 0, 0);

	bmap = isl_basic_map_simplify(bmap);
	return isl_basic_map_finalize(bmap);
error:
	isl_basic_map_free(bmap);
	isl_basic_set_free(bset);
	return NULL;
}

int isl_basic_map_contains(struct isl_basic_map *bmap, struct isl_vec *vec)
{
	int i;
	unsigned total;
	isl_int s;

	total = 1 + isl_basic_map_total_dim(bmap);
	if (total != vec->size)
		return -1;

	isl_int_init(s);

	for (i = 0; i < bmap->n_eq; ++i) {
		isl_seq_inner_product(vec->el, bmap->eq[i], total, &s);
		if (!isl_int_is_zero(s)) {
			isl_int_clear(s);
			return 0;
		}
	}

	for (i = 0; i < bmap->n_ineq; ++i) {
		isl_seq_inner_product(vec->el, bmap->ineq[i], total, &s);
		if (isl_int_is_neg(s)) {
			isl_int_clear(s);
			return 0;
		}
	}

	isl_int_clear(s);

	return 1;
}

int isl_basic_set_contains(struct isl_basic_set *bset, struct isl_vec *vec)
{
	return isl_basic_map_contains((struct isl_basic_map *)bset, vec);
}

struct isl_basic_map *isl_basic_map_intersect(
		struct isl_basic_map *bmap1, struct isl_basic_map *bmap2)
{
	struct isl_vec *sample = NULL;

	if (!bmap1 || !bmap2)
		goto error;

	isl_assert(bmap1->ctx, isl_dim_match(bmap1->dim, isl_dim_param,
				     bmap2->dim, isl_dim_param), goto error);
	if (isl_dim_total(bmap1->dim) ==
				isl_dim_size(bmap1->dim, isl_dim_param) &&
	    isl_dim_total(bmap2->dim) !=
				isl_dim_size(bmap2->dim, isl_dim_param))
		return isl_basic_map_intersect(bmap2, bmap1);

	if (isl_dim_total(bmap2->dim) !=
					isl_dim_size(bmap2->dim, isl_dim_param))
		isl_assert(bmap1->ctx,
			    isl_dim_equal(bmap1->dim, bmap2->dim), goto error);

	if (bmap1->sample &&
	    isl_basic_map_contains(bmap1, bmap1->sample) > 0 &&
	    isl_basic_map_contains(bmap2, bmap1->sample) > 0)
		sample = isl_vec_copy(bmap1->sample);
	else if (bmap2->sample &&
	    isl_basic_map_contains(bmap1, bmap2->sample) > 0 &&
	    isl_basic_map_contains(bmap2, bmap2->sample) > 0)
		sample = isl_vec_copy(bmap2->sample);

	bmap1 = isl_basic_map_cow(bmap1);
	if (!bmap1)
		goto error;
	bmap1 = isl_basic_map_extend_dim(bmap1, isl_dim_copy(bmap1->dim),
			bmap2->n_div, bmap2->n_eq, bmap2->n_ineq);
	bmap1 = add_constraints(bmap1, bmap2, 0, 0);

	if (!bmap1)
		isl_vec_free(sample);
	else if (sample) {
		isl_vec_free(bmap1->sample);
		bmap1->sample = sample;
	}

	bmap1 = isl_basic_map_simplify(bmap1);
	return isl_basic_map_finalize(bmap1);
error:
	if (sample)
		isl_vec_free(sample);
	isl_basic_map_free(bmap1);
	isl_basic_map_free(bmap2);
	return NULL;
}

struct isl_basic_set *isl_basic_set_intersect(
		struct isl_basic_set *bset1, struct isl_basic_set *bset2)
{
	return (struct isl_basic_set *)
		isl_basic_map_intersect(
			(struct isl_basic_map *)bset1,
			(struct isl_basic_map *)bset2);
}

/* Special case of isl_map_intersect, where both map1 and map2
 * are convex, without any divs and such that either map1 or map2
 * contains a single constraint.  This constraint is then simply
 * added to the other map.
 */
static __isl_give isl_map *map_intersect_add_constraint(
	__isl_take isl_map *map1, __isl_take isl_map *map2)
{
	struct isl_basic_map *bmap1;
	struct isl_basic_map *bmap2;

	isl_assert(map1->ctx, map1->n == 1, goto error);
	isl_assert(map2->ctx, map1->n == 1, goto error);
	isl_assert(map1->ctx, map1->p[0]->n_div == 0, goto error);
	isl_assert(map2->ctx, map1->p[0]->n_div == 0, goto error);

	if (map2->p[0]->n_eq + map2->p[0]->n_ineq != 1)
		return isl_map_intersect(map2, map1);

	isl_assert(map2->ctx,
		    map2->p[0]->n_eq + map2->p[0]->n_ineq == 1, goto error);

	map1 = isl_map_cow(map1);
	if (!map1)
		goto error;
	if (isl_map_fast_is_empty(map1)) {
		isl_map_free(map2);
		return map1;
	}
	map1->p[0] = isl_basic_map_cow(map1->p[0]);
	if (map2->p[0]->n_eq == 1)
		map1->p[0] = isl_basic_map_add_eq(map1->p[0], map2->p[0]->eq[0]);
	else
		map1->p[0] = isl_basic_map_add_ineq(map1->p[0],
							map2->p[0]->ineq[0]);

	map1->p[0] = isl_basic_map_simplify(map1->p[0]);
	map1->p[0] = isl_basic_map_finalize(map1->p[0]);
	if (!map1->p[0])
		goto error;

	if (isl_basic_map_fast_is_empty(map1->p[0])) {
		isl_basic_map_free(map1->p[0]);
		map1->n = 0;
	}

	isl_map_free(map2);

	return map1;
error:
	isl_map_free(map1);
	isl_map_free(map2);
	return NULL;
}

struct isl_map *isl_map_intersect(struct isl_map *map1, struct isl_map *map2)
{
	unsigned flags = 0;
	struct isl_map *result;
	int i, j;

	if (!map1 || !map2)
		goto error;

	if (isl_map_fast_is_empty(map1)) {
		isl_map_free(map2);
		return map1;
	}
	if (isl_map_fast_is_empty(map2)) {
		isl_map_free(map1);
		return map2;
	}

	if (map1->n == 1 && map2->n == 1 &&
	    map1->p[0]->n_div == 0 && map2->p[0]->n_div == 0 &&
	    isl_dim_equal(map1->dim, map2->dim) &&
	    (map1->p[0]->n_eq + map1->p[0]->n_ineq == 1 ||
	     map2->p[0]->n_eq + map2->p[0]->n_ineq == 1))
		return map_intersect_add_constraint(map1, map2);
	isl_assert(map1->ctx, isl_dim_match(map1->dim, isl_dim_param,
					 map2->dim, isl_dim_param), goto error);
	if (isl_dim_total(map1->dim) ==
				isl_dim_size(map1->dim, isl_dim_param) &&
	    isl_dim_total(map2->dim) != isl_dim_size(map2->dim, isl_dim_param))
		return isl_map_intersect(map2, map1);

	if (isl_dim_total(map2->dim) != isl_dim_size(map2->dim, isl_dim_param))
		isl_assert(map1->ctx,
			    isl_dim_equal(map1->dim, map2->dim), goto error);

	if (ISL_F_ISSET(map1, ISL_MAP_DISJOINT) &&
	    ISL_F_ISSET(map2, ISL_MAP_DISJOINT))
		ISL_FL_SET(flags, ISL_MAP_DISJOINT);

	result = isl_map_alloc_dim(isl_dim_copy(map1->dim),
				map1->n * map2->n, flags);
	if (!result)
		goto error;
	for (i = 0; i < map1->n; ++i)
		for (j = 0; j < map2->n; ++j) {
			struct isl_basic_map *part;
			part = isl_basic_map_intersect(
				    isl_basic_map_copy(map1->p[i]),
				    isl_basic_map_copy(map2->p[j]));
			if (isl_basic_map_is_empty(part))
				isl_basic_map_free(part);
			else
				result = isl_map_add_basic_map(result, part);
			if (!result)
				goto error;
		}
	isl_map_free(map1);
	isl_map_free(map2);
	return result;
error:
	isl_map_free(map1);
	isl_map_free(map2);
	return NULL;
}

struct isl_set *isl_set_intersect(struct isl_set *set1, struct isl_set *set2)
{
	return (struct isl_set *)
		isl_map_intersect((struct isl_map *)set1,
				  (struct isl_map *)set2);
}

struct isl_basic_map *isl_basic_map_reverse(struct isl_basic_map *bmap)
{
	struct isl_dim *dim;
	struct isl_basic_set *bset;
	unsigned in;

	if (!bmap)
		return NULL;
	bmap = isl_basic_map_cow(bmap);
	if (!bmap)
		return NULL;
	dim = isl_dim_reverse(isl_dim_copy(bmap->dim));
	in = isl_basic_map_n_in(bmap);
	bset = isl_basic_set_from_basic_map(bmap);
	bset = isl_basic_set_swap_vars(bset, in);
	return isl_basic_map_from_basic_set(bset, dim);
}

__isl_give isl_basic_map *isl_basic_map_insert(__isl_take isl_basic_map *bmap,
		enum isl_dim_type type, unsigned pos, unsigned n)
{
	struct isl_dim *res_dim;
	struct isl_basic_map *res;
	struct isl_dim_map *dim_map;
	unsigned total, off;
	enum isl_dim_type t;

	if (n == 0)
		return bmap;

	if (!bmap)
		return NULL;

	res_dim = isl_dim_insert(isl_basic_map_get_dim(bmap), type, pos, n);

	total = isl_basic_map_total_dim(bmap) + n;
	dim_map = isl_dim_map_alloc(bmap->ctx, total);
	off = 0;
	for (t = isl_dim_param; t <= isl_dim_out; ++t) {
		if (t != type) {
			isl_dim_map_dim(dim_map, bmap->dim, t, off);
		} else {
			unsigned size = isl_basic_map_dim(bmap, t);
			isl_dim_map_dim_range(dim_map, bmap->dim, t,
						0, pos, off);
			isl_dim_map_dim_range(dim_map, bmap->dim, t,
						pos, size - pos, off + pos + n);
		}
		off += isl_dim_size(res_dim, t);
	}
	isl_dim_map_div(dim_map, bmap, off);

	res = isl_basic_map_alloc_dim(res_dim,
			bmap->n_div, bmap->n_eq, bmap->n_ineq);
	res = add_constraints_dim_map(res, bmap, dim_map);
	res = isl_basic_map_simplify(res);
	return isl_basic_map_finalize(res);
}

__isl_give isl_basic_map *isl_basic_map_add(__isl_take isl_basic_map *bmap,
		enum isl_dim_type type, unsigned n)
{
	if (!bmap)
		return NULL;
	return isl_basic_map_insert(bmap, type,
					isl_basic_map_dim(bmap, type), n);
}

__isl_give isl_basic_set *isl_basic_set_add(__isl_take isl_basic_set *bset,
		enum isl_dim_type type, unsigned n)
{
	if (!bset)
		return NULL;
	isl_assert(bset->ctx, type != isl_dim_in, goto error);
	return (isl_basic_set *)isl_basic_map_add((isl_basic_map *)bset, type, n);
error:
	isl_basic_set_free(bset);
	return NULL;
}

__isl_give isl_map *isl_map_insert(__isl_take isl_map *map,
		enum isl_dim_type type, unsigned pos, unsigned n)
{
	int i;

	if (n == 0)
		return map;

	map = isl_map_cow(map);
	if (!map)
		return NULL;

	map->dim = isl_dim_insert(map->dim, type, pos, n);
	if (!map->dim)
		goto error;

	for (i = 0; i < map->n; ++i) {
		map->p[i] = isl_basic_map_insert(map->p[i], type, pos, n);
		if (!map->p[i])
			goto error;
	}

	return map;
error:
	isl_map_free(map);
	return NULL;
}

__isl_give isl_map *isl_map_add(__isl_take isl_map *map,
		enum isl_dim_type type, unsigned n)
{
	if (!map)
		return NULL;
	return isl_map_insert(map, type, isl_map_dim(map, type), n);
}

__isl_give isl_set *isl_set_add(__isl_take isl_set *set,
		enum isl_dim_type type, unsigned n)
{
	if (!set)
		return NULL;
	isl_assert(set->ctx, type != isl_dim_in, goto error);
	return (isl_set *)isl_map_add((isl_map *)set, type, n);
error:
	isl_set_free(set);
	return NULL;
}

__isl_give isl_basic_map *isl_basic_map_move_dims(
	__isl_take isl_basic_map *bmap,
	enum isl_dim_type dst_type, unsigned dst_pos,
	enum isl_dim_type src_type, unsigned src_pos, unsigned n)
{
	int i;
	struct isl_dim_map *dim_map;
	struct isl_basic_map *res;
	enum isl_dim_type t;
	unsigned total, off;

	if (!bmap)
		return NULL;
	if (n == 0)
		return bmap;

	isl_assert(bmap->ctx, src_pos + n <= isl_basic_map_dim(bmap, src_type),
		goto error);

	if (dst_type == src_type && dst_pos == src_pos)
		return bmap;

	isl_assert(bmap->ctx, dst_type != src_type, goto error);

	if (pos(bmap->dim, dst_type) + dst_pos ==
	    pos(bmap->dim, src_type) + src_pos +
					    ((src_type < dst_type) ? n : 0)) {
		bmap = isl_basic_map_cow(bmap);
		if (!bmap)
			return NULL;

		bmap->dim = isl_dim_move(bmap->dim, dst_type, dst_pos,
						src_type, src_pos, n);
		if (!bmap->dim)
			goto error;

		bmap = isl_basic_map_finalize(bmap);

		return bmap;
	}

	total = isl_basic_map_total_dim(bmap);
	dim_map = isl_dim_map_alloc(bmap->ctx, total);

	off = 0;
	for (t = isl_dim_param; t <= isl_dim_out; ++t) {
		unsigned size = isl_dim_size(bmap->dim, t);
		if (t == dst_type) {
			isl_dim_map_dim_range(dim_map, bmap->dim, t,
					    0, dst_pos, off);
			off += dst_pos;
			isl_dim_map_dim_range(dim_map, bmap->dim, src_type,
					    src_pos, n, off);
			off += n;
			isl_dim_map_dim_range(dim_map, bmap->dim, t,
					    dst_pos, size - dst_pos, off);
			off += size - dst_pos;
		} else if (t == src_type) {
			isl_dim_map_dim_range(dim_map, bmap->dim, t,
					    0, src_pos, off);
			off += src_pos;
			isl_dim_map_dim_range(dim_map, bmap->dim, t,
					src_pos + n, size - src_pos - n, off);
			off += size - src_pos - n;
		} else {
			isl_dim_map_dim(dim_map, bmap->dim, t, off);
			off += size;
		}
	}
	isl_dim_map_div(dim_map, bmap, off + n);

	res = isl_basic_map_alloc_dim(isl_basic_map_get_dim(bmap),
			bmap->n_div, bmap->n_eq, bmap->n_ineq);
	bmap = add_constraints_dim_map(res, bmap, dim_map);

	bmap->dim = isl_dim_move(bmap->dim, dst_type, dst_pos,
					src_type, src_pos, n);
	if (!bmap->dim)
		goto error;

	ISL_F_CLR(bmap, ISL_BASIC_MAP_NORMALIZED);
	bmap = isl_basic_map_gauss(bmap, NULL);
	bmap = isl_basic_map_finalize(bmap);

	return bmap;
error:
	isl_basic_map_free(bmap);
	return NULL;
}

__isl_give isl_basic_set *isl_basic_set_move_dims(__isl_take isl_basic_set *bset,
	enum isl_dim_type dst_type, unsigned dst_pos,
	enum isl_dim_type src_type, unsigned src_pos, unsigned n)
{
	return (isl_basic_set *)isl_basic_map_move_dims(
		(isl_basic_map *)bset, dst_type, dst_pos, src_type, src_pos, n);
}

__isl_give isl_set *isl_set_move_dims(__isl_take isl_set *set,
	enum isl_dim_type dst_type, unsigned dst_pos,
	enum isl_dim_type src_type, unsigned src_pos, unsigned n)
{
	if (!set)
		return NULL;
	isl_assert(set->ctx, dst_type != isl_dim_in, goto error);
	return (isl_set *)isl_map_move_dims((isl_map *)set, dst_type, dst_pos,
					src_type, src_pos, n);
error:
	isl_set_free(set);
	return NULL;
}

__isl_give isl_map *isl_map_move_dims(__isl_take isl_map *map,
	enum isl_dim_type dst_type, unsigned dst_pos,
	enum isl_dim_type src_type, unsigned src_pos, unsigned n)
{
	int i;

	if (!map)
		return NULL;
	if (n == 0)
		return map;

	isl_assert(map->ctx, src_pos + n <= isl_map_dim(map, src_type),
		goto error);

	if (dst_type == src_type && dst_pos == src_pos)
		return map;

	isl_assert(map->ctx, dst_type != src_type, goto error);

	map = isl_map_cow(map);
	if (!map)
		return NULL;

	map->dim = isl_dim_move(map->dim, dst_type, dst_pos, src_type, src_pos, n);
	if (!map->dim)
		goto error;

	for (i = 0; i < map->n; ++i) {
		map->p[i] = isl_basic_map_move_dims(map->p[i],
						dst_type, dst_pos,
						src_type, src_pos, n);
		if (!map->p[i])
			goto error;
	}

	return map;
error:
	isl_map_free(map);
	return NULL;
}

/* Move the specified dimensions to the last columns right before
 * the divs.  Don't change the dimension specification of bmap.
 * That's the responsibility of the caller.
 */
static __isl_give isl_basic_map *move_last(__isl_take isl_basic_map *bmap,
	enum isl_dim_type type, unsigned first, unsigned n)
{
	struct isl_dim_map *dim_map;
	struct isl_basic_map *res;
	enum isl_dim_type t;
	unsigned total, off;

	if (!bmap)
		return NULL;
	if (pos(bmap->dim, type) + first + n == 1 + isl_dim_total(bmap->dim))
		return bmap;

	total = isl_basic_map_total_dim(bmap);
	dim_map = isl_dim_map_alloc(bmap->ctx, total);

	off = 0;
	for (t = isl_dim_param; t <= isl_dim_out; ++t) {
		unsigned size = isl_dim_size(bmap->dim, t);
		if (t == type) {
			isl_dim_map_dim_range(dim_map, bmap->dim, t,
					    0, first, off);
			off += first;
			isl_dim_map_dim_range(dim_map, bmap->dim, t,
					    first, n, total - bmap->n_div - n);
			isl_dim_map_dim_range(dim_map, bmap->dim, t,
					    first + n, size - (first + n), off);
			off += size - (first + n);
		} else {
			isl_dim_map_dim(dim_map, bmap->dim, t, off);
			off += size;
		}
	}
	isl_dim_map_div(dim_map, bmap, off + n);

	res = isl_basic_map_alloc_dim(isl_basic_map_get_dim(bmap),
			bmap->n_div, bmap->n_eq, bmap->n_ineq);
	res = add_constraints_dim_map(res, bmap, dim_map);
	return res;
}

/* Turn the n dimensions of type type, starting at first
 * into existentially quantified variables.
 */
__isl_give isl_basic_map *isl_basic_map_project_out(
		__isl_take isl_basic_map *bmap,
		enum isl_dim_type type, unsigned first, unsigned n)
{
	int i;
	size_t row_size;
	isl_int **new_div;
	isl_int *old;

	if (n == 0)
		return bmap;

	if (!bmap)
		return NULL;

	if (ISL_F_ISSET(bmap, ISL_BASIC_MAP_RATIONAL))
		return isl_basic_map_remove(bmap, type, first, n);

	isl_assert(bmap->ctx, first + n <= isl_basic_map_dim(bmap, type),
			goto error);

	bmap = move_last(bmap, type, first, n);
	bmap = isl_basic_map_cow(bmap);
	if (!bmap)
		return NULL;

	row_size = 1 + isl_dim_total(bmap->dim) + bmap->extra;
	old = bmap->block2.data;
	bmap->block2 = isl_blk_extend(bmap->ctx, bmap->block2,
					(bmap->extra + n) * (1 + row_size));
	if (!bmap->block2.data)
		goto error;
	new_div = isl_alloc_array(bmap->ctx, isl_int *, bmap->extra + n);
	if (!new_div)
		goto error;
	for (i = 0; i < n; ++i) {
		new_div[i] = bmap->block2.data +
				(bmap->extra + i) * (1 + row_size);
		isl_seq_clr(new_div[i], 1 + row_size);
	}
	for (i = 0; i < bmap->extra; ++i)
		new_div[n + i] = bmap->block2.data + (bmap->div[i] - old);
	free(bmap->div);
	bmap->div = new_div;
	bmap->n_div += n;
	bmap->extra += n;

	bmap->dim = isl_dim_drop(bmap->dim, type, first, n);
	if (!bmap->dim)
		goto error;
	bmap = isl_basic_map_simplify(bmap);
	bmap = isl_basic_map_drop_redundant_divs(bmap);
	return isl_basic_map_finalize(bmap);
error:
	isl_basic_map_free(bmap);
	return NULL;
}

/* Turn the n dimensions of type type, starting at first
 * into existentially quantified variables.
 */
struct isl_basic_set *isl_basic_set_project_out(struct isl_basic_set *bset,
		enum isl_dim_type type, unsigned first, unsigned n)
{
	return (isl_basic_set *)isl_basic_map_project_out(
			(isl_basic_map *)bset, type, first, n);
}

/* Turn the n dimensions of type type, starting at first
 * into existentially quantified variables.
 */
__isl_give isl_map *isl_map_project_out(__isl_take isl_map *map,
		enum isl_dim_type type, unsigned first, unsigned n)
{
	int i;

	if (!map)
		return NULL;

	if (n == 0)
		return map;

	isl_assert(map->ctx, first + n <= isl_map_dim(map, type), goto error);

	map = isl_map_cow(map);
	if (!map)
		return NULL;

	map->dim = isl_dim_drop(map->dim, type, first, n);
	if (!map->dim)
		goto error;

	for (i = 0; i < map->n; ++i) {
		map->p[i] = isl_basic_map_project_out(map->p[i], type, first, n);
		if (!map->p[i])
			goto error;
	}

	return map;
error:
	isl_map_free(map);
	return NULL;
}

/* Turn the n dimensions of type type, starting at first
 * into existentially quantified variables.
 */
__isl_give isl_set *isl_set_project_out(__isl_take isl_set *set,
		enum isl_dim_type type, unsigned first, unsigned n)
{
	return (isl_set *)isl_map_project_out((isl_map *)set, type, first, n);
}

static struct isl_basic_map *add_divs(struct isl_basic_map *bmap, unsigned n)
{
	int i, j;

	for (i = 0; i < n; ++i) {
		j = isl_basic_map_alloc_div(bmap);
		if (j < 0)
			goto error;
		isl_seq_clr(bmap->div[j], 1+1+isl_basic_map_total_dim(bmap));
	}
	return bmap;
error:
	isl_basic_map_free(bmap);
	return NULL;
}

struct isl_basic_map *isl_basic_map_apply_range(
		struct isl_basic_map *bmap1, struct isl_basic_map *bmap2)
{
	struct isl_dim *dim_result = NULL;
	struct isl_basic_map *bmap;
	unsigned n_in, n_out, n, nparam, total, pos;
	struct isl_dim_map *dim_map1, *dim_map2;

	if (!bmap1 || !bmap2)
		goto error;

	dim_result = isl_dim_join(isl_dim_copy(bmap1->dim),
				  isl_dim_copy(bmap2->dim));

	n_in = isl_basic_map_n_in(bmap1);
	n_out = isl_basic_map_n_out(bmap2);
	n = isl_basic_map_n_out(bmap1);
	nparam = isl_basic_map_n_param(bmap1);

	total = nparam + n_in + n_out + bmap1->n_div + bmap2->n_div + n;
	dim_map1 = isl_dim_map_alloc(bmap1->ctx, total);
	dim_map2 = isl_dim_map_alloc(bmap1->ctx, total);
	isl_dim_map_dim(dim_map1, bmap1->dim, isl_dim_param, pos = 0);
	isl_dim_map_dim(dim_map2, bmap2->dim, isl_dim_param, pos = 0);
	isl_dim_map_dim(dim_map1, bmap1->dim, isl_dim_in, pos += nparam);
	isl_dim_map_dim(dim_map2, bmap2->dim, isl_dim_out, pos += n_in);
	isl_dim_map_div(dim_map1, bmap1, pos += n_out);
	isl_dim_map_div(dim_map2, bmap2, pos += bmap1->n_div);
	isl_dim_map_dim(dim_map1, bmap1->dim, isl_dim_out, pos += bmap2->n_div);
	isl_dim_map_dim(dim_map2, bmap2->dim, isl_dim_in, pos);

	bmap = isl_basic_map_alloc_dim(dim_result,
			bmap1->n_div + bmap2->n_div + n,
			bmap1->n_eq + bmap2->n_eq,
			bmap1->n_ineq + bmap2->n_ineq);
	bmap = add_constraints_dim_map(bmap, bmap1, dim_map1);
	bmap = add_constraints_dim_map(bmap, bmap2, dim_map2);
	bmap = add_divs(bmap, n);
	bmap = isl_basic_map_simplify(bmap);
	bmap = isl_basic_map_drop_redundant_divs(bmap);
	return isl_basic_map_finalize(bmap);
error:
	isl_basic_map_free(bmap1);
	isl_basic_map_free(bmap2);
	return NULL;
}

struct isl_basic_set *isl_basic_set_apply(
		struct isl_basic_set *bset, struct isl_basic_map *bmap)
{
	if (!bset || !bmap)
		goto error;

	isl_assert(bset->ctx, isl_basic_map_compatible_domain(bmap, bset),
		    goto error);

	return (struct isl_basic_set *)
		isl_basic_map_apply_range((struct isl_basic_map *)bset, bmap);
error:
	isl_basic_set_free(bset);
	isl_basic_map_free(bmap);
	return NULL;
}

struct isl_basic_map *isl_basic_map_apply_domain(
		struct isl_basic_map *bmap1, struct isl_basic_map *bmap2)
{
	if (!bmap1 || !bmap2)
		goto error;

	isl_assert(bmap1->ctx,
	    isl_basic_map_n_in(bmap1) == isl_basic_map_n_in(bmap2), goto error);
	isl_assert(bmap1->ctx,
	    isl_basic_map_n_param(bmap1) == isl_basic_map_n_param(bmap2),
	    goto error);

	bmap1 = isl_basic_map_reverse(bmap1);
	bmap1 = isl_basic_map_apply_range(bmap1, bmap2);
	return isl_basic_map_reverse(bmap1);
error:
	isl_basic_map_free(bmap1);
	isl_basic_map_free(bmap2);
	return NULL;
}

/* Given two basic maps A -> f(A) and B -> g(B), construct a basic map
 * A \cap B -> f(A) + f(B)
 */
struct isl_basic_map *isl_basic_map_sum(
		struct isl_basic_map *bmap1, struct isl_basic_map *bmap2)
{
	unsigned n_in, n_out, nparam, total, pos;
	struct isl_basic_map *bmap = NULL;
	struct isl_dim_map *dim_map1, *dim_map2;
	int i;

	if (!bmap1 || !bmap2)
		goto error;

	isl_assert(bmap1->ctx, isl_dim_equal(bmap1->dim, bmap2->dim),
		goto error);

	nparam = isl_basic_map_n_param(bmap1);
	n_in = isl_basic_map_n_in(bmap1);
	n_out = isl_basic_map_n_out(bmap1);

	total = nparam + n_in + n_out + bmap1->n_div + bmap2->n_div + 2 * n_out;
	dim_map1 = isl_dim_map_alloc(bmap1->ctx, total);
	dim_map2 = isl_dim_map_alloc(bmap2->ctx, total);
	isl_dim_map_dim(dim_map1, bmap1->dim, isl_dim_param, pos = 0);
	isl_dim_map_dim(dim_map2, bmap2->dim, isl_dim_param, pos);
	isl_dim_map_dim(dim_map1, bmap1->dim, isl_dim_in, pos += nparam);
	isl_dim_map_dim(dim_map2, bmap2->dim, isl_dim_in, pos);
	isl_dim_map_div(dim_map1, bmap1, pos += n_in + n_out);
	isl_dim_map_div(dim_map2, bmap2, pos += bmap1->n_div);
	isl_dim_map_dim(dim_map1, bmap1->dim, isl_dim_out, pos += bmap2->n_div);
	isl_dim_map_dim(dim_map2, bmap2->dim, isl_dim_out, pos += n_out);

	bmap = isl_basic_map_alloc_dim(isl_dim_copy(bmap1->dim),
			bmap1->n_div + bmap2->n_div + 2 * n_out,
			bmap1->n_eq + bmap2->n_eq + n_out,
			bmap1->n_ineq + bmap2->n_ineq);
	for (i = 0; i < n_out; ++i) {
		int j = isl_basic_map_alloc_equality(bmap);
		if (j < 0)
			goto error;
		isl_seq_clr(bmap->eq[j], 1+total);
		isl_int_set_si(bmap->eq[j][1+nparam+n_in+i], -1);
		isl_int_set_si(bmap->eq[j][1+pos+i], 1);
		isl_int_set_si(bmap->eq[j][1+pos-n_out+i], 1);
	}
	bmap = add_constraints_dim_map(bmap, bmap1, dim_map1);
	bmap = add_constraints_dim_map(bmap, bmap2, dim_map2);
	bmap = add_divs(bmap, 2 * n_out);

	bmap = isl_basic_map_simplify(bmap);
	return isl_basic_map_finalize(bmap);
error:
	isl_basic_map_free(bmap);
	isl_basic_map_free(bmap1);
	isl_basic_map_free(bmap2);
	return NULL;
}

/* Given two maps A -> f(A) and B -> g(B), construct a map
 * A \cap B -> f(A) + f(B)
 */
struct isl_map *isl_map_sum(struct isl_map *map1, struct isl_map *map2)
{
	struct isl_map *result;
	int i, j;

	if (!map1 || !map2)
		goto error;

	isl_assert(map1->ctx, isl_dim_equal(map1->dim, map2->dim), goto error);

	result = isl_map_alloc_dim(isl_dim_copy(map1->dim),
				map1->n * map2->n, 0);
	if (!result)
		goto error;
	for (i = 0; i < map1->n; ++i)
		for (j = 0; j < map2->n; ++j) {
			struct isl_basic_map *part;
			part = isl_basic_map_sum(
				    isl_basic_map_copy(map1->p[i]),
				    isl_basic_map_copy(map2->p[j]));
			if (isl_basic_map_is_empty(part))
				isl_basic_map_free(part);
			else
				result = isl_map_add_basic_map(result, part);
			if (!result)
				goto error;
		}
	isl_map_free(map1);
	isl_map_free(map2);
	return result;
error:
	isl_map_free(map1);
	isl_map_free(map2);
	return NULL;
}

__isl_give isl_set *isl_set_sum(__isl_take isl_set *set1,
	__isl_take isl_set *set2)
{
	return (isl_set *)isl_map_sum((isl_map *)set1, (isl_map *)set2);
}

/* Given a basic map A -> f(A), construct A -> -f(A).
 */
struct isl_basic_map *isl_basic_map_neg(struct isl_basic_map *bmap)
{
	int i, j;
	unsigned off, n;

	bmap = isl_basic_map_cow(bmap);
	if (!bmap)
		return NULL;

	n = isl_basic_map_dim(bmap, isl_dim_out);
	off = isl_basic_map_offset(bmap, isl_dim_out);
	for (i = 0; i < bmap->n_eq; ++i)
		for (j = 0; j < n; ++j)
			isl_int_neg(bmap->eq[i][off+j], bmap->eq[i][off+j]);
	for (i = 0; i < bmap->n_ineq; ++i)
		for (j = 0; j < n; ++j)
			isl_int_neg(bmap->ineq[i][off+j], bmap->ineq[i][off+j]);
	for (i = 0; i < bmap->n_div; ++i)
		for (j = 0; j < n; ++j)
			isl_int_neg(bmap->div[i][1+off+j], bmap->div[i][1+off+j]);
	return isl_basic_map_finalize(bmap);
}

/* Given a map A -> f(A), construct A -> -f(A).
 */
struct isl_map *isl_map_neg(struct isl_map *map)
{
	int i;

	map = isl_map_cow(map);
	if (!map)
		return NULL;

	for (i = 0; i < map->n; ++i) {
		map->p[i] = isl_basic_map_neg(map->p[i]);
		if (!map->p[i])
			goto error;
	}

	return map;
error:
	isl_map_free(map);
	return NULL;
}

__isl_give isl_set *isl_set_neg(__isl_take isl_set *set)
{
	return (isl_set *)isl_map_neg((isl_map *)set);
}

/* Given a basic map A -> f(A) and an integer d, construct a basic map
 * A -> floor(f(A)/d).
 */
struct isl_basic_map *isl_basic_map_floordiv(struct isl_basic_map *bmap,
		isl_int d)
{
	unsigned n_in, n_out, nparam, total, pos;
	struct isl_basic_map *result = NULL;
	struct isl_dim_map *dim_map;
	int i;

	if (!bmap)
		return NULL;

	nparam = isl_basic_map_n_param(bmap);
	n_in = isl_basic_map_n_in(bmap);
	n_out = isl_basic_map_n_out(bmap);

	total = nparam + n_in + n_out + bmap->n_div + n_out;
	dim_map = isl_dim_map_alloc(bmap->ctx, total);
	isl_dim_map_dim(dim_map, bmap->dim, isl_dim_param, pos = 0);
	isl_dim_map_dim(dim_map, bmap->dim, isl_dim_in, pos += nparam);
	isl_dim_map_div(dim_map, bmap, pos += n_in + n_out);
	isl_dim_map_dim(dim_map, bmap->dim, isl_dim_out, pos += bmap->n_div);

	result = isl_basic_map_alloc_dim(isl_dim_copy(bmap->dim),
			bmap->n_div + n_out,
			bmap->n_eq, bmap->n_ineq + 2 * n_out);
	result = add_constraints_dim_map(result, bmap, dim_map);
	result = add_divs(result, n_out);
	for (i = 0; i < n_out; ++i) {
		int j;
		j = isl_basic_map_alloc_inequality(result);
		if (j < 0)
			goto error;
		isl_seq_clr(result->ineq[j], 1+total);
		isl_int_neg(result->ineq[j][1+nparam+n_in+i], d);
		isl_int_set_si(result->ineq[j][1+pos+i], 1);
		j = isl_basic_map_alloc_inequality(result);
		if (j < 0)
			goto error;
		isl_seq_clr(result->ineq[j], 1+total);
		isl_int_set(result->ineq[j][1+nparam+n_in+i], d);
		isl_int_set_si(result->ineq[j][1+pos+i], -1);
		isl_int_sub_ui(result->ineq[j][0], d, 1);
	}

	result = isl_basic_map_simplify(result);
	return isl_basic_map_finalize(result);
error:
	isl_basic_map_free(result);
	return NULL;
}

/* Given a map A -> f(A) and an integer d, construct a map
 * A -> floor(f(A)/d).
 */
struct isl_map *isl_map_floordiv(struct isl_map *map, isl_int d)
{
	int i;

	map = isl_map_cow(map);
	if (!map)
		return NULL;

	ISL_F_CLR(map, ISL_MAP_DISJOINT);
	ISL_F_CLR(map, ISL_MAP_NORMALIZED);
	for (i = 0; i < map->n; ++i) {
		map->p[i] = isl_basic_map_floordiv(map->p[i], d);
		if (!map->p[i])
			goto error;
	}

	return map;
error:
	isl_map_free(map);
	return NULL;
}

static struct isl_basic_map *var_equal(struct isl_basic_map *bmap, unsigned pos)
{
	int i;
	unsigned nparam;
	unsigned n_in;

	i = isl_basic_map_alloc_equality(bmap);
	if (i < 0)
		goto error;
	nparam = isl_basic_map_n_param(bmap);
	n_in = isl_basic_map_n_in(bmap);
	isl_seq_clr(bmap->eq[i], 1 + isl_basic_map_total_dim(bmap));
	isl_int_set_si(bmap->eq[i][1+nparam+pos], -1);
	isl_int_set_si(bmap->eq[i][1+nparam+n_in+pos], 1);
	return isl_basic_map_finalize(bmap);
error:
	isl_basic_map_free(bmap);
	return NULL;
}

/* Add a constraints to "bmap" expressing i_pos < o_pos
 */
static struct isl_basic_map *var_less(struct isl_basic_map *bmap, unsigned pos)
{
	int i;
	unsigned nparam;
	unsigned n_in;

	i = isl_basic_map_alloc_inequality(bmap);
	if (i < 0)
		goto error;
	nparam = isl_basic_map_n_param(bmap);
	n_in = isl_basic_map_n_in(bmap);
	isl_seq_clr(bmap->ineq[i], 1 + isl_basic_map_total_dim(bmap));
	isl_int_set_si(bmap->ineq[i][0], -1);
	isl_int_set_si(bmap->ineq[i][1+nparam+pos], -1);
	isl_int_set_si(bmap->ineq[i][1+nparam+n_in+pos], 1);
	return isl_basic_map_finalize(bmap);
error:
	isl_basic_map_free(bmap);
	return NULL;
}

/* Add a constraint to "bmap" expressing i_pos <= o_pos
 */
static __isl_give isl_basic_map *var_less_or_equal(
	__isl_take isl_basic_map *bmap, unsigned pos)
{
	int i;
	unsigned nparam;
	unsigned n_in;

	i = isl_basic_map_alloc_inequality(bmap);
	if (i < 0)
		goto error;
	nparam = isl_basic_map_n_param(bmap);
	n_in = isl_basic_map_n_in(bmap);
	isl_seq_clr(bmap->ineq[i], 1 + isl_basic_map_total_dim(bmap));
	isl_int_set_si(bmap->ineq[i][1+nparam+pos], -1);
	isl_int_set_si(bmap->ineq[i][1+nparam+n_in+pos], 1);
	return isl_basic_map_finalize(bmap);
error:
	isl_basic_map_free(bmap);
	return NULL;
}

/* Add a constraints to "bmap" expressing i_pos > o_pos
 */
static struct isl_basic_map *var_more(struct isl_basic_map *bmap, unsigned pos)
{
	int i;
	unsigned nparam;
	unsigned n_in;

	i = isl_basic_map_alloc_inequality(bmap);
	if (i < 0)
		goto error;
	nparam = isl_basic_map_n_param(bmap);
	n_in = isl_basic_map_n_in(bmap);
	isl_seq_clr(bmap->ineq[i], 1 + isl_basic_map_total_dim(bmap));
	isl_int_set_si(bmap->ineq[i][0], -1);
	isl_int_set_si(bmap->ineq[i][1+nparam+pos], 1);
	isl_int_set_si(bmap->ineq[i][1+nparam+n_in+pos], -1);
	return isl_basic_map_finalize(bmap);
error:
	isl_basic_map_free(bmap);
	return NULL;
}

/* Add a constraint to "bmap" expressing i_pos >= o_pos
 */
static __isl_give isl_basic_map *var_more_or_equal(
	__isl_take isl_basic_map *bmap, unsigned pos)
{
	int i;
	unsigned nparam;
	unsigned n_in;

	i = isl_basic_map_alloc_inequality(bmap);
	if (i < 0)
		goto error;
	nparam = isl_basic_map_n_param(bmap);
	n_in = isl_basic_map_n_in(bmap);
	isl_seq_clr(bmap->ineq[i], 1 + isl_basic_map_total_dim(bmap));
	isl_int_set_si(bmap->ineq[i][1+nparam+pos], 1);
	isl_int_set_si(bmap->ineq[i][1+nparam+n_in+pos], -1);
	return isl_basic_map_finalize(bmap);
error:
	isl_basic_map_free(bmap);
	return NULL;
}

struct isl_basic_map *isl_basic_map_equal(struct isl_dim *dim, unsigned n_equal)
{
	int i;
	struct isl_basic_map *bmap;
	bmap = isl_basic_map_alloc_dim(dim, 0, n_equal, 0);
	if (!bmap)
		return NULL;
	for (i = 0; i < n_equal && bmap; ++i)
		bmap = var_equal(bmap, i);
	return isl_basic_map_finalize(bmap);
}

/* Return a relation on of dimension "dim" expressing i_[0..pos] << o_[0..pos]
 */
struct isl_basic_map *isl_basic_map_less_at(struct isl_dim *dim, unsigned pos)
{
	int i;
	struct isl_basic_map *bmap;
	bmap = isl_basic_map_alloc_dim(dim, 0, pos, 1);
	if (!bmap)
		return NULL;
	for (i = 0; i < pos && bmap; ++i)
		bmap = var_equal(bmap, i);
	if (bmap)
		bmap = var_less(bmap, pos);
	return isl_basic_map_finalize(bmap);
}

/* Return a relation on of dimension "dim" expressing i_[0..pos] <<= o_[0..pos]
 */
__isl_give isl_basic_map *isl_basic_map_less_or_equal_at(
	__isl_take isl_dim *dim, unsigned pos)
{
	int i;
	isl_basic_map *bmap;

	bmap = isl_basic_map_alloc_dim(dim, 0, pos, 1);
	for (i = 0; i < pos; ++i)
		bmap = var_equal(bmap, i);
	bmap = var_less_or_equal(bmap, pos);
	return isl_basic_map_finalize(bmap);
}

/* Return a relation on pairs of sets of dimension "dim" expressing i_pos > o_pos
 */
struct isl_basic_map *isl_basic_map_more_at(struct isl_dim *dim, unsigned pos)
{
	int i;
	struct isl_basic_map *bmap;
	bmap = isl_basic_map_alloc_dim(dim, 0, pos, 1);
	if (!bmap)
		return NULL;
	for (i = 0; i < pos && bmap; ++i)
		bmap = var_equal(bmap, i);
	if (bmap)
		bmap = var_more(bmap, pos);
	return isl_basic_map_finalize(bmap);
}

/* Return a relation on of dimension "dim" expressing i_[0..pos] >>= o_[0..pos]
 */
__isl_give isl_basic_map *isl_basic_map_more_or_equal_at(
	__isl_take isl_dim *dim, unsigned pos)
{
	int i;
	isl_basic_map *bmap;

	bmap = isl_basic_map_alloc_dim(dim, 0, pos, 1);
	for (i = 0; i < pos; ++i)
		bmap = var_equal(bmap, i);
	bmap = var_more_or_equal(bmap, pos);
	return isl_basic_map_finalize(bmap);
}

static __isl_give isl_map *map_lex_lte_first(__isl_take isl_dim *dims,
	unsigned n, int equal)
{
	struct isl_map *map;
	int i;

	map = isl_map_alloc_dim(isl_dim_copy(dims), n, ISL_MAP_DISJOINT);

	for (i = 0; i + 1 < n; ++i)
		map = isl_map_add_basic_map(map,
				  isl_basic_map_less_at(isl_dim_copy(dims), i));
	if (n > 0) {
		if (equal)
			map = isl_map_add_basic_map(map,
			      isl_basic_map_less_or_equal_at(dims, n - 1));
		else
			map = isl_map_add_basic_map(map,
			      isl_basic_map_less_at(dims, n - 1));
	} else
		isl_dim_free(dims);

	return map;
}

static __isl_give isl_map *map_lex_lte(__isl_take isl_dim *dims, int equal)
{
	if (!dims)
		return NULL;
	return map_lex_lte_first(dims, dims->n_out, equal);
}

__isl_give isl_map *isl_map_lex_lt_first(__isl_take isl_dim *dim, unsigned n)
{
	return map_lex_lte_first(dim, n, 0);
}

__isl_give isl_map *isl_map_lex_le_first(__isl_take isl_dim *dim, unsigned n)
{
	return map_lex_lte_first(dim, n, 1);
}

__isl_give isl_map *isl_map_lex_lt(__isl_take isl_dim *set_dim)
{
	return map_lex_lte(isl_dim_map(set_dim), 0);
}

__isl_give isl_map *isl_map_lex_le(__isl_take isl_dim *set_dim)
{
	return map_lex_lte(isl_dim_map(set_dim), 1);
}

static __isl_give isl_map *map_lex_gte_first(__isl_take isl_dim *dims,
	unsigned n, int equal)
{
	struct isl_map *map;
	int i;

	map = isl_map_alloc_dim(isl_dim_copy(dims), n, ISL_MAP_DISJOINT);

	for (i = 0; i + 1 < n; ++i)
		map = isl_map_add_basic_map(map,
				  isl_basic_map_more_at(isl_dim_copy(dims), i));
	if (n > 0) {
		if (equal)
			map = isl_map_add_basic_map(map,
			      isl_basic_map_more_or_equal_at(dims, n - 1));
		else
			map = isl_map_add_basic_map(map,
			      isl_basic_map_more_at(dims, n - 1));
	} else
		isl_dim_free(dims);

	return map;
}

static __isl_give isl_map *map_lex_gte(__isl_take isl_dim *dims, int equal)
{
	if (!dims)
		return NULL;
	return map_lex_gte_first(dims, dims->n_out, equal);
}

__isl_give isl_map *isl_map_lex_gt_first(__isl_take isl_dim *dim, unsigned n)
{
	return map_lex_gte_first(dim, n, 0);
}

__isl_give isl_map *isl_map_lex_ge_first(__isl_take isl_dim *dim, unsigned n)
{
	return map_lex_gte_first(dim, n, 1);
}

__isl_give isl_map *isl_map_lex_gt(__isl_take isl_dim *set_dim)
{
	return map_lex_gte(isl_dim_map(set_dim), 0);
}

__isl_give isl_map *isl_map_lex_ge(__isl_take isl_dim *set_dim)
{
	return map_lex_gte(isl_dim_map(set_dim), 1);
}

struct isl_basic_map *isl_basic_map_from_basic_set(
		struct isl_basic_set *bset, struct isl_dim *dim)
{
	struct isl_basic_map *bmap;

	bset = isl_basic_set_cow(bset);
	if (!bset || !dim)
		goto error;

	isl_assert(bset->ctx, isl_dim_compatible(bset->dim, dim), goto error);
	isl_dim_free(bset->dim);
	bmap = (struct isl_basic_map *) bset;
	bmap->dim = dim;
	return isl_basic_map_finalize(bmap);
error:
	isl_basic_set_free(bset);
	isl_dim_free(dim);
	return NULL;
}

struct isl_basic_set *isl_basic_set_from_basic_map(struct isl_basic_map *bmap)
{
	if (!bmap)
		goto error;
	if (bmap->dim->n_in == 0)
		return (struct isl_basic_set *)bmap;
	bmap = isl_basic_map_cow(bmap);
	if (!bmap)
		goto error;
	bmap->dim = isl_dim_cow(bmap->dim);
	if (!bmap->dim)
		goto error;
	bmap->dim->n_out += bmap->dim->n_in;
	bmap->dim->n_in = 0;
	bmap = isl_basic_map_finalize(bmap);
	return (struct isl_basic_set *)bmap;
error:
	isl_basic_map_free(bmap);
	return NULL;
}

/* For a div d = floor(f/m), add the constraints
 *
 *		f - m d >= 0
 *		-(f-(n-1)) + m d >= 0
 *
 * Note that the second constraint is the negation of
 *
 *		f - m d >= n
 */
int isl_basic_map_add_div_constraints_var(__isl_keep isl_basic_map *bmap,
	unsigned pos, isl_int *div)
{
	int i, j;
	unsigned total = isl_basic_map_total_dim(bmap);

	i = isl_basic_map_alloc_inequality(bmap);
	if (i < 0)
		return -1;
	isl_seq_cpy(bmap->ineq[i], div + 1, 1 + total);
	isl_int_neg(bmap->ineq[i][1 + pos], div[0]);

	j = isl_basic_map_alloc_inequality(bmap);
	if (j < 0)
		return -1;
	isl_seq_neg(bmap->ineq[j], bmap->ineq[i], 1 + total);
	isl_int_add(bmap->ineq[j][0], bmap->ineq[j][0], bmap->ineq[j][1 + pos]);
	isl_int_sub_ui(bmap->ineq[j][0], bmap->ineq[j][0], 1);
	return j;
}

int isl_basic_set_add_div_constraints_var(__isl_keep isl_basic_set *bset,
	unsigned pos, isl_int *div)
{
	return isl_basic_map_add_div_constraints_var((isl_basic_map *)bset,
							pos, div);
}

int isl_basic_map_add_div_constraints(struct isl_basic_map *bmap, unsigned div)
{
	unsigned total = isl_basic_map_total_dim(bmap);
	unsigned div_pos = total - bmap->n_div + div;

	return isl_basic_map_add_div_constraints_var(bmap, div_pos,
							bmap->div[div]);
}

struct isl_basic_set *isl_basic_map_underlying_set(
		struct isl_basic_map *bmap)
{
	if (!bmap)
		goto error;
	if (bmap->dim->nparam == 0 && bmap->dim->n_in == 0 && bmap->n_div == 0)
		return (struct isl_basic_set *)bmap;
	bmap = isl_basic_map_cow(bmap);
	if (!bmap)
		goto error;
	bmap->dim = isl_dim_underlying(bmap->dim, bmap->n_div);
	if (!bmap->dim)
		goto error;
	bmap->extra -= bmap->n_div;
	bmap->n_div = 0;
	bmap = isl_basic_map_finalize(bmap);
	return (struct isl_basic_set *)bmap;
error:
	return NULL;
}

__isl_give isl_basic_set *isl_basic_set_underlying_set(
		__isl_take isl_basic_set *bset)
{
	return isl_basic_map_underlying_set((isl_basic_map *)bset);
}

struct isl_basic_map *isl_basic_map_overlying_set(
	struct isl_basic_set *bset, struct isl_basic_map *like)
{
	struct isl_basic_map *bmap;
	struct isl_ctx *ctx;
	unsigned total;
	int i;

	if (!bset || !like)
		goto error;
	ctx = bset->ctx;
	isl_assert(ctx, bset->n_div == 0, goto error);
	isl_assert(ctx, isl_basic_set_n_param(bset) == 0, goto error);
	isl_assert(ctx, bset->dim->n_out == isl_basic_map_total_dim(like),
			goto error);
	if (isl_dim_equal(bset->dim, like->dim) && like->n_div == 0) {
		isl_basic_map_free(like);
		return (struct isl_basic_map *)bset;
	}
	bset = isl_basic_set_cow(bset);
	if (!bset)
		goto error;
	total = bset->dim->n_out + bset->extra;
	bmap = (struct isl_basic_map *)bset;
	isl_dim_free(bmap->dim);
	bmap->dim = isl_dim_copy(like->dim);
	if (!bmap->dim)
		goto error;
	bmap->n_div = like->n_div;
	bmap->extra += like->n_div;
	if (bmap->extra) {
		unsigned ltotal;
		ltotal = total - bmap->extra + like->extra;
		if (ltotal > total)
			ltotal = total;
		bmap->block2 = isl_blk_extend(ctx, bmap->block2,
					bmap->extra * (1 + 1 + total));
		if (isl_blk_is_error(bmap->block2))
			goto error;
		bmap->div = isl_realloc_array(ctx, bmap->div, isl_int *,
						bmap->extra);
		if (!bmap->div)
			goto error;
		for (i = 0; i < bmap->extra; ++i)
			bmap->div[i] = bmap->block2.data + i * (1 + 1 + total);
		for (i = 0; i < like->n_div; ++i) {
			isl_seq_cpy(bmap->div[i], like->div[i], 1 + 1 + ltotal);
			isl_seq_clr(bmap->div[i]+1+1+ltotal, total - ltotal);
		}
		bmap = isl_basic_map_extend_constraints(bmap, 
							0, 2 * like->n_div);
		for (i = 0; i < like->n_div; ++i) {
			if (isl_int_is_zero(bmap->div[i][0]))
				continue;
			if (isl_basic_map_add_div_constraints(bmap, i) < 0)
				goto error;
		}
	}
	isl_basic_map_free(like);
	bmap = isl_basic_map_simplify(bmap);
	bmap = isl_basic_map_finalize(bmap);
	return bmap;
error:
	isl_basic_map_free(like);
	isl_basic_set_free(bset);
	return NULL;
}

struct isl_basic_set *isl_basic_set_from_underlying_set(
	struct isl_basic_set *bset, struct isl_basic_set *like)
{
	return (struct isl_basic_set *)
		isl_basic_map_overlying_set(bset, (struct isl_basic_map *)like);
}

struct isl_set *isl_set_from_underlying_set(
	struct isl_set *set, struct isl_basic_set *like)
{
	int i;

	if (!set || !like)
		goto error;
	isl_assert(set->ctx, set->dim->n_out == isl_basic_set_total_dim(like),
		    goto error);
	if (isl_dim_equal(set->dim, like->dim) && like->n_div == 0) {
		isl_basic_set_free(like);
		return set;
	}
	set = isl_set_cow(set);
	if (!set)
		goto error;
	for (i = 0; i < set->n; ++i) {
		set->p[i] = isl_basic_set_from_underlying_set(set->p[i],
						      isl_basic_set_copy(like));
		if (!set->p[i])
			goto error;
	}
	isl_dim_free(set->dim);
	set->dim = isl_dim_copy(like->dim);
	if (!set->dim)
		goto error;
	isl_basic_set_free(like);
	return set;
error:
	isl_basic_set_free(like);
	isl_set_free(set);
	return NULL;
}

struct isl_set *isl_map_underlying_set(struct isl_map *map)
{
	int i;

	map = isl_map_cow(map);
	if (!map)
		return NULL;
	map->dim = isl_dim_cow(map->dim);
	if (!map->dim)
		goto error;

	for (i = 1; i < map->n; ++i)
		isl_assert(map->ctx, map->p[0]->n_div == map->p[i]->n_div,
				goto error);
	for (i = 0; i < map->n; ++i) {
		map->p[i] = (struct isl_basic_map *)
				isl_basic_map_underlying_set(map->p[i]);
		if (!map->p[i])
			goto error;
	}
	if (map->n == 0)
		map->dim = isl_dim_underlying(map->dim, 0);
	else {
		isl_dim_free(map->dim);
		map->dim = isl_dim_copy(map->p[0]->dim);
	}
	if (!map->dim)
		goto error;
	return (struct isl_set *)map;
error:
	isl_map_free(map);
	return NULL;
}

struct isl_set *isl_set_to_underlying_set(struct isl_set *set)
{
	return (struct isl_set *)isl_map_underlying_set((struct isl_map *)set);
}

struct isl_basic_set *isl_basic_map_domain(struct isl_basic_map *bmap)
{
	struct isl_basic_set *domain;
	unsigned n_in;
	unsigned n_out;
	if (!bmap)
		return NULL;
	n_in = isl_basic_map_n_in(bmap);
	n_out = isl_basic_map_n_out(bmap);
	domain = isl_basic_set_from_basic_map(bmap);
	return isl_basic_set_project_out(domain, isl_dim_set, n_in, n_out);
}

struct isl_basic_set *isl_basic_map_range(struct isl_basic_map *bmap)
{
	return isl_basic_map_domain(isl_basic_map_reverse(bmap));
}

struct isl_set *isl_map_range(struct isl_map *map)
{
	int i;
	struct isl_set *set;

	if (!map)
		goto error;
	if (isl_map_dim(map, isl_dim_in) == 0)
		return (isl_set *)map;

	map = isl_map_cow(map);
	if (!map)
		goto error;

	set = (struct isl_set *) map;
	set->dim = isl_dim_drop_inputs(set->dim, 0, set->dim->n_in);
	if (!set->dim)
		goto error;
	for (i = 0; i < map->n; ++i) {
		set->p[i] = isl_basic_map_range(map->p[i]);
		if (!set->p[i])
			goto error;
	}
	ISL_F_CLR(set, ISL_MAP_DISJOINT);
	ISL_F_CLR(set, ISL_SET_NORMALIZED);
	return set;
error:
	isl_map_free(map);
	return NULL;
}

struct isl_map *isl_map_from_set(struct isl_set *set, struct isl_dim *dim)
{
	int i;
	struct isl_map *map = NULL;

	set = isl_set_cow(set);
	if (!set || !dim)
		goto error;
	isl_assert(set->ctx, isl_dim_compatible(set->dim, dim), goto error);
	map = (struct isl_map *)set;
	for (i = 0; i < set->n; ++i) {
		map->p[i] = isl_basic_map_from_basic_set(
				set->p[i], isl_dim_copy(dim));
		if (!map->p[i])
			goto error;
	}
	isl_dim_free(map->dim);
	map->dim = dim;
	return map;
error:
	isl_dim_free(dim);
	isl_set_free(set);
	return NULL;
}

struct isl_map *isl_map_from_range(struct isl_set *set)
{
	return (struct isl_map *)set;
}

__isl_give isl_map *isl_map_from_domain(__isl_take isl_set *set)
{
	return isl_map_reverse(isl_map_from_range(set));;
}

__isl_give isl_map *isl_map_from_domain_and_range(__isl_take isl_set *domain,
	__isl_take isl_set *range)
{
	return isl_map_product(isl_map_from_domain(domain),
			       isl_map_from_range(range));
}

struct isl_set *isl_set_from_map(struct isl_map *map)
{
	int i;
	struct isl_set *set = NULL;

	if (!map)
		return NULL;
	map = isl_map_cow(map);
	if (!map)
		return NULL;
	map->dim = isl_dim_cow(map->dim);
	if (!map->dim)
		goto error;
	map->dim->n_out += map->dim->n_in;
	map->dim->n_in = 0;
	set = (struct isl_set *)map;
	for (i = 0; i < map->n; ++i) {
		set->p[i] = isl_basic_set_from_basic_map(map->p[i]);
		if (!set->p[i])
			goto error;
	}
	return set;
error:
	isl_map_free(map);
	return NULL;
}

struct isl_map *isl_map_alloc_dim(struct isl_dim *dim, int n, unsigned flags)
{
	struct isl_map *map;

	if (!dim)
		return NULL;
	isl_assert(dim->ctx, n >= 0, return NULL);
	map = isl_alloc(dim->ctx, struct isl_map,
			sizeof(struct isl_map) +
			(n - 1) * sizeof(struct isl_basic_map *));
	if (!map)
		goto error;

	map->ctx = dim->ctx;
	isl_ctx_ref(map->ctx);
	map->ref = 1;
	map->size = n;
	map->n = 0;
	map->dim = dim;
	map->flags = flags;
	return map;
error:
	isl_dim_free(dim);
	return NULL;
}

struct isl_map *isl_map_alloc(struct isl_ctx *ctx,
		unsigned nparam, unsigned in, unsigned out, int n,
		unsigned flags)
{
	struct isl_map *map;
	struct isl_dim *dims;

	dims = isl_dim_alloc(ctx, nparam, in, out);
	if (!dims)
		return NULL;

	map = isl_map_alloc_dim(dims, n, flags);
	return map;
}

struct isl_basic_map *isl_basic_map_empty(struct isl_dim *dim)
{
	struct isl_basic_map *bmap;
	bmap = isl_basic_map_alloc_dim(dim, 0, 1, 0);
	bmap = isl_basic_map_set_to_empty(bmap);
	return bmap;
}

struct isl_basic_set *isl_basic_set_empty(struct isl_dim *dim)
{
	struct isl_basic_set *bset;
	bset = isl_basic_set_alloc_dim(dim, 0, 1, 0);
	bset = isl_basic_set_set_to_empty(bset);
	return bset;
}

struct isl_basic_map *isl_basic_map_empty_like(struct isl_basic_map *model)
{
	struct isl_basic_map *bmap;
	if (!model)
		return NULL;
	bmap = isl_basic_map_alloc_dim(isl_dim_copy(model->dim), 0, 1, 0);
	bmap = isl_basic_map_set_to_empty(bmap);
	return bmap;
}

struct isl_basic_map *isl_basic_map_empty_like_map(struct isl_map *model)
{
	struct isl_basic_map *bmap;
	if (!model)
		return NULL;
	bmap = isl_basic_map_alloc_dim(isl_dim_copy(model->dim), 0, 1, 0);
	bmap = isl_basic_map_set_to_empty(bmap);
	return bmap;
}

struct isl_basic_set *isl_basic_set_empty_like(struct isl_basic_set *model)
{
	struct isl_basic_set *bset;
	if (!model)
		return NULL;
	bset = isl_basic_set_alloc_dim(isl_dim_copy(model->dim), 0, 1, 0);
	bset = isl_basic_set_set_to_empty(bset);
	return bset;
}

struct isl_basic_map *isl_basic_map_universe(struct isl_dim *dim)
{
	struct isl_basic_map *bmap;
	bmap = isl_basic_map_alloc_dim(dim, 0, 0, 0);
	return bmap;
}

struct isl_basic_set *isl_basic_set_universe(struct isl_dim *dim)
{
	struct isl_basic_set *bset;
	bset = isl_basic_set_alloc_dim(dim, 0, 0, 0);
	return bset;
}

__isl_give isl_basic_map *isl_basic_map_universe_like(
		__isl_keep isl_basic_map *model)
{
	if (!model)
		return NULL;
	return isl_basic_map_alloc_dim(isl_dim_copy(model->dim), 0, 0, 0);
}

struct isl_basic_set *isl_basic_set_universe_like(struct isl_basic_set *model)
{
	if (!model)
		return NULL;
	return isl_basic_set_alloc_dim(isl_dim_copy(model->dim), 0, 0, 0);
}

__isl_give isl_basic_set *isl_basic_set_universe_like_set(
	__isl_keep isl_set *model)
{
	if (!model)
		return NULL;
	return isl_basic_set_alloc_dim(isl_dim_copy(model->dim), 0, 0, 0);
}

struct isl_map *isl_map_empty(struct isl_dim *dim)
{
	return isl_map_alloc_dim(dim, 0, ISL_MAP_DISJOINT);
}

struct isl_map *isl_map_empty_like(struct isl_map *model)
{
	if (!model)
		return NULL;
	return isl_map_alloc_dim(isl_dim_copy(model->dim), 0, ISL_MAP_DISJOINT);
}

struct isl_map *isl_map_empty_like_basic_map(struct isl_basic_map *model)
{
	if (!model)
		return NULL;
	return isl_map_alloc_dim(isl_dim_copy(model->dim), 0, ISL_MAP_DISJOINT);
}

struct isl_set *isl_set_empty(struct isl_dim *dim)
{
	return isl_set_alloc_dim(dim, 0, ISL_MAP_DISJOINT);
}

struct isl_set *isl_set_empty_like(struct isl_set *model)
{
	if (!model)
		return NULL;
	return isl_set_empty(isl_dim_copy(model->dim));
}

struct isl_map *isl_map_universe(struct isl_dim *dim)
{
	struct isl_map *map;
	if (!dim)
		return NULL;
	map = isl_map_alloc_dim(isl_dim_copy(dim), 1, ISL_MAP_DISJOINT);
	map = isl_map_add_basic_map(map, isl_basic_map_universe(dim));
	return map;
}

struct isl_set *isl_set_universe(struct isl_dim *dim)
{
	struct isl_set *set;
	if (!dim)
		return NULL;
	set = isl_set_alloc_dim(isl_dim_copy(dim), 1, ISL_MAP_DISJOINT);
	set = isl_set_add_basic_set(set, isl_basic_set_universe(dim));
	return set;
}

__isl_give isl_set *isl_set_universe_like(__isl_keep isl_set *model)
{
	if (!model)
		return NULL;
	return isl_set_universe(isl_dim_copy(model->dim));
}

struct isl_map *isl_map_dup(struct isl_map *map)
{
	int i;
	struct isl_map *dup;

	if (!map)
		return NULL;
	dup = isl_map_alloc_dim(isl_dim_copy(map->dim), map->n, map->flags);
	for (i = 0; i < map->n; ++i)
		dup = isl_map_add_basic_map(dup, isl_basic_map_copy(map->p[i]));
	return dup;
}

__isl_give isl_map *isl_map_add_basic_map(__isl_take isl_map *map,
						__isl_take isl_basic_map *bmap)
{
	if (!bmap || !map)
		goto error;
	if (isl_basic_map_fast_is_empty(bmap)) {
		isl_basic_map_free(bmap);
		return map;
	}
	isl_assert(map->ctx, isl_dim_equal(map->dim, bmap->dim), goto error);
	isl_assert(map->ctx, map->n < map->size, goto error);
	map->p[map->n] = bmap;
	map->n++;
	ISL_F_CLR(map, ISL_MAP_NORMALIZED);
	return map;
error:
	if (map)
		isl_map_free(map);
	if (bmap)
		isl_basic_map_free(bmap);
	return NULL;
}

void isl_map_free(struct isl_map *map)
{
	int i;

	if (!map)
		return;

	if (--map->ref > 0)
		return;

	isl_ctx_deref(map->ctx);
	for (i = 0; i < map->n; ++i)
		isl_basic_map_free(map->p[i]);
	isl_dim_free(map->dim);
	free(map);
}

struct isl_map *isl_map_extend(struct isl_map *base,
		unsigned nparam, unsigned n_in, unsigned n_out)
{
	int i;

	base = isl_map_cow(base);
	if (!base)
		return NULL;

	base->dim = isl_dim_extend(base->dim, nparam, n_in, n_out);
	if (!base->dim)
		goto error;
	for (i = 0; i < base->n; ++i) {
		base->p[i] = isl_basic_map_extend_dim(base->p[i],
				isl_dim_copy(base->dim), 0, 0, 0);
		if (!base->p[i])
			goto error;
	}
	return base;
error:
	isl_map_free(base);
	return NULL;
}

struct isl_set *isl_set_extend(struct isl_set *base,
		unsigned nparam, unsigned dim)
{
	return (struct isl_set *)isl_map_extend((struct isl_map *)base,
							nparam, 0, dim);
}

static struct isl_basic_map *isl_basic_map_fix_pos_si(
	struct isl_basic_map *bmap, unsigned pos, int value)
{
	int j;

	bmap = isl_basic_map_cow(bmap);
	bmap = isl_basic_map_extend_constraints(bmap, 1, 0);
	j = isl_basic_map_alloc_equality(bmap);
	if (j < 0)
		goto error;
	isl_seq_clr(bmap->eq[j] + 1, isl_basic_map_total_dim(bmap));
	isl_int_set_si(bmap->eq[j][pos], -1);
	isl_int_set_si(bmap->eq[j][0], value);
	bmap = isl_basic_map_simplify(bmap);
	return isl_basic_map_finalize(bmap);
error:
	isl_basic_map_free(bmap);
	return NULL;
}

static __isl_give isl_basic_map *isl_basic_map_fix_pos(
	__isl_take isl_basic_map *bmap, unsigned pos, isl_int value)
{
	int j;

	bmap = isl_basic_map_cow(bmap);
	bmap = isl_basic_map_extend_constraints(bmap, 1, 0);
	j = isl_basic_map_alloc_equality(bmap);
	if (j < 0)
		goto error;
	isl_seq_clr(bmap->eq[j] + 1, isl_basic_map_total_dim(bmap));
	isl_int_set_si(bmap->eq[j][pos], -1);
	isl_int_set(bmap->eq[j][0], value);
	bmap = isl_basic_map_simplify(bmap);
	return isl_basic_map_finalize(bmap);
error:
	isl_basic_map_free(bmap);
	return NULL;
}

struct isl_basic_map *isl_basic_map_fix_si(struct isl_basic_map *bmap,
		enum isl_dim_type type, unsigned pos, int value)
{
	if (!bmap)
		return NULL;
	isl_assert(bmap->ctx, pos < isl_basic_map_dim(bmap, type), goto error);
	return isl_basic_map_fix_pos_si(bmap,
		isl_basic_map_offset(bmap, type) + pos, value);
error:
	isl_basic_map_free(bmap);
	return NULL;
}

__isl_give isl_basic_map *isl_basic_map_fix(__isl_take isl_basic_map *bmap,
		enum isl_dim_type type, unsigned pos, isl_int value)
{
	if (!bmap)
		return NULL;
	isl_assert(bmap->ctx, pos < isl_basic_map_dim(bmap, type), goto error);
	return isl_basic_map_fix_pos(bmap,
		isl_basic_map_offset(bmap, type) + pos, value);
error:
	isl_basic_map_free(bmap);
	return NULL;
}

struct isl_basic_set *isl_basic_set_fix_si(struct isl_basic_set *bset,
		enum isl_dim_type type, unsigned pos, int value)
{
	return (struct isl_basic_set *)
		isl_basic_map_fix_si((struct isl_basic_map *)bset,
					type, pos, value);
}

__isl_give isl_basic_set *isl_basic_set_fix(__isl_take isl_basic_set *bset,
		enum isl_dim_type type, unsigned pos, isl_int value)
{
	return (struct isl_basic_set *)
		isl_basic_map_fix((struct isl_basic_map *)bset,
					type, pos, value);
}

struct isl_basic_map *isl_basic_map_fix_input_si(struct isl_basic_map *bmap,
		unsigned input, int value)
{
	return isl_basic_map_fix_si(bmap, isl_dim_in, input, value);
}

struct isl_basic_set *isl_basic_set_fix_dim_si(struct isl_basic_set *bset,
		unsigned dim, int value)
{
	return (struct isl_basic_set *)
		isl_basic_map_fix_si((struct isl_basic_map *)bset,
					isl_dim_set, dim, value);
}

struct isl_map *isl_map_fix_si(struct isl_map *map,
		enum isl_dim_type type, unsigned pos, int value)
{
	int i;

	map = isl_map_cow(map);
	if (!map)
		return NULL;

	isl_assert(map->ctx, pos < isl_map_dim(map, type), goto error);
	for (i = 0; i < map->n; ++i) {
		map->p[i] = isl_basic_map_fix_si(map->p[i], type, pos, value);
		if (!map->p[i])
			goto error;
	}
	ISL_F_CLR(map, ISL_MAP_NORMALIZED);
	return map;
error:
	isl_map_free(map);
	return NULL;
}

__isl_give isl_set *isl_set_fix_si(__isl_take isl_set *set,
		enum isl_dim_type type, unsigned pos, int value)
{
	return (struct isl_set *)
		isl_map_fix_si((struct isl_map *)set, type, pos, value);
}

__isl_give isl_map *isl_map_fix(__isl_take isl_map *map,
		enum isl_dim_type type, unsigned pos, isl_int value)
{
	int i;

	map = isl_map_cow(map);
	if (!map)
		return NULL;

	isl_assert(map->ctx, pos < isl_map_dim(map, type), goto error);
	for (i = 0; i < map->n; ++i) {
		map->p[i] = isl_basic_map_fix(map->p[i], type, pos, value);
		if (!map->p[i])
			goto error;
	}
	ISL_F_CLR(map, ISL_MAP_NORMALIZED);
	return map;
error:
	isl_map_free(map);
	return NULL;
}

__isl_give isl_set *isl_set_fix(__isl_take isl_set *set,
		enum isl_dim_type type, unsigned pos, isl_int value)
{
	return (struct isl_set *)isl_map_fix((isl_map *)set, type, pos, value);
}

struct isl_map *isl_map_fix_input_si(struct isl_map *map,
		unsigned input, int value)
{
	return isl_map_fix_si(map, isl_dim_in, input, value);
}

struct isl_set *isl_set_fix_dim_si(struct isl_set *set, unsigned dim, int value)
{
	return (struct isl_set *)
		isl_map_fix_si((struct isl_map *)set, isl_dim_set, dim, value);
}

__isl_give isl_basic_map *isl_basic_map_lower_bound_si(
		__isl_take isl_basic_map *bmap,
		enum isl_dim_type type, unsigned pos, int value)
{
	int j;

	if (!bmap)
		return NULL;
	isl_assert(bmap->ctx, pos < isl_basic_map_dim(bmap, type), goto error);
	pos += isl_basic_map_offset(bmap, type);
	bmap = isl_basic_map_cow(bmap);
	bmap = isl_basic_map_extend_constraints(bmap, 0, 1);
	j = isl_basic_map_alloc_inequality(bmap);
	if (j < 0)
		goto error;
	isl_seq_clr(bmap->ineq[j], 1 + isl_basic_map_total_dim(bmap));
	isl_int_set_si(bmap->ineq[j][pos], 1);
	isl_int_set_si(bmap->ineq[j][0], -value);
	bmap = isl_basic_map_simplify(bmap);
	return isl_basic_map_finalize(bmap);
error:
	isl_basic_map_free(bmap);
	return NULL;
}

struct isl_basic_set *isl_basic_set_lower_bound_dim(struct isl_basic_set *bset,
	unsigned dim, isl_int value)
{
	int j;

	bset = isl_basic_set_cow(bset);
	bset = isl_basic_set_extend_constraints(bset, 0, 1);
	j = isl_basic_set_alloc_inequality(bset);
	if (j < 0)
		goto error;
	isl_seq_clr(bset->ineq[j], 1 + isl_basic_set_total_dim(bset));
	isl_int_set_si(bset->ineq[j][1 + isl_basic_set_n_param(bset) + dim], 1);
	isl_int_neg(bset->ineq[j][0], value);
	bset = isl_basic_set_simplify(bset);
	return isl_basic_set_finalize(bset);
error:
	isl_basic_set_free(bset);
	return NULL;
}

__isl_give isl_map *isl_map_lower_bound_si(__isl_take isl_map *map,
		enum isl_dim_type type, unsigned pos, int value)
{
	int i;

	map = isl_map_cow(map);
	if (!map)
		return NULL;

	isl_assert(map->ctx, pos < isl_map_dim(map, type), goto error);
	for (i = 0; i < map->n; ++i) {
		map->p[i] = isl_basic_map_lower_bound_si(map->p[i],
							 type, pos, value);
		if (!map->p[i])
			goto error;
	}
	ISL_F_CLR(map, ISL_MAP_NORMALIZED);
	return map;
error:
	isl_map_free(map);
	return NULL;
}

__isl_give isl_set *isl_set_lower_bound_si(__isl_take isl_set *set,
		enum isl_dim_type type, unsigned pos, int value)
{
	return (struct isl_set *)
		isl_map_lower_bound_si((struct isl_map *)set, type, pos, value);
}

struct isl_set *isl_set_lower_bound_dim(struct isl_set *set, unsigned dim,
					isl_int value)
{
	int i;

	set = isl_set_cow(set);
	if (!set)
		return NULL;

	isl_assert(set->ctx, dim < isl_set_n_dim(set), goto error);
	for (i = 0; i < set->n; ++i) {
		set->p[i] = isl_basic_set_lower_bound_dim(set->p[i], dim, value);
		if (!set->p[i])
			goto error;
	}
	return set;
error:
	isl_set_free(set);
	return NULL;
}

struct isl_map *isl_map_reverse(struct isl_map *map)
{
	int i;

	map = isl_map_cow(map);
	if (!map)
		return NULL;

	map->dim = isl_dim_reverse(map->dim);
	if (!map->dim)
		goto error;
	for (i = 0; i < map->n; ++i) {
		map->p[i] = isl_basic_map_reverse(map->p[i]);
		if (!map->p[i])
			goto error;
	}
	ISL_F_CLR(map, ISL_MAP_NORMALIZED);
	return map;
error:
	isl_map_free(map);
	return NULL;
}

static struct isl_map *isl_basic_map_partial_lexopt(
		struct isl_basic_map *bmap, struct isl_basic_set *dom,
		struct isl_set **empty, int max)
{
	if (!bmap)
		goto error;
	if (bmap->ctx->opt->pip == ISL_PIP_PIP)
		return isl_pip_basic_map_lexopt(bmap, dom, empty, max);
	else
		return isl_tab_basic_map_partial_lexopt(bmap, dom, empty, max);
error:
	isl_basic_map_free(bmap);
	isl_basic_set_free(dom);
	if (empty)
		*empty = NULL;
	return NULL;
}

struct isl_map *isl_basic_map_partial_lexmax(
		struct isl_basic_map *bmap, struct isl_basic_set *dom,
		struct isl_set **empty)
{
	return isl_basic_map_partial_lexopt(bmap, dom, empty, 1);
}

struct isl_map *isl_basic_map_partial_lexmin(
		struct isl_basic_map *bmap, struct isl_basic_set *dom,
		struct isl_set **empty)
{
	return isl_basic_map_partial_lexopt(bmap, dom, empty, 0);
}

struct isl_set *isl_basic_set_partial_lexmin(
		struct isl_basic_set *bset, struct isl_basic_set *dom,
		struct isl_set **empty)
{
	return (struct isl_set *)
		isl_basic_map_partial_lexmin((struct isl_basic_map *)bset,
			dom, empty);
}

struct isl_set *isl_basic_set_partial_lexmax(
		struct isl_basic_set *bset, struct isl_basic_set *dom,
		struct isl_set **empty)
{
	return (struct isl_set *)
		isl_basic_map_partial_lexmax((struct isl_basic_map *)bset,
			dom, empty);
}

/* Given a basic map "bmap", compute the lexicograhically minimal
 * (or maximal) image element for each domain element in dom.
 * Set *empty to those elements in dom that do not have an image element.
 *
 * We first make sure the basic sets in dom are disjoint and then
 * simply collect the results over each of the basic sets separately.
 * We could probably improve the efficiency a bit by moving the union
 * domain down into the parametric integer programming.
 */
static __isl_give isl_map *basic_map_partial_lexopt(
		__isl_take isl_basic_map *bmap, __isl_take isl_set *dom,
		__isl_give isl_set **empty, int max)
{
	int i;
	struct isl_map *res;

	dom = isl_set_make_disjoint(dom);
	if (!dom)
		goto error;

	if (isl_set_fast_is_empty(dom)) {
		res = isl_map_empty_like_basic_map(bmap);
		*empty = isl_set_empty_like(dom);
		isl_set_free(dom);
		isl_basic_map_free(bmap);
		return res;
	}

	res = isl_basic_map_partial_lexopt(isl_basic_map_copy(bmap),
			isl_basic_set_copy(dom->p[0]), empty, max);
		
	for (i = 1; i < dom->n; ++i) {
		struct isl_map *res_i;
		struct isl_set *empty_i;

		res_i = isl_basic_map_partial_lexopt(isl_basic_map_copy(bmap),
				isl_basic_set_copy(dom->p[i]), &empty_i, max);

		res = isl_map_union_disjoint(res, res_i);
		*empty = isl_set_union_disjoint(*empty, empty_i);
	}

	isl_set_free(dom);
	isl_basic_map_free(bmap);
	return res;
error:
	*empty = NULL;
	isl_set_free(dom);
	isl_basic_map_free(bmap);
	return NULL;
}

/* Given a map "map", compute the lexicograhically minimal
 * (or maximal) image element for each domain element in dom.
 * Set *empty to those elements in dom that do not have an image element.
 *
 * We first compute the lexicographically minimal or maximal element
 * in the first basic map.  This results in a partial solution "res"
 * and a subset "todo" of dom that still need to be handled.
 * We then consider each of the remaining maps in "map" and successively
 * improve both "res" and "todo".
 *
 * Let res^k and todo^k be the results after k steps and let i = k + 1.
 * Assume we are computing the lexicographical maximum.
 * We first intersect basic map i with a relation that maps elements
 * to elements that are lexicographically larger than the image elements
 * in res^k and the compute the maximum image element of this intersection.
 * The result ("better") corresponds to those image elements in basic map i
 * that are better than what we had before.  The remainder ("keep") are the
 * domain elements for which the image element in res_k was better.
 * We also compute the lexicographical maximum of basic map i in todo^k.
 * res^i is the result of the operation + better + those elements in
 *		res^k that we should keep
 * todo^i is the remainder of the maximum operation on todo^k.
 */
static __isl_give isl_map *isl_map_partial_lexopt(
		__isl_take isl_map *map, __isl_take isl_set *dom,
		__isl_give isl_set **empty, int max)
{
	int i;
	struct isl_map *res;
	struct isl_set *todo;

	if (!map || !dom)
		goto error;

	if (isl_map_fast_is_empty(map)) {
		if (empty)
			*empty = dom;
		else
			isl_set_free(dom);
		return map;
	}

	res = basic_map_partial_lexopt(isl_basic_map_copy(map->p[0]),
					isl_set_copy(dom), &todo, max);

	for (i = 1; i < map->n; ++i) {
		struct isl_map *lt;
		struct isl_map *better;
		struct isl_set *keep;
		struct isl_map *res_i;
		struct isl_set *todo_i;
		struct isl_dim *dim = isl_map_get_dim(res);

		dim = isl_dim_range(dim);
		if (max)
			lt = isl_map_lex_lt(dim);
		else
			lt = isl_map_lex_gt(dim);
		lt = isl_map_apply_range(isl_map_copy(res), lt);
		lt = isl_map_intersect(lt,
			isl_map_from_basic_map(isl_basic_map_copy(map->p[i])));
		better = isl_map_partial_lexopt(lt,
			isl_map_domain(isl_map_copy(res)),
			&keep, max);

		res_i = basic_map_partial_lexopt(isl_basic_map_copy(map->p[i]),
						todo, &todo_i, max);

		res = isl_map_intersect_domain(res, keep);
		res = isl_map_union_disjoint(res, res_i);
		res = isl_map_union_disjoint(res, better);
		todo = todo_i;
	}

	isl_set_free(dom);
	isl_map_free(map);

	if (empty)
		*empty = todo;
	else
		isl_set_free(todo);

	return res;
error:
	if (empty)
		*empty = NULL;
	isl_set_free(dom);
	isl_map_free(map);
	return NULL;
}

__isl_give isl_map *isl_map_partial_lexmax(
		__isl_take isl_map *map, __isl_take isl_set *dom,
		__isl_give isl_set **empty)
{
	return isl_map_partial_lexopt(map, dom, empty, 1);
}

__isl_give isl_map *isl_map_partial_lexmin(
		__isl_take isl_map *map, __isl_take isl_set *dom,
		__isl_give isl_set **empty)
{
	return isl_map_partial_lexopt(map, dom, empty, 0);
}

__isl_give isl_set *isl_set_partial_lexmin(
		__isl_take isl_set *set, __isl_take isl_set *dom,
		__isl_give isl_set **empty)
{
	return (struct isl_set *)
		isl_map_partial_lexmin((struct isl_map *)set,
			dom, empty);
}

__isl_give isl_set *isl_set_partial_lexmax(
		__isl_take isl_set *set, __isl_take isl_set *dom,
		__isl_give isl_set **empty)
{
	return (struct isl_set *)
		isl_map_partial_lexmax((struct isl_map *)set,
			dom, empty);
}

__isl_give isl_map *isl_basic_map_lexopt(__isl_take isl_basic_map *bmap, int max)
{
	struct isl_basic_set *dom = NULL;
	struct isl_dim *dom_dim;

	if (!bmap)
		goto error;
	dom_dim = isl_dim_domain(isl_dim_copy(bmap->dim));
	dom = isl_basic_set_universe(dom_dim);
	return isl_basic_map_partial_lexopt(bmap, dom, NULL, max);
error:
	isl_basic_map_free(bmap);
	return NULL;
}

__isl_give isl_map *isl_basic_map_lexmin(__isl_take isl_basic_map *bmap)
{
	return isl_basic_map_lexopt(bmap, 0);
}

__isl_give isl_map *isl_basic_map_lexmax(__isl_take isl_basic_map *bmap)
{
	return isl_basic_map_lexopt(bmap, 1);
}

__isl_give isl_set *isl_basic_set_lexmin(__isl_take isl_basic_set *bset)
{
	return (isl_set *)isl_basic_map_lexmin((isl_basic_map *)bset);
}

__isl_give isl_set *isl_basic_set_lexmax(__isl_take isl_basic_set *bset)
{
	return (isl_set *)isl_basic_map_lexmax((isl_basic_map *)bset);
}

__isl_give isl_map *isl_map_lexopt(__isl_take isl_map *map, int max)
{
	struct isl_set *dom = NULL;
	struct isl_dim *dom_dim;

	if (!map)
		goto error;
	dom_dim = isl_dim_domain(isl_dim_copy(map->dim));
	dom = isl_set_universe(dom_dim);
	return isl_map_partial_lexopt(map, dom, NULL, max);
error:
	isl_map_free(map);
	return NULL;
}

__isl_give isl_map *isl_map_lexmin(__isl_take isl_map *map)
{
	return isl_map_lexopt(map, 0);
}

__isl_give isl_map *isl_map_lexmax(__isl_take isl_map *map)
{
	return isl_map_lexopt(map, 1);
}

__isl_give isl_set *isl_set_lexmin(__isl_take isl_set *set)
{
	return (isl_set *)isl_map_lexmin((isl_map *)set);
}

__isl_give isl_set *isl_set_lexmax(__isl_take isl_set *set)
{
	return (isl_set *)isl_map_lexmax((isl_map *)set);
}

static struct isl_map *isl_map_reset_dim(struct isl_map *map,
	struct isl_dim *dim)
{
	int i;

	if (!map || !dim)
		goto error;

	for (i = 0; i < map->n; ++i) {
		isl_dim_free(map->p[i]->dim);
		map->p[i]->dim = isl_dim_copy(dim);
	}
	isl_dim_free(map->dim);
	map->dim = dim;

	return map;
error:
	isl_map_free(map);
	isl_dim_free(dim);
	return NULL;
}

static struct isl_set *isl_set_reset_dim(struct isl_set *set,
	struct isl_dim *dim)
{
	return (struct isl_set *) isl_map_reset_dim((struct isl_map *)set, dim);
}

/* Apply a preimage specified by "mat" on the parameters of "bset".
 * bset is assumed to have only parameters and divs.
 */
static struct isl_basic_set *basic_set_parameter_preimage(
	struct isl_basic_set *bset, struct isl_mat *mat)
{
	unsigned nparam;

	if (!bset || !mat)
		goto error;

	bset->dim = isl_dim_cow(bset->dim);
	if (!bset->dim)
		goto error;

	nparam = isl_basic_set_dim(bset, isl_dim_param);

	isl_assert(bset->ctx, mat->n_row == 1 + nparam, goto error);

	bset->dim->nparam = 0;
	bset->dim->n_out = nparam;
	bset = isl_basic_set_preimage(bset, mat);
	if (bset) {
		bset->dim->nparam = bset->dim->n_out;
		bset->dim->n_out = 0;
	}
	return bset;
error:
	isl_mat_free(mat);
	isl_basic_set_free(bset);
	return NULL;
}

/* Apply a preimage specified by "mat" on the parameters of "set".
 * set is assumed to have only parameters and divs.
 */
static struct isl_set *set_parameter_preimage(
	struct isl_set *set, struct isl_mat *mat)
{
	struct isl_dim *dim = NULL;
	unsigned nparam;

	if (!set || !mat)
		goto error;

	dim = isl_dim_copy(set->dim);
	dim = isl_dim_cow(dim);
	if (!dim)
		goto error;

	nparam = isl_set_dim(set, isl_dim_param);

	isl_assert(set->ctx, mat->n_row == 1 + nparam, goto error);

	dim->nparam = 0;
	dim->n_out = nparam;
	isl_set_reset_dim(set, dim);
	set = isl_set_preimage(set, mat);
	if (!set)
		goto error2;
	dim = isl_dim_copy(set->dim);
	dim = isl_dim_cow(dim);
	if (!dim)
		goto error2;
	dim->nparam = dim->n_out;
	dim->n_out = 0;
	isl_set_reset_dim(set, dim);
	return set;
error:
	isl_dim_free(dim);
	isl_mat_free(mat);
error2:
	isl_set_free(set);
	return NULL;
}

/* Intersect the basic set "bset" with the affine space specified by the
 * equalities in "eq".
 */
static struct isl_basic_set *basic_set_append_equalities(
	struct isl_basic_set *bset, struct isl_mat *eq)
{
	int i, k;
	unsigned len;

	if (!bset || !eq)
		goto error;

	bset = isl_basic_set_extend_dim(bset, isl_dim_copy(bset->dim), 0,
					eq->n_row, 0);
	if (!bset)
		goto error;

	len = 1 + isl_dim_total(bset->dim) + bset->extra;
	for (i = 0; i < eq->n_row; ++i) {
		k = isl_basic_set_alloc_equality(bset);
		if (k < 0)
			goto error;
		isl_seq_cpy(bset->eq[k], eq->row[i], eq->n_col);
		isl_seq_clr(bset->eq[k] + eq->n_col, len - eq->n_col);
	}
	isl_mat_free(eq);

	bset = isl_basic_set_gauss(bset, NULL);
	bset = isl_basic_set_finalize(bset);

	return bset;
error:
	isl_mat_free(eq);
	isl_basic_set_free(bset);
	return NULL;
}

/* Intersect the set "set" with the affine space specified by the
 * equalities in "eq".
 */
static struct isl_set *set_append_equalities(struct isl_set *set,
	struct isl_mat *eq)
{
	int i;

	if (!set || !eq)
		goto error;

	for (i = 0; i < set->n; ++i) {
		set->p[i] = basic_set_append_equalities(set->p[i],
					isl_mat_copy(eq));
		if (!set->p[i])
			goto error;
	}
	isl_mat_free(eq);
	return set;
error:
	isl_mat_free(eq);
	isl_set_free(set);
	return NULL;
}

/* Project the given basic set onto its parameter domain, possibly introducing
 * new, explicit, existential variables in the constraints.
 * The input has parameters and (possibly implicit) existential variables.
 * The output has the same parameters, but only
 * explicit existentially quantified variables.
 *
 * The actual projection is performed by pip, but pip doesn't seem
 * to like equalities very much, so we first remove the equalities
 * among the parameters by performing a variable compression on
 * the parameters.  Afterward, an inverse transformation is performed
 * and the equalities among the parameters are inserted back in.
 */
static struct isl_set *parameter_compute_divs(struct isl_basic_set *bset)
{
	int i, j;
	struct isl_mat *eq;
	struct isl_mat *T, *T2;
	struct isl_set *set;
	unsigned nparam, n_div;

	bset = isl_basic_set_cow(bset);
	if (!bset)
		return NULL;

	if (bset->n_eq == 0)
		return isl_basic_set_lexmin(bset);

	isl_basic_set_gauss(bset, NULL);

	nparam = isl_basic_set_dim(bset, isl_dim_param);
	n_div = isl_basic_set_dim(bset, isl_dim_div);

	for (i = 0, j = n_div - 1; i < bset->n_eq && j >= 0; --j) {
		if (!isl_int_is_zero(bset->eq[i][1 + nparam + j]))
			++i;
	}
	if (i == bset->n_eq)
		return isl_basic_set_lexmin(bset);

	eq = isl_mat_sub_alloc(bset->ctx, bset->eq, i, bset->n_eq - i,
		0, 1 + nparam);
	eq = isl_mat_cow(eq);
	T = isl_mat_variable_compression(isl_mat_copy(eq), &T2);
	if (T && T->n_col == 0) {
		isl_mat_free(T);
		isl_mat_free(T2);
		isl_mat_free(eq);
		bset = isl_basic_set_set_to_empty(bset);
		return isl_set_from_basic_set(bset);
	}
	bset = basic_set_parameter_preimage(bset, T);

	set = isl_basic_set_lexmin(bset);
	set = set_parameter_preimage(set, T2);
	set = set_append_equalities(set, eq);
	return set;
}

/* Compute an explicit representation for all the existentially
 * quantified variables.
 * The input and output dimensions are first turned into parameters.
 * compute_divs then returns a map with the same parameters and
 * no input or output dimensions and the dimension specification
 * is reset to that of the input.
 */
static struct isl_map *compute_divs(struct isl_basic_map *bmap)
{
	struct isl_basic_set *bset;
	struct isl_set *set;
	struct isl_map *map;
	struct isl_dim *dim, *orig_dim = NULL;
	unsigned	 nparam;
	unsigned	 n_in;
	unsigned	 n_out;

	bmap = isl_basic_map_cow(bmap);
	if (!bmap)
		return NULL;

	nparam = isl_basic_map_dim(bmap, isl_dim_param);
	n_in = isl_basic_map_dim(bmap, isl_dim_in);
	n_out = isl_basic_map_dim(bmap, isl_dim_out);
	dim = isl_dim_set_alloc(bmap->ctx, nparam + n_in + n_out, 0);
	if (!dim)
		goto error;

	orig_dim = bmap->dim;
	bmap->dim = dim;
	bset = (struct isl_basic_set *)bmap;

	set = parameter_compute_divs(bset);
	map = (struct isl_map *)set;
	map = isl_map_reset_dim(map, orig_dim);

	return map;
error:
	isl_basic_map_free(bmap);
	return NULL;
}

static int basic_map_divs_known(__isl_keep isl_basic_map *bmap)
{
	int i;
	unsigned off;

	if (!bmap)
		return -1;

	off = isl_dim_total(bmap->dim);
	for (i = 0; i < bmap->n_div; ++i) {
		if (isl_int_is_zero(bmap->div[i][0]))
			return 0;
		isl_assert(bmap->ctx, isl_int_is_zero(bmap->div[i][1+1+off+i]),
				return -1);
	}
	return 1;
}

static int map_divs_known(__isl_keep isl_map *map)
{
	int i;

	if (!map)
		return -1;

	for (i = 0; i < map->n; ++i) {
		int known = basic_map_divs_known(map->p[i]);
		if (known <= 0)
			return known;
	}

	return 1;
}

/* If bmap contains any unknown divs, then compute explicit
 * expressions for them.  However, this computation may be
 * quite expensive, so first try to remove divs that aren't
 * strictly needed.
 */
struct isl_map *isl_basic_map_compute_divs(struct isl_basic_map *bmap)
{
	int i;
	int known;
	struct isl_map *map;

	known = basic_map_divs_known(bmap);
	if (known < 0)
		goto error;
	if (known)
		return isl_map_from_basic_map(bmap);

	bmap = isl_basic_map_drop_redundant_divs(bmap);

	known = basic_map_divs_known(bmap);
	if (known < 0)
		goto error;
	if (known)
		return isl_map_from_basic_map(bmap);

	map = compute_divs(bmap);
	return map;
error:
	isl_basic_map_free(bmap);
	return NULL;
}

struct isl_map *isl_map_compute_divs(struct isl_map *map)
{
	int i;
	int known;
	struct isl_map *res;

	if (!map)
		return NULL;
	if (map->n == 0)
		return map;

	known = map_divs_known(map);
	if (known < 0) {
		isl_map_free(map);
		return NULL;
	}
	if (known)
		return map;

	res = isl_basic_map_compute_divs(isl_basic_map_copy(map->p[0]));
	for (i = 1 ; i < map->n; ++i) {
		struct isl_map *r2;
		r2 = isl_basic_map_compute_divs(isl_basic_map_copy(map->p[i]));
		if (ISL_F_ISSET(map, ISL_MAP_DISJOINT))
			res = isl_map_union_disjoint(res, r2);
		else
			res = isl_map_union(res, r2);
	}
	isl_map_free(map);

	return res;
}

struct isl_set *isl_basic_set_compute_divs(struct isl_basic_set *bset)
{
	return (struct isl_set *)
		isl_basic_map_compute_divs((struct isl_basic_map *)bset);
}

struct isl_set *isl_set_compute_divs(struct isl_set *set)
{
	return (struct isl_set *)
		isl_map_compute_divs((struct isl_map *)set);
}

struct isl_set *isl_map_domain(struct isl_map *map)
{
	int i;
	struct isl_set *set;

	if (!map)
		goto error;

	map = isl_map_cow(map);
	if (!map)
		return NULL;

	set = (struct isl_set *)map;
	set->dim = isl_dim_domain(set->dim);
	if (!set->dim)
		goto error;
	for (i = 0; i < map->n; ++i) {
		set->p[i] = isl_basic_map_domain(map->p[i]);
		if (!set->p[i])
			goto error;
	}
	ISL_F_CLR(set, ISL_MAP_DISJOINT);
	ISL_F_CLR(set, ISL_SET_NORMALIZED);
	return set;
error:
	isl_map_free(map);
	return NULL;
}

struct isl_map *isl_map_union_disjoint(
			struct isl_map *map1, struct isl_map *map2)
{
	int i;
	unsigned flags = 0;
	struct isl_map *map = NULL;

	if (!map1 || !map2)
		goto error;

	if (map1->n == 0) {
		isl_map_free(map1);
		return map2;
	}
	if (map2->n == 0) {
		isl_map_free(map2);
		return map1;
	}

	isl_assert(map1->ctx, isl_dim_equal(map1->dim, map2->dim), goto error);

	if (ISL_F_ISSET(map1, ISL_MAP_DISJOINT) &&
	    ISL_F_ISSET(map2, ISL_MAP_DISJOINT))
		ISL_FL_SET(flags, ISL_MAP_DISJOINT);

	map = isl_map_alloc_dim(isl_dim_copy(map1->dim),
				map1->n + map2->n, flags);
	if (!map)
		goto error;
	for (i = 0; i < map1->n; ++i) {
		map = isl_map_add_basic_map(map,
				  isl_basic_map_copy(map1->p[i]));
		if (!map)
			goto error;
	}
	for (i = 0; i < map2->n; ++i) {
		map = isl_map_add_basic_map(map,
				  isl_basic_map_copy(map2->p[i]));
		if (!map)
			goto error;
	}
	isl_map_free(map1);
	isl_map_free(map2);
	return map;
error:
	isl_map_free(map);
	isl_map_free(map1);
	isl_map_free(map2);
	return NULL;
}

struct isl_map *isl_map_union(struct isl_map *map1, struct isl_map *map2)
{
	map1 = isl_map_union_disjoint(map1, map2);
	if (!map1)
		return NULL;
	if (map1->n > 1)
		ISL_F_CLR(map1, ISL_MAP_DISJOINT);
	return map1;
}

struct isl_set *isl_set_union_disjoint(
			struct isl_set *set1, struct isl_set *set2)
{
	return (struct isl_set *)
		isl_map_union_disjoint(
			(struct isl_map *)set1, (struct isl_map *)set2);
}

struct isl_set *isl_set_union(struct isl_set *set1, struct isl_set *set2)
{
	return (struct isl_set *)
		isl_map_union((struct isl_map *)set1, (struct isl_map *)set2);
}

struct isl_map *isl_map_intersect_range(
		struct isl_map *map, struct isl_set *set)
{
	unsigned flags = 0;
	struct isl_map *result;
	int i, j;

	if (!map || !set)
		goto error;

	if (ISL_F_ISSET(map, ISL_MAP_DISJOINT) &&
	    ISL_F_ISSET(set, ISL_MAP_DISJOINT))
		ISL_FL_SET(flags, ISL_MAP_DISJOINT);

	result = isl_map_alloc_dim(isl_dim_copy(map->dim),
					map->n * set->n, flags);
	if (!result)
		goto error;
	for (i = 0; i < map->n; ++i)
		for (j = 0; j < set->n; ++j) {
			result = isl_map_add_basic_map(result,
			    isl_basic_map_intersect_range(
				isl_basic_map_copy(map->p[i]),
				isl_basic_set_copy(set->p[j])));
			if (!result)
				goto error;
		}
	isl_map_free(map);
	isl_set_free(set);
	return result;
error:
	isl_map_free(map);
	isl_set_free(set);
	return NULL;
}

struct isl_map *isl_map_intersect_domain(
		struct isl_map *map, struct isl_set *set)
{
	return isl_map_reverse(
		isl_map_intersect_range(isl_map_reverse(map), set));
}

struct isl_map *isl_map_apply_domain(
		struct isl_map *map1, struct isl_map *map2)
{
	if (!map1 || !map2)
		goto error;
	map1 = isl_map_reverse(map1);
	map1 = isl_map_apply_range(map1, map2);
	return isl_map_reverse(map1);
error:
	isl_map_free(map1);
	isl_map_free(map2);
	return NULL;
}

struct isl_map *isl_map_apply_range(
		struct isl_map *map1, struct isl_map *map2)
{
	struct isl_dim *dim_result;
	struct isl_map *result;
	int i, j;

	if (!map1 || !map2)
		goto error;

	dim_result = isl_dim_join(isl_dim_copy(map1->dim),
				  isl_dim_copy(map2->dim));

	result = isl_map_alloc_dim(dim_result, map1->n * map2->n, 0);
	if (!result)
		goto error;
	for (i = 0; i < map1->n; ++i)
		for (j = 0; j < map2->n; ++j) {
			result = isl_map_add_basic_map(result,
			    isl_basic_map_apply_range(
				isl_basic_map_copy(map1->p[i]),
				isl_basic_map_copy(map2->p[j])));
			if (!result)
				goto error;
		}
	isl_map_free(map1);
	isl_map_free(map2);
	if (result && result->n <= 1)
		ISL_F_SET(result, ISL_MAP_DISJOINT);
	return result;
error:
	isl_map_free(map1);
	isl_map_free(map2);
	return NULL;
}

/*
 * returns range - domain
 */
struct isl_basic_set *isl_basic_map_deltas(struct isl_basic_map *bmap)
{
	isl_dim *dims;
	struct isl_basic_set *bset;
	unsigned dim;
	unsigned nparam;
	int i;

	if (!bmap)
		goto error;
	dim = isl_basic_map_n_in(bmap);
	nparam = isl_basic_map_n_param(bmap);
	isl_assert(bmap->ctx, dim == isl_basic_map_n_out(bmap), goto error);
	bset = isl_basic_set_from_basic_map(bmap);
	bset = isl_basic_set_cow(bset);
	dims = isl_basic_set_get_dim(bset);
	dims = isl_dim_add(dims, isl_dim_set, dim);
	bset = isl_basic_set_extend_dim(bset, dims, 0, dim, 0);
	bset = isl_basic_set_swap_vars(bset, 2*dim);
	for (i = 0; i < dim; ++i) {
		int j = isl_basic_map_alloc_equality(
					    (struct isl_basic_map *)bset);
		if (j < 0)
			goto error;
		isl_seq_clr(bset->eq[j], 1 + isl_basic_set_total_dim(bset));
		isl_int_set_si(bset->eq[j][1+nparam+i], 1);
		isl_int_set_si(bset->eq[j][1+nparam+dim+i], 1);
		isl_int_set_si(bset->eq[j][1+nparam+2*dim+i], -1);
	}
	return isl_basic_set_project_out(bset, isl_dim_set, dim, 2*dim);
error:
	isl_basic_map_free(bmap);
	return NULL;
}

/*
 * returns range - domain
 */
struct isl_set *isl_map_deltas(struct isl_map *map)
{
	int i;
	isl_dim *dim;
	struct isl_set *result;

	if (!map)
		return NULL;

	isl_assert(map->ctx, isl_map_n_in(map) == isl_map_n_out(map), goto error);
	dim = isl_map_get_dim(map);
	dim = isl_dim_domain(dim);
	result = isl_set_alloc_dim(dim, map->n, map->flags);
	if (!result)
		goto error;
	for (i = 0; i < map->n; ++i)
		result = isl_set_add_basic_set(result,
			  isl_basic_map_deltas(isl_basic_map_copy(map->p[i])));
	isl_map_free(map);
	return result;
error:
	isl_map_free(map);
	return NULL;
}

static struct isl_basic_map *basic_map_identity(struct isl_dim *dims)
{
	struct isl_basic_map *bmap;
	unsigned nparam;
	unsigned dim;
	int i;

	if (!dims)
		return NULL;

	nparam = dims->nparam;
	dim = dims->n_out;
	bmap = isl_basic_map_alloc_dim(dims, 0, dim, 0);
	if (!bmap)
		goto error;

	for (i = 0; i < dim; ++i) {
		int j = isl_basic_map_alloc_equality(bmap);
		if (j < 0)
			goto error;
		isl_seq_clr(bmap->eq[j], 1 + isl_basic_map_total_dim(bmap));
		isl_int_set_si(bmap->eq[j][1+nparam+i], 1);
		isl_int_set_si(bmap->eq[j][1+nparam+dim+i], -1);
	}
	return isl_basic_map_finalize(bmap);
error:
	isl_basic_map_free(bmap);
	return NULL;
}

struct isl_basic_map *isl_basic_map_identity(struct isl_dim *set_dim)
{
	struct isl_dim *dim = isl_dim_map(set_dim);
	if (!dim)
		return NULL;
	return basic_map_identity(dim);
}

struct isl_basic_map *isl_basic_map_identity_like(struct isl_basic_map *model)
{
	if (!model || !model->dim)
		return NULL;
	isl_assert(model->ctx,
			model->dim->n_in == model->dim->n_out, return NULL);
	return basic_map_identity(isl_dim_copy(model->dim));
}

static struct isl_map *map_identity(struct isl_dim *dim)
{
	struct isl_map *map = isl_map_alloc_dim(dim, 1, ISL_MAP_DISJOINT);
	return isl_map_add_basic_map(map, basic_map_identity(isl_dim_copy(dim)));
}

struct isl_map *isl_map_identity(struct isl_dim *set_dim)
{
	struct isl_dim *dim = isl_dim_map(set_dim);
	if (!dim)
		return NULL;
	return map_identity(dim);
}

struct isl_map *isl_map_identity_like(struct isl_map *model)
{
	if (!model || !model->dim)
		return NULL;
	isl_assert(model->ctx,
			model->dim->n_in == model->dim->n_out, return NULL);
	return map_identity(isl_dim_copy(model->dim));
}

struct isl_map *isl_map_identity_like_basic_map(struct isl_basic_map *model)
{
	if (!model || !model->dim)
		return NULL;
	isl_assert(model->ctx,
			model->dim->n_in == model->dim->n_out, return NULL);
	return map_identity(isl_dim_copy(model->dim));
}

/* Construct a basic set with all set dimensions having only non-negative
 * values.
 */
struct isl_basic_set *isl_basic_set_positive_orthant(struct isl_dim *dims)
{
	int i;
	unsigned nparam;
	unsigned dim;
	struct isl_basic_set *bset;

	if (!dims)
		return NULL;
	nparam = dims->nparam;
	dim = dims->n_out;
	bset = isl_basic_set_alloc_dim(dims, 0, 0, dim);
	if (!bset)
		return NULL;
	for (i = 0; i < dim; ++i) {
		int k = isl_basic_set_alloc_inequality(bset);
		if (k < 0)
			goto error;
		isl_seq_clr(bset->ineq[k], 1 + isl_basic_set_total_dim(bset));
		isl_int_set_si(bset->ineq[k][1 + nparam + i], 1);
	}
	return bset;
error:
	isl_basic_set_free(bset);
	return NULL;
}

__isl_give isl_set *isl_set_split_dims(__isl_take isl_set *set,
	enum isl_dim_type type, unsigned first, unsigned n)
{
	int i;
	isl_basic_set *nonneg = NULL;
	isl_basic_set *neg = NULL;

	if (!set)
		return NULL;
	if (n == 0)
		return set;

	isl_assert(set->ctx, first + n <= isl_set_dim(set, type), goto error);

	for (i = 0; i < n; ++i) {
		int k;

		neg = NULL;
		nonneg = isl_basic_set_alloc_dim(isl_set_get_dim(set), 0, 0, 1);
		k = isl_basic_set_alloc_inequality(nonneg);
		if (k < 0)
			goto error;
		isl_seq_clr(nonneg->ineq[k], 1 + isl_basic_set_total_dim(nonneg));
		isl_int_set_si(nonneg->ineq[k][pos(set->dim, type) + first + i], 1);

		neg = isl_basic_set_alloc_dim(isl_set_get_dim(set), 0, 0, 1);
		k = isl_basic_set_alloc_inequality(neg);
		if (k < 0)
			goto error;
		isl_seq_clr(neg->ineq[k], 1 + isl_basic_set_total_dim(neg));
		isl_int_set_si(neg->ineq[k][0], -1);
		isl_int_set_si(neg->ineq[k][pos(set->dim, type) + first + i], -1);

		set = isl_set_intersect(set, isl_basic_set_union(nonneg, neg));
	}

	return set;
error:
	isl_basic_set_free(nonneg);
	isl_basic_set_free(neg);
	isl_set_free(set);
	return NULL;
}

int isl_set_is_equal(struct isl_set *set1, struct isl_set *set2)
{
	return isl_map_is_equal((struct isl_map *)set1, (struct isl_map *)set2);
}

int isl_basic_map_is_subset(
		struct isl_basic_map *bmap1, struct isl_basic_map *bmap2)
{
	int is_subset;
	struct isl_map *map1;
	struct isl_map *map2;

	if (!bmap1 || !bmap2)
		return -1;

	map1 = isl_map_from_basic_map(isl_basic_map_copy(bmap1));
	map2 = isl_map_from_basic_map(isl_basic_map_copy(bmap2));

	is_subset = isl_map_is_subset(map1, map2);

	isl_map_free(map1);
	isl_map_free(map2);

	return is_subset;
}

int isl_basic_map_is_equal(
		struct isl_basic_map *bmap1, struct isl_basic_map *bmap2)
{
	int is_subset;

	if (!bmap1 || !bmap2)
		return -1;
	is_subset = isl_basic_map_is_subset(bmap1, bmap2);
	if (is_subset != 1)
		return is_subset;
	is_subset = isl_basic_map_is_subset(bmap2, bmap1);
	return is_subset;
}

int isl_basic_set_is_equal(
		struct isl_basic_set *bset1, struct isl_basic_set *bset2)
{
	return isl_basic_map_is_equal(
		(struct isl_basic_map *)bset1, (struct isl_basic_map *)bset2);
}

int isl_map_is_empty(struct isl_map *map)
{
	int i;
	int is_empty;

	if (!map)
		return -1;
	for (i = 0; i < map->n; ++i) {
		is_empty = isl_basic_map_is_empty(map->p[i]);
		if (is_empty < 0)
			return -1;
		if (!is_empty)
			return 0;
	}
	return 1;
}

int isl_map_fast_is_empty(struct isl_map *map)
{
	return map ? map->n == 0 : -1;
}

int isl_set_fast_is_empty(struct isl_set *set)
{
	return set ? set->n == 0 : -1;
}

int isl_set_is_empty(struct isl_set *set)
{
	return isl_map_is_empty((struct isl_map *)set);
}

int isl_map_is_equal(struct isl_map *map1, struct isl_map *map2)
{
	int is_subset;

	if (!map1 || !map2)
		return -1;
	is_subset = isl_map_is_subset(map1, map2);
	if (is_subset != 1)
		return is_subset;
	is_subset = isl_map_is_subset(map2, map1);
	return is_subset;
}

int isl_basic_map_is_strict_subset(
		struct isl_basic_map *bmap1, struct isl_basic_map *bmap2)
{
	int is_subset;

	if (!bmap1 || !bmap2)
		return -1;
	is_subset = isl_basic_map_is_subset(bmap1, bmap2);
	if (is_subset != 1)
		return is_subset;
	is_subset = isl_basic_map_is_subset(bmap2, bmap1);
	if (is_subset == -1)
		return is_subset;
	return !is_subset;
}

int isl_map_is_strict_subset(struct isl_map *map1, struct isl_map *map2)
{
	int is_subset;

	if (!map1 || !map2)
		return -1;
	is_subset = isl_map_is_subset(map1, map2);
	if (is_subset != 1)
		return is_subset;
	is_subset = isl_map_is_subset(map2, map1);
	if (is_subset == -1)
		return is_subset;
	return !is_subset;
}

int isl_set_is_strict_subset(__isl_keep isl_set *set1, __isl_keep isl_set *set2)
{
	return isl_map_is_strict_subset((isl_map *)set1, (isl_map *)set2);
}

int isl_basic_map_is_universe(struct isl_basic_map *bmap)
{
	if (!bmap)
		return -1;
	return bmap->n_eq == 0 && bmap->n_ineq == 0;
}

int isl_basic_set_is_universe(struct isl_basic_set *bset)
{
	if (!bset)
		return -1;
	return bset->n_eq == 0 && bset->n_ineq == 0;
}

int isl_map_fast_is_universe(__isl_keep isl_map *map)
{
	if (!map)
		return -1;

	return map->n == 1 && isl_basic_map_is_universe(map->p[0]);
}

int isl_set_fast_is_universe(__isl_keep isl_set *set)
{
	return isl_map_fast_is_universe((isl_map *) set);
}

int isl_basic_map_is_empty(struct isl_basic_map *bmap)
{
	struct isl_basic_set *bset = NULL;
	struct isl_vec *sample = NULL;
	int empty;
	unsigned total;

	if (!bmap)
		return -1;

	if (ISL_F_ISSET(bmap, ISL_BASIC_MAP_EMPTY))
		return 1;

	if (ISL_F_ISSET(bmap, ISL_BASIC_MAP_RATIONAL)) {
		struct isl_basic_map *copy = isl_basic_map_copy(bmap);
		copy = isl_basic_map_remove_redundancies(copy);
		empty = ISL_F_ISSET(copy, ISL_BASIC_MAP_EMPTY);
		isl_basic_map_free(copy);
		return empty;
	}

	total = 1 + isl_basic_map_total_dim(bmap);
	if (bmap->sample && bmap->sample->size == total) {
		int contains = isl_basic_map_contains(bmap, bmap->sample);
		if (contains < 0)
			return -1;
		if (contains)
			return 0;
	}
	isl_vec_free(bmap->sample);
	bmap->sample = NULL;
	bset = isl_basic_map_underlying_set(isl_basic_map_copy(bmap));
	if (!bset)
		return -1;
	sample = isl_basic_set_sample_vec(bset);
	if (!sample)
		return -1;
	empty = sample->size == 0;
	isl_vec_free(bmap->sample);
	bmap->sample = sample;
	if (empty)
		ISL_F_SET(bmap, ISL_BASIC_MAP_EMPTY);

	return empty;
}

int isl_basic_map_fast_is_empty(struct isl_basic_map *bmap)
{
	if (!bmap)
		return -1;
	return ISL_F_ISSET(bmap, ISL_BASIC_MAP_EMPTY);
}

int isl_basic_set_fast_is_empty(struct isl_basic_set *bset)
{
	if (!bset)
		return -1;
	return ISL_F_ISSET(bset, ISL_BASIC_SET_EMPTY);
}

int isl_basic_set_is_empty(struct isl_basic_set *bset)
{
	return isl_basic_map_is_empty((struct isl_basic_map *)bset);
}

struct isl_map *isl_basic_map_union(
	struct isl_basic_map *bmap1, struct isl_basic_map *bmap2)
{
	struct isl_map *map;
	if (!bmap1 || !bmap2)
		return NULL;

	isl_assert(bmap1->ctx, isl_dim_equal(bmap1->dim, bmap2->dim), goto error);

	map = isl_map_alloc_dim(isl_dim_copy(bmap1->dim), 2, 0);
	if (!map)
		goto error;
	map = isl_map_add_basic_map(map, bmap1);
	map = isl_map_add_basic_map(map, bmap2);
	return map;
error:
	isl_basic_map_free(bmap1);
	isl_basic_map_free(bmap2);
	return NULL;
}

struct isl_set *isl_basic_set_union(
		struct isl_basic_set *bset1, struct isl_basic_set *bset2)
{
	return (struct isl_set *)isl_basic_map_union(
					    (struct isl_basic_map *)bset1,
					    (struct isl_basic_map *)bset2);
}

/* Order divs such that any div only depends on previous divs */
struct isl_basic_map *isl_basic_map_order_divs(struct isl_basic_map *bmap)
{
	int i;
	unsigned off;

	if (!bmap)
		return NULL;

	off = isl_dim_total(bmap->dim);

	for (i = 0; i < bmap->n_div; ++i) {
		int pos;
		if (isl_int_is_zero(bmap->div[i][0]))
			continue;
		pos = isl_seq_first_non_zero(bmap->div[i]+1+1+off+i,
							    bmap->n_div-i);
		if (pos == -1)
			continue;
		isl_basic_map_swap_div(bmap, i, i + pos);
		--i;
	}
	return bmap;
}

struct isl_basic_set *isl_basic_set_order_divs(struct isl_basic_set *bset)
{
	return (struct isl_basic_set *)
		isl_basic_map_order_divs((struct isl_basic_map *)bset);
}

__isl_give isl_map *isl_map_order_divs(__isl_take isl_map *map)
{
	int i;

	if (!map)
		return 0;

	for (i = 0; i < map->n; ++i) {
		map->p[i] = isl_basic_map_order_divs(map->p[i]);
		if (!map->p[i])
			goto error;
	}

	return map;
error:
	isl_map_free(map);
	return NULL;
}

/* Look for a div in dst that corresponds to the div "div" in src.
 * The divs before "div" in src and dst are assumed to be the same.
 * 
 * Returns -1 if no corresponding div was found and the position
 * of the corresponding div in dst otherwise.
 */
static int find_div(struct isl_basic_map *dst,
			struct isl_basic_map *src, unsigned div)
{
	int i;

	unsigned total = isl_dim_total(src->dim);

	isl_assert(dst->ctx, div <= dst->n_div, return -1);
	for (i = div; i < dst->n_div; ++i)
		if (isl_seq_eq(dst->div[i], src->div[div], 1+1+total+div) &&
		    isl_seq_first_non_zero(dst->div[i]+1+1+total+div,
						dst->n_div - div) == -1)
			return i;
	return -1;
}

struct isl_basic_map *isl_basic_map_align_divs(
		struct isl_basic_map *dst, struct isl_basic_map *src)
{
	int i;
	unsigned total = isl_dim_total(src->dim);

	if (!dst || !src)
		goto error;

	if (src->n_div == 0)
		return dst;

	for (i = 0; i < src->n_div; ++i)
		isl_assert(src->ctx, !isl_int_is_zero(src->div[i][0]), goto error);

	src = isl_basic_map_order_divs(src);
	dst = isl_basic_map_cow(dst);
	dst = isl_basic_map_extend_dim(dst, isl_dim_copy(dst->dim),
			src->n_div, 0, 2 * src->n_div);
	if (!dst)
		return NULL;
	for (i = 0; i < src->n_div; ++i) {
		int j = find_div(dst, src, i);
		if (j < 0) {
			j = isl_basic_map_alloc_div(dst);
			if (j < 0)
				goto error;
			isl_seq_cpy(dst->div[j], src->div[i], 1+1+total+i);
			isl_seq_clr(dst->div[j]+1+1+total+i, dst->n_div - i);
			if (isl_basic_map_add_div_constraints(dst, j) < 0)
				goto error;
		}
		if (j != i)
			isl_basic_map_swap_div(dst, i, j);
	}
	return dst;
error:
	isl_basic_map_free(dst);
	return NULL;
}

struct isl_basic_set *isl_basic_set_align_divs(
		struct isl_basic_set *dst, struct isl_basic_set *src)
{
	return (struct isl_basic_set *)isl_basic_map_align_divs(
		(struct isl_basic_map *)dst, (struct isl_basic_map *)src);
}

struct isl_map *isl_map_align_divs(struct isl_map *map)
{
	int i;

	if (!map)
		return NULL;
	if (map->n == 0)
		return map;
	map = isl_map_compute_divs(map);
	map = isl_map_cow(map);
	if (!map)
		return NULL;

	for (i = 1; i < map->n; ++i)
		map->p[0] = isl_basic_map_align_divs(map->p[0], map->p[i]);
	for (i = 1; i < map->n; ++i)
		map->p[i] = isl_basic_map_align_divs(map->p[i], map->p[0]);

	ISL_F_CLR(map, ISL_MAP_NORMALIZED);
	return map;
}

struct isl_set *isl_set_align_divs(struct isl_set *set)
{
	return (struct isl_set *)isl_map_align_divs((struct isl_map *)set);
}

struct isl_set *isl_set_apply(struct isl_set *set, struct isl_map *map)
{
	if (!set || !map)
		goto error;
	isl_assert(set->ctx, isl_map_compatible_domain(map, set), goto error);
	map = isl_map_intersect_domain(map, set);
	set = isl_map_range(map);
	return set;
error:
	isl_set_free(set);
	isl_map_free(map);
	return NULL;
}

/* There is no need to cow as removing empty parts doesn't change
 * the meaning of the set.
 */
struct isl_map *isl_map_remove_empty_parts(struct isl_map *map)
{
	int i;

	if (!map)
		return NULL;

	for (i = map->n-1; i >= 0; --i) {
		if (!ISL_F_ISSET(map->p[i], ISL_BASIC_MAP_EMPTY))
			continue;
		isl_basic_map_free(map->p[i]);
		if (i != map->n-1) {
			ISL_F_CLR(map, ISL_MAP_NORMALIZED);
			map->p[i] = map->p[map->n-1];
		}
		map->n--;
	}

	return map;
}

struct isl_set *isl_set_remove_empty_parts(struct isl_set *set)
{
	return (struct isl_set *)
		isl_map_remove_empty_parts((struct isl_map *)set);
}

struct isl_basic_map *isl_map_copy_basic_map(struct isl_map *map)
{
	struct isl_basic_map *bmap;
	if (!map || map->n == 0)
		return NULL;
	bmap = map->p[map->n-1];
	isl_assert(map->ctx, ISL_F_ISSET(bmap, ISL_BASIC_SET_FINAL), return NULL);
	return isl_basic_map_copy(bmap);
}

struct isl_basic_set *isl_set_copy_basic_set(struct isl_set *set)
{
	return (struct isl_basic_set *)
		isl_map_copy_basic_map((struct isl_map *)set);
}

__isl_give isl_map *isl_map_drop_basic_map(__isl_take isl_map *map,
						__isl_keep isl_basic_map *bmap)
{
	int i;

	if (!map || !bmap)
		goto error;
	for (i = map->n-1; i >= 0; --i) {
		if (map->p[i] != bmap)
			continue;
		map = isl_map_cow(map);
		if (!map)
			goto error;
		isl_basic_map_free(map->p[i]);
		if (i != map->n-1) {
			ISL_F_CLR(map, ISL_SET_NORMALIZED);
			map->p[i] = map->p[map->n-1];
		}
		map->n--;
		return map;
	}
	return map;
error:
	isl_map_free(map);
	return NULL;
}

struct isl_set *isl_set_drop_basic_set(struct isl_set *set,
						struct isl_basic_set *bset)
{
	return (struct isl_set *)isl_map_drop_basic_map((struct isl_map *)set,
						(struct isl_basic_map *)bset);
}

/* Given two basic sets bset1 and bset2, compute the maximal difference
 * between the values of dimension pos in bset1 and those in bset2
 * for any common value of the parameters and dimensions preceding pos.
 */
static enum isl_lp_result basic_set_maximal_difference_at(
	__isl_keep isl_basic_set *bset1, __isl_keep isl_basic_set *bset2,
	int pos, isl_int *opt)
{
	struct isl_dim *dims;
	struct isl_basic_map *bmap1 = NULL;
	struct isl_basic_map *bmap2 = NULL;
	struct isl_ctx *ctx;
	struct isl_vec *obj;
	unsigned total;
	unsigned nparam;
	unsigned dim1, dim2;
	enum isl_lp_result res;

	if (!bset1 || !bset2)
		return isl_lp_error;

	nparam = isl_basic_set_n_param(bset1);
	dim1 = isl_basic_set_n_dim(bset1);
	dim2 = isl_basic_set_n_dim(bset2);
	dims = isl_dim_alloc(bset1->ctx, nparam, pos, dim1 - pos);
	bmap1 = isl_basic_map_from_basic_set(isl_basic_set_copy(bset1), dims);
	dims = isl_dim_alloc(bset2->ctx, nparam, pos, dim2 - pos);
	bmap2 = isl_basic_map_from_basic_set(isl_basic_set_copy(bset2), dims);
	if (!bmap1 || !bmap2)
		goto error;
	bmap1 = isl_basic_map_cow(bmap1);
	bmap1 = isl_basic_map_extend(bmap1, nparam,
			pos, (dim1 - pos) + (dim2 - pos),
			bmap2->n_div, bmap2->n_eq, bmap2->n_ineq);
	bmap1 = add_constraints(bmap1, bmap2, 0, dim1 - pos);
	if (!bmap1)
		goto error;
	total = isl_basic_map_total_dim(bmap1);
	ctx = bmap1->ctx;
	obj = isl_vec_alloc(ctx, 1 + total);
	isl_seq_clr(obj->block.data, 1 + total);
	isl_int_set_si(obj->block.data[1+nparam+pos], 1);
	isl_int_set_si(obj->block.data[1+nparam+pos+(dim1-pos)], -1);
	if (!obj)
		goto error;
	res = isl_basic_map_solve_lp(bmap1, 1, obj->block.data, ctx->one,
					opt, NULL, NULL);
	isl_basic_map_free(bmap1);
	isl_vec_free(obj);
	return res;
error:
	isl_basic_map_free(bmap1);
	isl_basic_map_free(bmap2);
	return isl_lp_error;
}

/* Given two _disjoint_ basic sets bset1 and bset2, check whether
 * for any common value of the parameters and dimensions preceding pos
 * in both basic sets, the values of dimension pos in bset1 are
 * smaller or larger than those in bset2.
 *
 * Returns
 *	 1 if bset1 follows bset2
 *	-1 if bset1 precedes bset2
 *	 0 if bset1 and bset2 are incomparable
 *	-2 if some error occurred.
 */
int isl_basic_set_compare_at(struct isl_basic_set *bset1,
	struct isl_basic_set *bset2, int pos)
{
	isl_int opt;
	enum isl_lp_result res;
	int cmp;

	isl_int_init(opt);

	res = basic_set_maximal_difference_at(bset1, bset2, pos, &opt);

	if (res == isl_lp_empty)
		cmp = 0;
	else if ((res == isl_lp_ok && isl_int_is_pos(opt)) ||
		  res == isl_lp_unbounded)
		cmp = 1;
	else if (res == isl_lp_ok && isl_int_is_neg(opt))
		cmp = -1;
	else
		cmp = -2;

	isl_int_clear(opt);
	return cmp;
}

/* Given two basic sets bset1 and bset2, check whether
 * for any common value of the parameters and dimensions preceding pos
 * there is a value of dimension pos in bset1 that is larger
 * than a value of the same dimension in bset2.
 *
 * Return
 *	 1 if there exists such a pair
 *	 0 if there is no such pair, but there is a pair of equal values
 *	-1 otherwise
 *	-2 if some error occurred.
 */
int isl_basic_set_follows_at(__isl_keep isl_basic_set *bset1,
	__isl_keep isl_basic_set *bset2, int pos)
{
	isl_int opt;
	enum isl_lp_result res;
	int cmp;

	isl_int_init(opt);

	res = basic_set_maximal_difference_at(bset1, bset2, pos, &opt);

	if (res == isl_lp_empty)
		cmp = -1;
	else if ((res == isl_lp_ok && isl_int_is_pos(opt)) ||
		  res == isl_lp_unbounded)
		cmp = 1;
	else if (res == isl_lp_ok && isl_int_is_neg(opt))
		cmp = -1;
	else if (res == isl_lp_ok)
		cmp = 0;
	else
		cmp = -2;

	isl_int_clear(opt);
	return cmp;
}

/* Given two sets set1 and set2, check whether
 * for any common value of the parameters and dimensions preceding pos
 * there is a value of dimension pos in set1 that is larger
 * than a value of the same dimension in set2.
 *
 * Return
 *	 1 if there exists such a pair
 *	 0 if there is no such pair, but there is a pair of equal values
 *	-1 otherwise
 *	-2 if some error occurred.
 */
int isl_set_follows_at(__isl_keep isl_set *set1,
	__isl_keep isl_set *set2, int pos)
{
	int i, j;
	int follows = -1;

	if (!set1 || !set2)
		return -2;

	for (i = 0; i < set1->n; ++i)
		for (j = 0; j < set2->n; ++j) {
			int f;
			f = isl_basic_set_follows_at(set1->p[i], set2->p[j], pos);
			if (f == 1 || f == -2)
				return f;
			if (f > follows)
				follows = f;
		}

	return follows;
}

static int isl_basic_map_fast_has_fixed_var(struct isl_basic_map *bmap,
	unsigned pos, isl_int *val)
{
	int i;
	int d;
	unsigned total;

	if (!bmap)
		return -1;
	total = isl_basic_map_total_dim(bmap);
	for (i = 0, d = total-1; i < bmap->n_eq && d+1 > pos; ++i) {
		for (; d+1 > pos; --d)
			if (!isl_int_is_zero(bmap->eq[i][1+d]))
				break;
		if (d != pos)
			continue;
		if (isl_seq_first_non_zero(bmap->eq[i]+1, d) != -1)
			return 0;
		if (isl_seq_first_non_zero(bmap->eq[i]+1+d+1, total-d-1) != -1)
			return 0;
		if (!isl_int_is_one(bmap->eq[i][1+d]))
			return 0;
		if (val)
			isl_int_neg(*val, bmap->eq[i][0]);
		return 1;
	}
	return 0;
}

static int isl_map_fast_has_fixed_var(struct isl_map *map,
	unsigned pos, isl_int *val)
{
	int i;
	isl_int v;
	isl_int tmp;
	int fixed;

	if (!map)
		return -1;
	if (map->n == 0)
		return 0;
	if (map->n == 1)
		return isl_basic_map_fast_has_fixed_var(map->p[0], pos, val); 
	isl_int_init(v);
	isl_int_init(tmp);
	fixed = isl_basic_map_fast_has_fixed_var(map->p[0], pos, &v); 
	for (i = 1; fixed == 1 && i < map->n; ++i) {
		fixed = isl_basic_map_fast_has_fixed_var(map->p[i], pos, &tmp); 
		if (fixed == 1 && isl_int_ne(tmp, v))
			fixed = 0;
	}
	if (val)
		isl_int_set(*val, v);
	isl_int_clear(tmp);
	isl_int_clear(v);
	return fixed;
}

static int isl_basic_set_fast_has_fixed_var(struct isl_basic_set *bset,
	unsigned pos, isl_int *val)
{
	return isl_basic_map_fast_has_fixed_var((struct isl_basic_map *)bset,
						pos, val);
}

static int isl_set_fast_has_fixed_var(struct isl_set *set, unsigned pos,
	isl_int *val)
{
	return isl_map_fast_has_fixed_var((struct isl_map *)set, pos, val);
}

int isl_basic_map_fast_is_fixed(struct isl_basic_map *bmap,
	enum isl_dim_type type, unsigned pos, isl_int *val)
{
	if (pos >= isl_basic_map_dim(bmap, type))
		return -1;
	return isl_basic_map_fast_has_fixed_var(bmap,
		isl_basic_map_offset(bmap, type) - 1 + pos, val);
}

int isl_map_fast_is_fixed(struct isl_map *map,
	enum isl_dim_type type, unsigned pos, isl_int *val)
{
	if (pos >= isl_map_dim(map, type))
		return -1;
	return isl_map_fast_has_fixed_var(map,
		map_offset(map, type) - 1 + pos, val);
}

/* Check if dimension dim has fixed value and if so and if val is not NULL,
 * then return this fixed value in *val.
 */
int isl_basic_set_fast_dim_is_fixed(struct isl_basic_set *bset, unsigned dim,
	isl_int *val)
{
	return isl_basic_set_fast_has_fixed_var(bset,
					isl_basic_set_n_param(bset) + dim, val);
}

/* Check if dimension dim has fixed value and if so and if val is not NULL,
 * then return this fixed value in *val.
 */
int isl_set_fast_dim_is_fixed(struct isl_set *set, unsigned dim, isl_int *val)
{
	return isl_set_fast_has_fixed_var(set, isl_set_n_param(set) + dim, val);
}

/* Check if input variable in has fixed value and if so and if val is not NULL,
 * then return this fixed value in *val.
 */
int isl_map_fast_input_is_fixed(struct isl_map *map, unsigned in, isl_int *val)
{
	return isl_map_fast_has_fixed_var(map, isl_map_n_param(map) + in, val);
}

/* Check if dimension dim has an (obvious) fixed lower bound and if so
 * and if val is not NULL, then return this lower bound in *val.
 */
int isl_basic_set_fast_dim_has_fixed_lower_bound(struct isl_basic_set *bset,
	unsigned dim, isl_int *val)
{
	int i, i_eq = -1, i_ineq = -1;
	isl_int *c;
	unsigned total;
	unsigned nparam;

	if (!bset)
		return -1;
	total = isl_basic_set_total_dim(bset);
	nparam = isl_basic_set_n_param(bset);
	for (i = 0; i < bset->n_eq; ++i) {
		if (isl_int_is_zero(bset->eq[i][1+nparam+dim]))
			continue;
		if (i_eq != -1)
			return 0;
		i_eq = i;
	}
	for (i = 0; i < bset->n_ineq; ++i) {
		if (!isl_int_is_pos(bset->ineq[i][1+nparam+dim]))
			continue;
		if (i_eq != -1 || i_ineq != -1)
			return 0;
		i_ineq = i;
	}
	if (i_eq == -1 && i_ineq == -1)
		return 0;
	c = i_eq != -1 ? bset->eq[i_eq] : bset->ineq[i_ineq];
	/* The coefficient should always be one due to normalization. */
	if (!isl_int_is_one(c[1+nparam+dim]))
		return 0;
	if (isl_seq_first_non_zero(c+1, nparam+dim) != -1)
		return 0;
	if (isl_seq_first_non_zero(c+1+nparam+dim+1,
					total - nparam - dim - 1) != -1)
		return 0;
	if (val)
		isl_int_neg(*val, c[0]);
	return 1;
}

int isl_set_fast_dim_has_fixed_lower_bound(struct isl_set *set,
	unsigned dim, isl_int *val)
{
	int i;
	isl_int v;
	isl_int tmp;
	int fixed;

	if (!set)
		return -1;
	if (set->n == 0)
		return 0;
	if (set->n == 1)
		return isl_basic_set_fast_dim_has_fixed_lower_bound(set->p[0],
								dim, val);
	isl_int_init(v);
	isl_int_init(tmp);
	fixed = isl_basic_set_fast_dim_has_fixed_lower_bound(set->p[0],
								dim, &v);
	for (i = 1; fixed == 1 && i < set->n; ++i) {
		fixed = isl_basic_set_fast_dim_has_fixed_lower_bound(set->p[i],
								dim, &tmp);
		if (fixed == 1 && isl_int_ne(tmp, v))
			fixed = 0;
	}
	if (val)
		isl_int_set(*val, v);
	isl_int_clear(tmp);
	isl_int_clear(v);
	return fixed;
}

struct constraint {
	unsigned	size;
	isl_int		*c;
};

static int qsort_constraint_cmp(const void *p1, const void *p2)
{
	const struct constraint *c1 = (const struct constraint *)p1;
	const struct constraint *c2 = (const struct constraint *)p2;
	unsigned size = isl_min(c1->size, c2->size);
	return isl_seq_cmp(c1->c, c2->c, size);
}

static struct isl_basic_map *isl_basic_map_sort_constraints(
	struct isl_basic_map *bmap)
{
	int i;
	struct constraint *c;
	unsigned total;

	if (!bmap)
		return NULL;
	total = isl_basic_map_total_dim(bmap);
	c = isl_alloc_array(bmap->ctx, struct constraint, bmap->n_ineq);
	if (!c)
		goto error;
	for (i = 0; i < bmap->n_ineq; ++i) {
		c[i].size = total;
		c[i].c = bmap->ineq[i];
	}
	qsort(c, bmap->n_ineq, sizeof(struct constraint), qsort_constraint_cmp);
	for (i = 0; i < bmap->n_ineq; ++i)
		bmap->ineq[i] = c[i].c;
	free(c);
	return bmap;
error:
	isl_basic_map_free(bmap);
	return NULL;
}

__isl_give isl_basic_set *isl_basic_set_sort_constraints(
	__isl_take isl_basic_set *bset)
{
	return (struct isl_basic_set *)isl_basic_map_sort_constraints(
						(struct isl_basic_map *)bset);
}

struct isl_basic_map *isl_basic_map_normalize(struct isl_basic_map *bmap)
{
	if (!bmap)
		return NULL;
	if (ISL_F_ISSET(bmap, ISL_BASIC_MAP_NORMALIZED))
		return bmap;
	bmap = isl_basic_map_remove_redundancies(bmap);
	bmap = isl_basic_map_sort_constraints(bmap);
	ISL_F_SET(bmap, ISL_BASIC_MAP_NORMALIZED);
	return bmap;
}

struct isl_basic_set *isl_basic_set_normalize(struct isl_basic_set *bset)
{
	return (struct isl_basic_set *)isl_basic_map_normalize(
						(struct isl_basic_map *)bset);
}

int isl_basic_map_fast_cmp(const __isl_keep isl_basic_map *bmap1,
	const __isl_keep isl_basic_map *bmap2)
{
	int i, cmp;
	unsigned total;

	if (bmap1 == bmap2)
		return 0;
	if (isl_basic_map_n_param(bmap1) != isl_basic_map_n_param(bmap2))
		return isl_basic_map_n_param(bmap1) - isl_basic_map_n_param(bmap2);
	if (isl_basic_map_n_in(bmap1) != isl_basic_map_n_in(bmap2))
		return isl_basic_map_n_out(bmap1) - isl_basic_map_n_out(bmap2);
	if (isl_basic_map_n_out(bmap1) != isl_basic_map_n_out(bmap2))
		return isl_basic_map_n_out(bmap1) - isl_basic_map_n_out(bmap2);
	if (ISL_F_ISSET(bmap1, ISL_BASIC_MAP_EMPTY) &&
	    ISL_F_ISSET(bmap2, ISL_BASIC_MAP_EMPTY))
		return 0;
	if (ISL_F_ISSET(bmap1, ISL_BASIC_MAP_EMPTY))
		return 1;
	if (ISL_F_ISSET(bmap2, ISL_BASIC_MAP_EMPTY))
		return -1;
	if (bmap1->n_eq != bmap2->n_eq)
		return bmap1->n_eq - bmap2->n_eq;
	if (bmap1->n_ineq != bmap2->n_ineq)
		return bmap1->n_ineq - bmap2->n_ineq;
	if (bmap1->n_div != bmap2->n_div)
		return bmap1->n_div - bmap2->n_div;
	total = isl_basic_map_total_dim(bmap1);
	for (i = 0; i < bmap1->n_eq; ++i) {
		cmp = isl_seq_cmp(bmap1->eq[i], bmap2->eq[i], 1+total);
		if (cmp)
			return cmp;
	}
	for (i = 0; i < bmap1->n_ineq; ++i) {
		cmp = isl_seq_cmp(bmap1->ineq[i], bmap2->ineq[i], 1+total);
		if (cmp)
			return cmp;
	}
	for (i = 0; i < bmap1->n_div; ++i) {
		cmp = isl_seq_cmp(bmap1->div[i], bmap2->div[i], 1+1+total);
		if (cmp)
			return cmp;
	}
	return 0;
}

int isl_basic_map_fast_is_equal(__isl_keep isl_basic_map *bmap1,
	__isl_keep isl_basic_map *bmap2)
{
	return isl_basic_map_fast_cmp(bmap1, bmap2) == 0;
}

int isl_basic_set_fast_is_equal(__isl_keep isl_basic_set *bset1,
	__isl_keep isl_basic_set *bset2)
{
	return isl_basic_map_fast_is_equal((isl_basic_map *)bset1,
					    (isl_basic_map *)bset2);
}

static int qsort_bmap_cmp(const void *p1, const void *p2)
{
	const struct isl_basic_map *bmap1 = *(const struct isl_basic_map **)p1;
	const struct isl_basic_map *bmap2 = *(const struct isl_basic_map **)p2;

	return isl_basic_map_fast_cmp(bmap1, bmap2);
}

/* We normalize in place, but if anything goes wrong we need
 * to return NULL, so we need to make sure we don't change the
 * meaning of any possible other copies of map.
 */
struct isl_map *isl_map_normalize(struct isl_map *map)
{
	int i, j;
	struct isl_basic_map *bmap;

	if (!map)
		return NULL;
	if (ISL_F_ISSET(map, ISL_MAP_NORMALIZED))
		return map;
	for (i = 0; i < map->n; ++i) {
		bmap = isl_basic_map_normalize(isl_basic_map_copy(map->p[i]));
		if (!bmap)
			goto error;
		isl_basic_map_free(map->p[i]);
		map->p[i] = bmap;
	}
	qsort(map->p, map->n, sizeof(struct isl_basic_map *), qsort_bmap_cmp);
	ISL_F_SET(map, ISL_MAP_NORMALIZED);
	map = isl_map_remove_empty_parts(map);
	if (!map)
		return NULL;
	for (i = map->n - 1; i >= 1; --i) {
		if (!isl_basic_map_fast_is_equal(map->p[i-1], map->p[i]))
			continue;
		isl_basic_map_free(map->p[i-1]);
		for (j = i; j < map->n; ++j)
			map->p[j-1] = map->p[j];
		map->n--;
	}
	return map;
error:
	isl_map_free(map);
	return NULL;

}

struct isl_set *isl_set_normalize(struct isl_set *set)
{
	return (struct isl_set *)isl_map_normalize((struct isl_map *)set);
}

int isl_map_fast_is_equal(struct isl_map *map1, struct isl_map *map2)
{
	int i;
	int equal;

	if (!map1 || !map2)
		return -1;

	if (map1 == map2)
		return 1;
	if (!isl_dim_equal(map1->dim, map2->dim))
		return 0;

	map1 = isl_map_copy(map1);
	map2 = isl_map_copy(map2);
	map1 = isl_map_normalize(map1);
	map2 = isl_map_normalize(map2);
	if (!map1 || !map2)
		goto error;
	equal = map1->n == map2->n;
	for (i = 0; equal && i < map1->n; ++i) {
		equal = isl_basic_map_fast_is_equal(map1->p[i], map2->p[i]);
		if (equal < 0)
			goto error;
	}
	isl_map_free(map1);
	isl_map_free(map2);
	return equal;
error:
	isl_map_free(map1);
	isl_map_free(map2);
	return -1;
}

int isl_set_fast_is_equal(struct isl_set *set1, struct isl_set *set2)
{
	return isl_map_fast_is_equal((struct isl_map *)set1,
						(struct isl_map *)set2);
}

/* Return an interval that ranges from min to max (inclusive)
 */
struct isl_basic_set *isl_basic_set_interval(struct isl_ctx *ctx,
	isl_int min, isl_int max)
{
	int k;
	struct isl_basic_set *bset = NULL;

	bset = isl_basic_set_alloc(ctx, 0, 1, 0, 0, 2);
	if (!bset)
		goto error;

	k = isl_basic_set_alloc_inequality(bset);
	if (k < 0)
		goto error;
	isl_int_set_si(bset->ineq[k][1], 1);
	isl_int_neg(bset->ineq[k][0], min);

	k = isl_basic_set_alloc_inequality(bset);
	if (k < 0)
		goto error;
	isl_int_set_si(bset->ineq[k][1], -1);
	isl_int_set(bset->ineq[k][0], max);

	return bset;
error:
	isl_basic_set_free(bset);
	return NULL;
}

/* Return the Cartesian product of the basic sets in list (in the given order).
 */
struct isl_basic_set *isl_basic_set_product(struct isl_basic_set_list *list)
{
	int i;
	unsigned dim;
	unsigned nparam;
	unsigned extra;
	unsigned n_eq;
	unsigned n_ineq;
	struct isl_basic_set *product = NULL;

	if (!list)
		goto error;
	isl_assert(list->ctx, list->n > 0, goto error);
	isl_assert(list->ctx, list->p[0], goto error);
	nparam = isl_basic_set_n_param(list->p[0]);
	dim = isl_basic_set_n_dim(list->p[0]);
	extra = list->p[0]->n_div;
	n_eq = list->p[0]->n_eq;
	n_ineq = list->p[0]->n_ineq;
	for (i = 1; i < list->n; ++i) {
		isl_assert(list->ctx, list->p[i], goto error);
		isl_assert(list->ctx,
		    nparam == isl_basic_set_n_param(list->p[i]), goto error);
		dim += isl_basic_set_n_dim(list->p[i]);
		extra += list->p[i]->n_div;
		n_eq += list->p[i]->n_eq;
		n_ineq += list->p[i]->n_ineq;
	}
	product = isl_basic_set_alloc(list->ctx, nparam, dim, extra,
					n_eq, n_ineq);
	if (!product)
		goto error;
	dim = 0;
	for (i = 0; i < list->n; ++i) {
		isl_basic_set_add_constraints(product,
					isl_basic_set_copy(list->p[i]), dim);
		dim += isl_basic_set_n_dim(list->p[i]);
	}
	isl_basic_set_list_free(list);
	return product;
error:
	isl_basic_set_free(product);
	isl_basic_set_list_free(list);
	return NULL;
}

struct isl_basic_map *isl_basic_map_product(
		struct isl_basic_map *bmap1, struct isl_basic_map *bmap2)
{
	struct isl_dim *dim_result = NULL;
	struct isl_basic_map *bmap;
	unsigned in1, in2, out1, out2, nparam, total, pos;
	struct isl_dim_map *dim_map1, *dim_map2;

	if (!bmap1 || !bmap2)
		goto error;

	isl_assert(bmap1->ctx, isl_dim_match(bmap1->dim, isl_dim_param,
				     bmap2->dim, isl_dim_param), goto error);
	dim_result = isl_dim_product(isl_dim_copy(bmap1->dim),
						   isl_dim_copy(bmap2->dim));

	in1 = isl_basic_map_n_in(bmap1);
	in2 = isl_basic_map_n_in(bmap2);
	out1 = isl_basic_map_n_out(bmap1);
	out2 = isl_basic_map_n_out(bmap2);
	nparam = isl_basic_map_n_param(bmap1);

	total = nparam + in1 + in2 + out1 + out2 + bmap1->n_div + bmap2->n_div;
	dim_map1 = isl_dim_map_alloc(bmap1->ctx, total);
	dim_map2 = isl_dim_map_alloc(bmap1->ctx, total);
	isl_dim_map_dim(dim_map1, bmap1->dim, isl_dim_param, pos = 0);
	isl_dim_map_dim(dim_map2, bmap2->dim, isl_dim_param, pos = 0);
	isl_dim_map_dim(dim_map1, bmap1->dim, isl_dim_in, pos += nparam);
	isl_dim_map_dim(dim_map2, bmap2->dim, isl_dim_in, pos += in1);
	isl_dim_map_dim(dim_map1, bmap1->dim, isl_dim_out, pos += in2);
	isl_dim_map_dim(dim_map2, bmap2->dim, isl_dim_out, pos += out1);
	isl_dim_map_div(dim_map1, bmap1, pos += out2);
	isl_dim_map_div(dim_map2, bmap2, pos += bmap1->n_div);

	bmap = isl_basic_map_alloc_dim(dim_result,
			bmap1->n_div + bmap2->n_div,
			bmap1->n_eq + bmap2->n_eq,
			bmap1->n_ineq + bmap2->n_ineq);
	bmap = add_constraints_dim_map(bmap, bmap1, dim_map1);
	bmap = add_constraints_dim_map(bmap, bmap2, dim_map2);
	bmap = isl_basic_map_simplify(bmap);
	return isl_basic_map_finalize(bmap);
error:
	isl_basic_map_free(bmap1);
	isl_basic_map_free(bmap2);
	return NULL;
}

/* Given two maps A -> B and C -> D, construct a map (A, C) -> (B, D)
 */
struct isl_map *isl_map_product(struct isl_map *map1, struct isl_map *map2)
{
	unsigned flags = 0;
	struct isl_map *result;
	int i, j;

	if (!map1 || !map2)
		goto error;

	isl_assert(map1->ctx, isl_dim_match(map1->dim, isl_dim_param,
					 map2->dim, isl_dim_param), goto error);

	if (ISL_F_ISSET(map1, ISL_MAP_DISJOINT) &&
	    ISL_F_ISSET(map2, ISL_MAP_DISJOINT))
		ISL_FL_SET(flags, ISL_MAP_DISJOINT);

	result = isl_map_alloc_dim(isl_dim_product(isl_dim_copy(map1->dim),
						   isl_dim_copy(map2->dim)),
				map1->n * map2->n, flags);
	if (!result)
		goto error;
	for (i = 0; i < map1->n; ++i)
		for (j = 0; j < map2->n; ++j) {
			struct isl_basic_map *part;
			part = isl_basic_map_product(
				    isl_basic_map_copy(map1->p[i]),
				    isl_basic_map_copy(map2->p[j]));
			if (isl_basic_map_is_empty(part))
				isl_basic_map_free(part);
			else
				result = isl_map_add_basic_map(result, part);
			if (!result)
				goto error;
		}
	isl_map_free(map1);
	isl_map_free(map2);
	return result;
error:
	isl_map_free(map1);
	isl_map_free(map2);
	return NULL;
}

/* Given two set A and B, construct its Cartesian product A x B.
 */
struct isl_set *isl_set_product(struct isl_set *set1, struct isl_set *set2)
{
	return (struct isl_set *)isl_map_product((struct isl_map *)set1,
						 (struct isl_map *)set2);
}

uint32_t isl_basic_set_get_hash(struct isl_basic_set *bset)
{
	int i;
	uint32_t hash = isl_hash_init();
	unsigned total;

	if (!bset)
		return 0;
	bset = isl_basic_set_copy(bset);
	bset = isl_basic_set_normalize(bset);
	if (!bset)
		return 0;
	total = isl_basic_set_total_dim(bset);
	isl_hash_byte(hash, bset->n_eq & 0xFF);
	for (i = 0; i < bset->n_eq; ++i) {
		uint32_t c_hash;
		c_hash = isl_seq_get_hash(bset->eq[i], 1 + total);
		isl_hash_hash(hash, c_hash);
	}
	isl_hash_byte(hash, bset->n_ineq & 0xFF);
	for (i = 0; i < bset->n_ineq; ++i) {
		uint32_t c_hash;
		c_hash = isl_seq_get_hash(bset->ineq[i], 1 + total);
		isl_hash_hash(hash, c_hash);
	}
	isl_hash_byte(hash, bset->n_div & 0xFF);
	for (i = 0; i < bset->n_div; ++i) {
		uint32_t c_hash;
		if (isl_int_is_zero(bset->div[i][0]))
			continue;
		isl_hash_byte(hash, i & 0xFF);
		c_hash = isl_seq_get_hash(bset->div[i], 1 + 1 + total);
		isl_hash_hash(hash, c_hash);
	}
	isl_basic_set_free(bset);
	return hash;
}

uint32_t isl_set_get_hash(struct isl_set *set)
{
	int i;
	uint32_t hash;

	if (!set)
		return 0;
	set = isl_set_copy(set);
	set = isl_set_normalize(set);
	if (!set)
		return 0;

	hash = isl_hash_init();
	for (i = 0; i < set->n; ++i) {
		uint32_t bset_hash;
		bset_hash = isl_basic_set_get_hash(set->p[i]);
		isl_hash_hash(hash, bset_hash);
	}
		
	isl_set_free(set);

	return hash;
}

/* Check if the value for dimension dim is completely determined
 * by the values of the other parameters and variables.
 * That is, check if dimension dim is involved in an equality.
 */
int isl_basic_set_dim_is_unique(struct isl_basic_set *bset, unsigned dim)
{
	int i;
	unsigned nparam;

	if (!bset)
		return -1;
	nparam = isl_basic_set_n_param(bset);
	for (i = 0; i < bset->n_eq; ++i)
		if (!isl_int_is_zero(bset->eq[i][1 + nparam + dim]))
			return 1;
	return 0;
}

/* Check if the value for dimension dim is completely determined
 * by the values of the other parameters and variables.
 * That is, check if dimension dim is involved in an equality
 * for each of the subsets.
 */
int isl_set_dim_is_unique(struct isl_set *set, unsigned dim)
{
	int i;

	if (!set)
		return -1;
	for (i = 0; i < set->n; ++i) {
		int unique;
		unique = isl_basic_set_dim_is_unique(set->p[i], dim);
		if (unique != 1)
			return unique;
	}
	return 1;
}

int isl_map_foreach_basic_map(__isl_keep isl_map *map,
	int (*fn)(__isl_take isl_basic_map *bmap, void *user), void *user)
{
	int i;

	if (!map)
		return -1;

	for (i = 0; i < map->n; ++i)
		if (fn(isl_basic_map_copy(map->p[i]), user) < 0)
			return -1;

	return 0;
}

int isl_set_foreach_basic_set(__isl_keep isl_set *set,
	int (*fn)(__isl_take isl_basic_set *bset, void *user), void *user)
{
	int i;

	if (!set)
		return -1;

	for (i = 0; i < set->n; ++i)
		if (fn(isl_basic_set_copy(set->p[i]), user) < 0)
			return -1;

	return 0;
}

__isl_give isl_basic_set *isl_basic_set_lift(__isl_take isl_basic_set *bset)
{
	struct isl_dim *dim;

	if (!bset)
		return NULL;

	if (bset->n_div == 0)
		return bset;

	bset = isl_basic_set_cow(bset);
	if (!bset)
		return NULL;

	dim = isl_basic_set_get_dim(bset);
	dim = isl_dim_add(dim, isl_dim_set, bset->n_div);
	if (!dim)
		goto error;
	isl_dim_free(bset->dim);
	bset->dim = dim;
	bset->n_div = 0;

	bset = isl_basic_set_finalize(bset);

	return bset;
error:
	isl_basic_set_free(bset);
	return NULL;
}

__isl_give isl_set *isl_set_lift(__isl_take isl_set *set)
{
	int i;
	struct isl_dim *dim;
	unsigned n_div;

	set = isl_set_align_divs(set);

	if (!set)
		return NULL;
	if (set->n == 0 || set->p[0]->n_div == 0)
		return set;

	set = isl_set_cow(set);
	if (!set)
		return NULL;

	n_div = set->p[0]->n_div;
	dim = isl_set_get_dim(set);
	dim = isl_dim_add(dim, isl_dim_set, n_div);
	if (!dim)
		goto error;
	isl_dim_free(set->dim);
	set->dim = dim;

	for (i = 0; i < set->n; ++i) {
		set->p[i] = isl_basic_set_lift(set->p[i]);
		if (!set->p[i])
			goto error;
	}

	return set;
error:
	isl_set_free(set);
	return NULL;
}

__isl_give isl_map *isl_set_lifting(__isl_take isl_set *set)
{
	struct isl_dim *dim;
	struct isl_basic_map *bmap;
	unsigned n_set;
	unsigned n_div;
	unsigned n_param;
	unsigned total;
	int i, k, l;

	set = isl_set_align_divs(set);

	if (!set)
		return NULL;

	dim = isl_set_get_dim(set);
	if (set->n == 0 || set->p[0]->n_div == 0) {
		isl_set_free(set);
		return isl_map_identity(dim);
	}

	n_div = set->p[0]->n_div;
	dim = isl_dim_map(dim);
	n_param = isl_dim_size(dim, isl_dim_param);
	n_set = isl_dim_size(dim, isl_dim_in);
	dim = isl_dim_extend(dim, n_param, n_set, n_set + n_div);
	bmap = isl_basic_map_alloc_dim(dim, 0, n_set, 2 * n_div);
	for (i = 0; i < n_set; ++i)
		bmap = var_equal(bmap, i);

	total = n_param + n_set + n_set + n_div;
	for (i = 0; i < n_div; ++i) {
		k = isl_basic_map_alloc_inequality(bmap);
		if (k < 0)
			goto error;
		isl_seq_cpy(bmap->ineq[k], set->p[0]->div[i]+1, 1+n_param);
		isl_seq_clr(bmap->ineq[k]+1+n_param, n_set);
		isl_seq_cpy(bmap->ineq[k]+1+n_param+n_set,
			    set->p[0]->div[i]+1+1+n_param, n_set + n_div);
		isl_int_neg(bmap->ineq[k][1+n_param+n_set+n_set+i],
			    set->p[0]->div[i][0]);

		l = isl_basic_map_alloc_inequality(bmap);
		if (l < 0)
			goto error;
		isl_seq_neg(bmap->ineq[l], bmap->ineq[k], 1 + total);
		isl_int_add(bmap->ineq[l][0], bmap->ineq[l][0],
			    set->p[0]->div[i][0]);
		isl_int_sub_ui(bmap->ineq[l][0], bmap->ineq[l][0], 1);
	}

	isl_set_free(set);
	return isl_map_from_basic_map(bmap);
error:
	isl_set_free(set);
	isl_basic_map_free(bmap);
	return NULL;
}

int isl_basic_set_size(__isl_keep isl_basic_set *bset)
{
	unsigned dim;
	int size = 0;

	if (!bset)
		return -1;

	dim = isl_basic_set_total_dim(bset);
	size += bset->n_eq * (1 + dim);
	size += bset->n_ineq * (1 + dim);
	size += bset->n_div * (2 + dim);

	return size;
}

int isl_set_size(__isl_keep isl_set *set)
{
	int i;
	int size = 0;

	if (!set)
		return -1;

	for (i = 0; i < set->n; ++i)
		size += isl_basic_set_size(set->p[i]);

	return size;
}

int isl_basic_map_dim_is_bounded(__isl_keep isl_basic_map *bmap,
	enum isl_dim_type type, unsigned pos)
{
	int i;
	int lower, upper;

	if (!bmap)
		return -1;

	isl_assert(bmap->ctx, pos < isl_basic_map_dim(bmap, type), return -1);

	pos += isl_basic_map_offset(bmap, type);

	for (i = 0; i < bmap->n_eq; ++i)
		if (!isl_int_is_zero(bmap->eq[i][pos]))
			return 1;

	lower = upper = 0;
	for (i = 0; i < bmap->n_ineq; ++i) {
		int sgn = isl_int_sgn(bmap->ineq[i][pos]);
		if (sgn > 0)
			lower = 1;
		if (sgn < 0)
			upper = 1;
	}

	return lower && upper;
}

int isl_map_dim_is_bounded(__isl_keep isl_map *map,
	enum isl_dim_type type, unsigned pos)
{
	int i;

	if (!map)
		return -1;

	for (i = 0; i < map->n; ++i) {
		int bounded;
		bounded = isl_basic_map_dim_is_bounded(map->p[i], type, pos);
		if (bounded < 0 || !bounded)
			return bounded;
	}

	return 1;
}

/* Return 1 if the specified dim is involved in both an upper bound
 * and a lower bound.
 */
int isl_set_dim_is_bounded(__isl_keep isl_set *set,
	enum isl_dim_type type, unsigned pos)
{
	return isl_map_dim_is_bounded((isl_map *)set, type, pos);
}

/* For each of the "n" variables starting at "first", determine
 * the sign of the variable and put the results in the first "n"
 * elements of the array "signs".
 * Sign
 *	1 means that the variable is non-negative
 *	-1 means that the variable is non-positive
 *	0 means the variable attains both positive and negative values.
 */
int isl_basic_set_vars_get_sign(__isl_keep isl_basic_set *bset,
	unsigned first, unsigned n, int *signs)
{
	isl_vec *bound = NULL;
	struct isl_tab *tab = NULL;
	struct isl_tab_undo *snap;
	int i;

	if (!bset || !signs)
		return -1;

	bound = isl_vec_alloc(bset->ctx, 1 + isl_basic_set_total_dim(bset));
	tab = isl_tab_from_basic_set(bset);
	if (!bound || !tab)
		goto error;

	isl_seq_clr(bound->el, bound->size);
	isl_int_set_si(bound->el[0], -1);

	snap = isl_tab_snap(tab);
	for (i = 0; i < n; ++i) {
		int empty;

		isl_int_set_si(bound->el[1 + first + i], -1);
		if (isl_tab_add_ineq(tab, bound->el) < 0)
			goto error;
		empty = tab->empty;
		isl_int_set_si(bound->el[1 + first + i], 0);
		if (isl_tab_rollback(tab, snap) < 0)
			goto error;

		if (empty) {
			signs[i] = 1;
			continue;
		}

		isl_int_set_si(bound->el[1 + first + i], 1);
		if (isl_tab_add_ineq(tab, bound->el) < 0)
			goto error;
		empty = tab->empty;
		isl_int_set_si(bound->el[1 + first + i], 0);
		if (isl_tab_rollback(tab, snap) < 0)
			goto error;

		signs[i] = empty ? -1 : 0;
	}

	isl_tab_free(tab);
	isl_vec_free(bound);
	return 0;
error:
	isl_tab_free(tab);
	isl_vec_free(bound);
	return -1;
}

int isl_basic_set_dims_get_sign(__isl_keep isl_basic_set *bset,
	enum isl_dim_type type, unsigned first, unsigned n, int *signs)
{
	if (!bset || !signs)
		return -1;
	isl_assert(bset->ctx, first + n <= isl_basic_set_dim(bset, type),
		return -1);

	first += pos(bset->dim, type) - 1;
	return isl_basic_set_vars_get_sign(bset, first, n, signs);
}

/* Check if the given map is single-valued.
 * We simply compute
 *
 *	M \circ M^-1
 *
 * and check if the result is a subset of the identity mapping.
 */
int isl_map_is_single_valued(__isl_keep isl_map *map)
{
	isl_map *test;
	isl_map *id;
	int sv;

	test = isl_map_reverse(isl_map_copy(map));
	test = isl_map_apply_range(test, isl_map_copy(map));

	id = isl_map_identity(isl_dim_range(isl_map_get_dim(map)));

	sv = isl_map_is_subset(test, id);

	isl_map_free(test);
	isl_map_free(id);

	return sv;
}

int isl_map_is_bijective(__isl_keep isl_map *map)
{
	int sv;

	sv = isl_map_is_single_valued(map);
	if (sv < 0 || !sv)
		return sv;

	map = isl_map_copy(map);
	map = isl_map_reverse(map);
	sv = isl_map_is_single_valued(map);
	isl_map_free(map);

	return sv;
}

int isl_set_is_singleton(__isl_keep isl_set *set)
{
	return isl_map_is_single_valued((isl_map *)set);
}

int isl_map_is_translation(__isl_keep isl_map *map)
{
	int ok;
	isl_set *delta;

	delta = isl_map_deltas(isl_map_copy(map));
	ok = isl_set_is_singleton(delta);
	isl_set_free(delta);

	return ok;
}

static int unique(isl_int *p, unsigned pos, unsigned len)
{
	if (isl_seq_first_non_zero(p, pos) != -1)
		return 0;
	if (isl_seq_first_non_zero(p + pos + 1, len - pos - 1) != -1)
		return 0;
	return 1;
}

int isl_basic_set_is_box(__isl_keep isl_basic_set *bset)
{
	int i, j;
	unsigned nvar;
	unsigned ovar;

	if (!bset)
		return -1;

	if (isl_basic_set_dim(bset, isl_dim_div) != 0)
		return 0;

	nvar = isl_basic_set_dim(bset, isl_dim_set);
	ovar = isl_dim_offset(bset->dim, isl_dim_set);
	for (j = 0; j < nvar; ++j) {
		int lower = 0, upper = 0;
		for (i = 0; i < bset->n_eq; ++i) {
			if (isl_int_is_zero(bset->eq[i][1 + ovar + j]))
				continue;
			if (!unique(bset->eq[i] + 1 + ovar, j, nvar))
				return 0;
			break;
		}
		if (i < bset->n_eq)
			continue;
		for (i = 0; i < bset->n_ineq; ++i) {
			if (isl_int_is_zero(bset->ineq[i][1 + ovar + j]))
				continue;
			if (!unique(bset->ineq[i] + 1 + ovar, j, nvar))
				return 0;
			if (isl_int_is_pos(bset->ineq[i][1 + ovar + j]))
				lower = 1;
			else
				upper = 1;
		}
		if (!lower || !upper)
			return 0;
	}

	return 1;
}

int isl_set_is_box(__isl_keep isl_set *set)
{
	if (!set)
		return -1;
	if (set->n != 1)
		return 0;

	return isl_basic_set_is_box(set->p[0]);
}
