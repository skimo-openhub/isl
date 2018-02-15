#ifndef ISL_POINT_PRIVATE_H
#define ISL_POINT_PRIVATE_H

#include <isl/space.h>
#include <isl/point.h>
#include <isl/vec.h>

struct isl_point {
	int		ref;
	isl_space	*dim;
	struct isl_vec	*vec;
};

__isl_give isl_point *isl_point_alloc(__isl_take isl_space *dim,
	__isl_take isl_vec *vec);

__isl_keep isl_space *isl_point_peek_space(__isl_keep isl_point *pnt);

#endif
