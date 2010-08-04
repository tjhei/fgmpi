/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *
 *  (C) 2003 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mpi.h"

#define NUM_TIMES 500
#define MAX_BUF_SIZE (400 * 1024 * 1024) /* 400 MB */
#define PUT_SIZE (1024 * 1024) /* 1MB */

static char MTEST_Descrip[] = "ADLB mimic test";

/*
 * ALGORITHM:
 *    This test uses one server process (S), one target process (T)
 *    and a bunch of origin processes (O). 'O' PUTs (LOCK/PUT/UNLOCK)
 *    data to a distinct part of the window, and sends a message to
 *    'S' once the UNLOCK has completed. The server forwards this
 *    message to 'T'. 'T' GETS the data from this buffer after it
 *    receives the message from 'S', to see if it contains the correct
 *    contents.
 *
 *                          -------
 *                          |  S  |
 *                          -------
 *                         ^       \
 *                step 2  /         \ step 3
 *                 SEND  /           \ SEND
 *                      /             v
 *                  -------  step 1   -------
 *                  |     | --------> |     |
 *                  |     |   PUT     |     |
 *                  |  O  |           |  T  |
 *                  |     |  step 4   |     |
 *                  |     | <-------- |     |
 *                  -------   SEND    -------
 *
 */

int main(int argc, char **argv)
{
    int comm_size, comm_rank, i, by_rank, errs = 0;
    char *rma_win_addr, *local_buf;
    char check;
    MPI_Win win;
    MPI_Status status;

    MTest_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);

    if ((comm_size > (MAX_BUF_SIZE / PUT_SIZE)) || (comm_size <= 2))
        MPI_Abort(MPI_COMM_WORLD, 1);

    MPI_Alloc_mem(MAX_BUF_SIZE, MPI_INFO_NULL, (void *) &rma_win_addr);
    memset(rma_win_addr, 0, MAX_BUF_SIZE);
    MPI_Win_create((void *) rma_win_addr, MAX_BUF_SIZE, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &win);

    MPI_Alloc_mem(PUT_SIZE, MPI_INFO_NULL, (void *) &local_buf);
    for (i = 0; i < PUT_SIZE; i++)
        local_buf[i] = 1;

    MPI_Barrier(MPI_COMM_WORLD);

    if (comm_rank == 0) { /* target */
        for (i = 0; i < (NUM_TIMES * (comm_size - 2)); i++) {
            /* Wait for a message from the server to notify me that
             * someone put some data in my window */
            MPI_Recv(&by_rank, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, &status);

            /* Got a message from the server that 'by_rank' put some
             * data in my local window. Check the last byte to make
             * sure we got it correctly. */
            MPI_Win_lock(MPI_LOCK_SHARED, 0, 0, win);
            MPI_Get((void *) &check, 1, MPI_CHAR, 0, ((by_rank + 1) * PUT_SIZE) - 1, 1,
                    MPI_CHAR, win);
            MPI_Win_unlock(0, win);

            /* If this is not the value I expect, count it as an error */
            if (check != 1)
                errs++;

            /* Reset the buffer to zero for the next round */
            memset((void *) (rma_win_addr + (by_rank * PUT_SIZE)), 0, PUT_SIZE);

            /* Tell the origin that I am ready for the next round */
            MPI_Send(NULL, 0, MPI_INT, by_rank, 0, MPI_COMM_WORLD);
        }
    }

    else if (comm_rank == 1) { /* server */
        for (i = 0; i < (NUM_TIMES * (comm_size - 2)); i++) {
            /* Wait for a message from any of the origin processes
             * informing me that it has put data to the target
             * process */
            MPI_Recv(NULL, 0, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
            by_rank = status.MPI_SOURCE;

            /* Tell the target process that it should be seeing some
             * data in its local buffer */
            MPI_Send(&by_rank, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        }
    }

    else { /* origin */
        for (i = 0; i < NUM_TIMES; i++) {
            /* Put some data in the target window */
            MPI_Win_lock(MPI_LOCK_SHARED, 0, 0, win);
            MPI_Put(local_buf, PUT_SIZE, MPI_CHAR, 0, comm_rank * PUT_SIZE, PUT_SIZE,
                    MPI_CHAR, win);
            MPI_Win_unlock(0, win);

            /* Tell the server that the put has completed */
            MPI_Send(NULL, 0, MPI_INT, 1, 0, MPI_COMM_WORLD);

            /* Wait for a message from the target that it is ready for
             * the next round */
            MPI_Recv(NULL, 0, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
        }
    }

    MPI_Win_free(&win);

    MTest_Finalize(errs);
    MPI_Finalize();

    return 0;
}