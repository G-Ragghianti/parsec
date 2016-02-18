/*
 * Copyright (c) 2010-2015 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */


#ifndef _DATA_DISTRIBUTION_H_
#define _DATA_DISTRIBUTION_H_

#include "dague_config.h"
#include "dague/types.h"

#if defined(DAGUE_HAVE_STDARG_H)
#include <stdarg.h>
#endif  /* defined(DAGUE_HAVE_STDARG_H) */
#if defined(DAGUE_HAVE_UNISTD_H)
#include <unistd.h>
#endif  /* defined(DAGUE_HAVE_UNISTD_H) */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef DAGUE_HAVE_MPI
#include "mpi.h"
#endif /*DAGUE_HAVE_MPI */

struct dague_device_s;
typedef int (*dague_memory_region_management_f)(dague_ddesc_t*, struct dague_device_s*);

typedef uint8_t memory_registration_status_t;
#define    MEMORY_STATUS_UNREGISTERED      ((memory_registration_status_t)0x0)
#define    MEMORY_STATUS_REGISTERED        ((memory_registration_status_t)0x1)

BEGIN_C_DECLS

struct dague_ddesc_s {
    uint32_t            myrank;    /**< process rank */
    uint32_t            nodes;     /**< number of nodes involved in the computation */

    /* return a unique key (unique only for the specified dague_ddesc) associated to a data */
    dague_data_key_t (*data_key)(dague_ddesc_t *d, ...);

    /* return the rank of the process owning the data  */
    uint32_t (*rank_of)(dague_ddesc_t *d, ...);
    uint32_t (*rank_of_key)(dague_ddesc_t *d, dague_data_key_t key);

    /* return the pointer to the data possessed locally */
    dague_data_t* (*data_of)(dague_ddesc_t *d, ...);
    dague_data_t* (*data_of_key)(dague_ddesc_t *d, dague_data_key_t key);

    /* return the virtual process ID of data possessed locally */
    int32_t  (*vpid_of)(dague_ddesc_t *d, ...);
    int32_t  (*vpid_of_key)(dague_ddesc_t *d, dague_data_key_t key);

    /* Memory management function. They are used to register/unregister the data description
     * with the active devices.
     */
    dague_memory_region_management_f register_memory;
    dague_memory_region_management_f unregister_memory;
    memory_registration_status_t memory_registration_status;

    char      *key_base;

#ifdef DAGUE_PROF_TRACE
    /* compute a string in 'buffer' meaningful for profiling about data, return the size of the string */
    int (*key_to_string)(dague_ddesc_t *d, dague_data_key_t key, char * buffer, uint32_t buffer_size);
    char      *key_dim;
    char      *key;
#endif /* DAGUE_PROF_TRACE */
};

/**
 * Define a static data representation where all data are local
 * on the current dague_context_t.
 */
extern const dague_ddesc_t dague_static_local_data_ddesc;

/**
 * Set of default functions that describes one fake data of size 0 owned by
 * everyone node on VP 0
 */
dague_data_key_t dague_ddesc_default_data_key(dague_ddesc_t *d, ...);
uint32_t         dague_ddesc_default_rank_of_key(dague_ddesc_t *d, dague_data_key_t key);
uint32_t         dague_ddesc_default_rank_of(dague_ddesc_t *d, ...);
dague_data_t *   dague_ddesc_default_data_of_key(dague_ddesc_t *d, dague_data_key_t key);
dague_data_t *   dague_ddesc_default_data_of(dague_ddesc_t *d, ...);
uint32_t         dague_ddesc_default_vpid_of_key(dague_ddesc_t *d, dague_data_key_t key );
uint32_t         dague_ddesc_default_vpid_of(dague_ddesc_t *d, ... );

#if defined(DAGUE_PROF_TRACE)
int              dague_ddesc_default_key_to_string(struct dague_ddesc_s *desc,
                                                   uint32_t datakey,
                                                   char * buffer,
                                                   uint32_t buffer_size);
#endif /* defined(DAGUE_PROF_TRACE) */


/**
 * Initialize the dague_desc to default values:
 * A descriptor with one fake data of size 0, replicated on all nodes on VP 0
 */
void dague_ddesc_init(dague_ddesc_t *d,
                      int nodes, int myrank );
void dague_ddesc_destroy(dague_ddesc_t *d);


#if defined(DAGUE_PROF_TRACE)
#include "dague/profiling.h"
/* TODO: Fix me pleaseeeeeee */
#define dague_ddesc_set_key(d, k) do {                                  \
        char dim[strlen(k) + strlen( (d)->key_dim ) + 4];               \
        (d)->key_base = strdup(k);                                      \
        sprintf(dim, "%s%s", k, (d)->key_dim);                          \
        dague_profiling_add_information( "DIMENSION", dim );            \
    } while(0)
#else
#define dague_ddesc_set_key(d, k) do {} while(0)
#endif

END_C_DECLS

#endif /* _DATA_DISTRIBUTION_H_ */

