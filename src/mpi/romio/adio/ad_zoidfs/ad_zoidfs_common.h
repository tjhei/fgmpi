/* -*- Mode: C; c-basic-offset:4 ; -*-
 * vim: ts=8 sts=4 sw=4 noexpandtab
 *
 *   Copyright (C) 1997 University of Chicago. 
 *   See COPYRIGHT notice in top-level directory.
 */

#ifndef _AD_ZOIDFS_COMMON_H
#define _AD_ZOIDFS_COMMON_H
#include "ad_zoidfs.h"


void ADIOI_ZOIDFS_Init(int rank, int *error_code );
void ADIOI_ZOIDFS_makeattribs(zoidfs_sattr_t * attribs);
void ADIOI_ZOIDFS_End(int *error_code);
int ADIOI_ZOIDFS_End_call(MPI_Comm comm, int keyval, 
	void *attribute_val, void *extra_state);
int ADIOI_ZOIDFS_error_convert(int error);

#endif
