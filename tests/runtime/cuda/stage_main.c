/*
 * Copyright (c) 2020-2024 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */
#include "parsec.h"
#include "parsec/data_distribution.h"
#include "parsec/data_dist/matrix/matrix.h"
#include "parsec/data_dist/matrix/two_dim_rectangle_cyclic.h"

#include "stage_custom.h"
parsec_taskpool_t* testing_stage_custom_New( parsec_context_t *ctx, int M, int N, int MB, int NB, int P, int *ret);

#if defined(DISTRIBUTED)
#include <mpi.h>
#endif

int main(int argc, char *argv[])
{
    parsec_context_t *parsec = NULL;
    parsec_taskpool_t *tp;
    int size = 1;
    int rank = 0;
    int M;
    int N;
    int MB;
    int NB;
    int P = 1;
    int ret = 0;

#if defined(DISTRIBUTED)
    {
        int provided;
        MPI_Init_thread(NULL, NULL, MPI_THREAD_SERIALIZED, &provided);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif /* DISTRIBUTED */

    /* Initialize PaRSEC */
    parsec = parsec_init(-1, &argc, &argv);
    if( NULL == parsec ) {
        /* Failed to correctly initialize. In a correct scenario report*/
         /* upstream, but in this particular case bail out.*/
        exit(-1);
    }

    /* can the test run? */
    assert(size == 1);
    int nb_gpus = parsec_context_query(parsec, PARSEC_CONTEXT_QUERY_DEVICES, PARSEC_DEV_CUDA);
    assert(nb_gpus >= 0);
    if(nb_gpus == 0) {
        parsec_warning("This test can only run if at least one GPU device is present");
        printf("TEST SKIPPED\n");
        exit(-PARSEC_ERR_DEVICE);
    }

    /* Test: comparing results when:
        - tile matrix transfered to GPU with default stage_in/stage_out
        - lapack matrix transfered to GPU with custum stage_in/stage_out */

    MB = NB = 1;
    M = N = 1;
    tp = testing_stage_custom_New(parsec, M, N, MB, NB, P, &ret);
    if( NULL != tp ) {
        parsec_context_add_taskpool(parsec, tp);
        parsec_context_start(parsec);
        parsec_context_wait(parsec);
        parsec_taskpool_free(tp);
    }

    MB = NB = 1;
    M = N = 10;
    tp = testing_stage_custom_New(parsec, M, N, MB, NB, P, &ret);
    if( NULL != tp ) {
        parsec_context_add_taskpool(parsec, tp);
        parsec_context_start(parsec);
        parsec_context_wait(parsec);
        parsec_taskpool_free(tp);
    }

    MB = NB = 4;
    M = N = 20;
    tp = testing_stage_custom_New(parsec, M, N, MB, NB, P, &ret);
    if( NULL != tp ) {
        parsec_context_add_taskpool(parsec, tp);
        parsec_context_start(parsec);
        parsec_context_wait(parsec);
        parsec_taskpool_free(tp);
    }

    MB = NB = 40;
    M = N = 240;
    tp = testing_stage_custom_New(parsec, M, N, MB, NB, P, &ret);
    if( NULL != tp ) {
        parsec_context_add_taskpool(parsec, tp);
        parsec_context_start(parsec);
        parsec_context_wait(parsec);
        parsec_taskpool_free(tp);
    }

    if( ret != 0) {
        printf("TEST FAILED (%d errors)\n", ret);
    } else {
        printf("TEST PASSED\n");
    }

    parsec_fini(&parsec);
#if defined(DISTRIBUTED)
    MPI_Finalize();
#endif /* DISTRIBUTED */

    return (0 == ret)? EXIT_SUCCESS: EXIT_FAILURE;
}
