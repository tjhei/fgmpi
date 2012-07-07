/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "mpiimpl.h"

/* -- Begin Profiling Symbol Block for routine MPIX_Type_create_hindexed_block */
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPIX_Type_create_hindexed_block = PMPIX_Type_create_hindexed_block
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPIX_Type_create_hindexed_block  MPIX_Type_create_hindexed_block
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPIX_Type_create_hindexed_block as PMPIX_Type_create_hindexed_block
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
   the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef MPIX_Type_create_hindexed_block
#define MPIX_Type_create_hindexed_block PMPIX_Type_create_hindexed_block

#undef FUNCNAME
#define FUNCNAME MPIR_Type_create_hindexed_block_impl
#undef FCNAME
#define FCNAME MPIU_QUOTE(FUNCNAME)
int MPIR_Type_create_hindexed_block_impl(int count, int blocklength,
                                         const MPI_Aint array_of_displacements[],
                                         MPI_Datatype oldtype, MPI_Datatype * newtype)
{
    int mpi_errno = MPI_SUCCESS;
    MPIU_CHKLMEM_DECL(1);
    MPI_Datatype new_handle;
    MPID_Datatype *new_dtp;
    int i, *ints;

    mpi_errno = MPID_Type_blockindexed(count, blocklength, array_of_displacements, 1,
                                       oldtype, &new_handle);
    if (mpi_errno)
        MPIU_ERR_POP(mpi_errno);

    MPIU_CHKLMEM_MALLOC_ORJUMP(ints, int *, (count + 2) * sizeof(int), mpi_errno,
                               "content description");

    ints[0] = count;
    ints[1] = blocklength;

    for (i = 0; i < count; i++)
        ints[i + 2] = array_of_displacements[i];

    MPID_Datatype_get_ptr(new_handle, new_dtp);
    mpi_errno = MPID_Datatype_set_contents(new_dtp, MPIX_COMBINER_HINDEXED_BLOCK, count + 2,
                                           0, 1, ints, NULL, &oldtype);
    if (mpi_errno)
        MPIU_ERR_POP(mpi_errno);

    MPIU_OBJ_PUBLISH_HANDLE(*newtype, new_handle);

  fn_exit:
    MPIU_CHKLMEM_FREEALL();
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}


#endif

#undef FUNCNAME
#define FUNCNAME MPIX_Type_create_hindexed_block
#undef FCNAME
#define FCNAME MPIU_QUOTE(FUNCNAME)
/*@
   MPIX_Type_create_hindexed_block - Create an hindexed
     datatype with constant-sized blocks

   Input Parameters:
+ count - length of array of displacements (integer)
. blocklength - size of block (integer)
. array_of_displacements - array of displacements (array of integer)
- oldtype - old datatype (handle)

    Output Parameter:
. newtype - new datatype (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_ARG
@*/
int MPIX_Type_create_hindexed_block(int count,
                                    int blocklength,
                                    const MPI_Aint array_of_displacements[],
                                    MPI_Datatype oldtype, MPI_Datatype * newtype)
{
    int mpi_errno = MPI_SUCCESS;
    MPID_MPI_STATE_DECL(MPID_STATE_MPI_TYPE_CREATE_HINDEXED_BLOCK);

    MPIR_ERRTEST_INITIALIZED_ORDIE();

    MPIU_THREAD_CS_ENTER(ALLFUNC,);
    MPID_MPI_FUNC_ENTER(MPID_STATE_MPI_TYPE_CREATE_HINDEXED_BLOCK);

    /* Validate parameters and objects */
#ifdef HAVE_ERROR_CHECKING
    {
        MPID_BEGIN_ERROR_CHECKS;
        {
            MPID_Datatype *datatype_ptr = NULL;

            MPIR_ERRTEST_COUNT(count, mpi_errno);
            MPIR_ERRTEST_ARGNEG(blocklength, "blocklen", mpi_errno);
            if (count > 0) {
                MPIR_ERRTEST_ARGNULL(array_of_displacements, "indices", mpi_errno);
            }
            MPIR_ERRTEST_DATATYPE(oldtype, "datatype", mpi_errno);
            if (mpi_errno)
                goto fn_fail;

            if (HANDLE_GET_KIND(oldtype) != HANDLE_KIND_BUILTIN) {
                MPID_Datatype_get_ptr(oldtype, datatype_ptr);
                MPID_Datatype_valid_ptr(datatype_ptr, mpi_errno);
            }

            if (mpi_errno)
                goto fn_fail;
        }
        MPID_END_ERROR_CHECKS;
    }
#endif /* HAVE_ERROR_CHECKING */

    /* ... body of routine ... */

    mpi_errno =
        MPIR_Type_create_hindexed_block_impl(count, blocklength, array_of_displacements,
                                             oldtype, newtype);
    if (mpi_errno)
        goto fn_fail;

    /* ... end of body of routine ... */

  fn_exit:
    MPID_MPI_FUNC_EXIT(MPID_STATE_MPI_TYPE_CREATE_HINDEXED_BLOCK);
    MPIU_THREAD_CS_EXIT(ALLFUNC,);
    return mpi_errno;

  fn_fail:
    /* --BEGIN ERROR HANDLING-- */
#ifdef HAVE_ERROR_CHECKING
    {
        mpi_errno =
            MPIR_Err_create_code(mpi_errno, MPIR_ERR_RECOVERABLE, FCNAME, __LINE__,
                                 MPI_ERR_OTHER, "**mpi_type_create_hindexed_block",
                                 "**mpi_type_create_hindexed_block %d %d %p %D %p", count,
                                 blocklength, array_of_displacements, oldtype, newtype);
    }
#endif
    mpi_errno = MPIR_Err_return_comm(NULL, FCNAME, mpi_errno);
    goto fn_exit;
    /* --END ERROR HANDLING-- */
}