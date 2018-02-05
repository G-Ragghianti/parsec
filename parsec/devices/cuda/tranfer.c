/*
 * Copyright (c) 2016-2017 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#include "parsec/parsec_config.h"
#include "parsec/parsec_internal.h"
#include "parsec/sys/atomic.h"

#include "parsec.h"
#include "parsec/constants.h"
#include "parsec/data_internal.h"
#include "parsec/devices/cuda/dev_cuda.h"
#include "parsec/profiling.h"
#include "parsec/execution_stream.h"
#include "parsec/arena.h"
#include "parsec/utils/output.h"
#include "parsec/scheduling.h"

#include <cuda.h>
#include <cuda_runtime_api.h>

/**
 * Entierly local tasks that should only be used to move data between a device and the main memory. Such
 * tasks would be generated by the CUDA engine when a lack of data for future tasks is detected, in which
 * case the least recently used data owned by the device will be marked for tranfer back to the main
 * memory. Upon succesful tranfer the data will become shared, and future tasks would then be able to
 * acquire the GPU space for their own usage. A second usage will be to forcefully move a data back to the
 * main memory (or any other location in fact), when a task that will execute in another context requires
 * one of the data from the GPU.
 */
typedef struct parsec_CUDA_d2h_task_s {
    parsec_task_t super;
    int nb_data;
} parsec_CUDA_d2h_task_t;

static int
hook_of_CUDA_d2h_task( parsec_execution_stream_t* es,
                       parsec_CUDA_d2h_task_t* this_task )
{
    (void)es; (void)this_task;
    return PARSEC_SUCCESS;
}

static int
affinity_of_CUDA_d2h_task( parsec_CUDA_d2h_task_t* this_task,
                           parsec_data_ref_t* ref )
{
    (void)this_task; (void)ref;
    return PARSEC_SUCCESS;
}

static void
iterate_successors_of_CUDA_d2h_task( parsec_execution_stream_t* es,
                                     const parsec_CUDA_d2h_task_t* this_task,
                                     uint32_t action_mask,
                                     parsec_ontask_function_t * ontask, void *ontask_arg )
{
    (void)es; (void)this_task; (void)action_mask; (void)ontask; (void)ontask_arg;
}

static void
iterate_predecessors_of_CUDA_d2h_task( parsec_execution_stream_t* es,
                                       const parsec_CUDA_d2h_task_t* this_task,
                                       uint32_t action_mask,
                                       parsec_ontask_function_t * ontask, void *ontask_arg )
{
    (void)es; (void)this_task; (void)action_mask; (void)ontask; (void)ontask_arg;
}

static int
release_deps_of_CUDA_d2h_task( parsec_execution_stream_t* es,
                               parsec_CUDA_d2h_task_t* this_task,
                               uint32_t action_mask,
                               parsec_remote_deps_t* deps )
{
    (void)es; (void)this_task; (void)action_mask; (void)deps;
    return PARSEC_SUCCESS;
}

static int
data_lookup_of_CUDA_d2h_task( parsec_execution_stream_t* es,
                              parsec_CUDA_d2h_task_t* this_task )
{
    (void)es; (void)this_task;
    return PARSEC_SUCCESS;
}

static int
complete_hook_of_CUDA_d2h_task( parsec_execution_stream_t* es,
                                parsec_CUDA_d2h_task_t* this_task )
{
    (void)es; (void)this_task;
    return PARSEC_SUCCESS;
}

static parsec_hook_return_t
release_task_of_CUDA_d2h_task(parsec_execution_stream_t* es,
                              parsec_CUDA_d2h_task_t* this_task )
{
    (void)es; (void)this_task;
    return PARSEC_HOOK_RETURN_DONE;
}

static int
datatype_lookup_of_CUDA_d2h_task( parsec_execution_stream_t * es,
                                  const parsec_CUDA_d2h_task_t* this_task,
                                  uint32_t * flow_mask, parsec_dep_data_description_t * data)
{
    (void)es; (void)this_task; (void)flow_mask; (void)data;
    return PARSEC_SUCCESS;
}

static int32_t parsec_CUDA_d2h_counter = 0;
static uint64_t key_of_CUDA_d2h_task(const parsec_taskpool_t *tp,
                                     const assignment_t *assignments)
{
    (void)tp; (void)assignments;
    return (uint64_t)parsec_atomic_add_32b((volatile int32_t*)&parsec_CUDA_d2h_counter, 1);
}

static parsec_data_t*
flow_of_CUDA_d2h_task_direct_access( const parsec_CUDA_d2h_task_t* this_task,
                                     const assignment_t *assignments )
{
    (void)this_task; (void)assignments;
    return NULL;
}

static const __parsec_chore_t __CUDA_d2h_task_chores[] = {
    {.type = PARSEC_DEV_CUDA,
     .evaluate = NULL,
     .hook = (parsec_hook_t *) hook_of_CUDA_d2h_task},
    {.type = PARSEC_DEV_NONE,
     .evaluate = NULL,
     .hook = (parsec_hook_t *) NULL},   /* End marker */
};

static const parsec_flow_t flow_of_CUDA_d2h_task;
static const dep_t flow_of_CUDA_d2h_task_dep = {
    .cond = NULL,
    .ctl_gather_nb = NULL,
    .task_class_id = -1,
    .direct_data = (direct_data_lookup_func_t)flow_of_CUDA_d2h_task_direct_access,
    .dep_index = 1,
    .dep_datatype_index = 0,
    .belongs_to = &flow_of_CUDA_d2h_task,
};

static const parsec_flow_t flow_of_CUDA_d2h_task = {
    .name = "Generic flow for d2h tasks",
    .sym_type = SYM_OUT,
    .flow_flags = FLOW_ACCESS_RW | FLOW_HAS_IN_DEPS,
    .flow_index = 0,
    .flow_datatype_mask = 0x1,
    .dep_in = {},
    .dep_out = {&flow_of_CUDA_d2h_task_dep}
};

static const symbol_t symb_CUDA_d2h_task_param = {
    .name = "unnamed",
    .min = NULL,
    .max = NULL,
    .context_index = 0,
    .cst_inc = 1,
    .expr_inc = NULL,
    .flags = 0x0
};

static const parsec_task_class_t parsec_CUDA_d2h_task_class = {
    .name = "CUDA D2H data tranfer",
    .task_class_id = 0,
    .nb_flows = 5,
    .nb_parameters = 1,
    .nb_locals = 0,
    .params = {&symb_CUDA_d2h_task_param},
    .locals = {NULL},
    .data_affinity = (parsec_data_ref_fn_t *) affinity_of_CUDA_d2h_task,
    .initial_data = (parsec_data_ref_fn_t *) NULL,
    .final_data = (parsec_data_ref_fn_t *) NULL,
    .priority = NULL,
    .in = {&flow_of_CUDA_d2h_task, NULL},
    .out = {&flow_of_CUDA_d2h_task, NULL},
    .flags = 0x0 | PARSEC_HAS_IN_IN_DEPENDENCIES | PARSEC_USE_DEPS_MASK,
    .dependencies_goal = 0x1,  /* we generate then when needed so the dependencies_goal is useless */
    .make_key = (parsec_functionkey_fn_t *) key_of_CUDA_d2h_task,
    .fini = (parsec_hook_t *) NULL,
    .incarnations = __CUDA_d2h_task_chores,
    .find_deps = parsec_hash_find_deps,
    .iterate_successors = (parsec_traverse_function_t *) iterate_successors_of_CUDA_d2h_task,
    .iterate_predecessors = (parsec_traverse_function_t *) iterate_predecessors_of_CUDA_d2h_task,
    .release_deps = (parsec_release_deps_t *) release_deps_of_CUDA_d2h_task,
    .prepare_input = (parsec_hook_t *) data_lookup_of_CUDA_d2h_task,
    .prepare_output = (parsec_hook_t *) NULL,
    .get_datatype = (parsec_datatype_lookup_t *) datatype_lookup_of_CUDA_d2h_task,
    .complete_execution = (parsec_hook_t *) complete_hook_of_CUDA_d2h_task,
    .release_task = (parsec_hook_t *) release_task_of_CUDA_d2h_task,
#if defined(PARSEC_SIM)
    .sim_cost_fct = (parsec_sim_cost_fct_t *) NULL,
#endif
};


/**
 * Transfer at most the MAX_PARAM_COUNT oldest data from the GPU back
 * to main memory. Create a single task to move them all out, then switch the
 * GPU data copy in shared mode.
 */
parsec_gpu_context_t*
parsec_gpu_create_W2R_task(gpu_device_t *gpu_device,
                           parsec_execution_stream_t *es)
{
    parsec_gpu_context_t *w2r_task = NULL;
    parsec_CUDA_d2h_task_t *d2h_task = NULL;
    parsec_gpu_data_copy_t *gpu_copy;
    parsec_list_item_t* item = (parsec_list_item_t*)gpu_device->gpu_mem_owned_lru.ghost_element.list_next;
    int nb_cleaned = 0;

    /* Find a data copy that has no pending users on the GPU, and can be
     * safely moved back on the main memory */
    while(nb_cleaned < MAX_PARAM_COUNT) {
        /* Break at the end of the list */
        if( item == &(gpu_device->gpu_mem_owned_lru.ghost_element) ) {
            break;
        }
        gpu_copy = (parsec_gpu_data_copy_t*)item;
        /* get the next item before altering the next pointer */
        item = (parsec_list_item_t*)item->list_next;  /* conversion needed for volatile */
        if( 0 == gpu_copy->readers ) {
            if( PARSEC_UNLIKELY(NULL == d2h_task) ) {  /* allocate on-demand */
                d2h_task = (parsec_CUDA_d2h_task_t*)parsec_thread_mempool_allocate(es->context_mempool);
                if( PARSEC_UNLIKELY(NULL == d2h_task) )  /* we're running out of memory. Bail out. */
                    break;
            }
            parsec_list_item_ring_chop((parsec_list_item_t*)gpu_copy);
            PARSEC_LIST_ITEM_SINGLETON(gpu_copy);
            gpu_copy->readers++;
            d2h_task->super.data[nb_cleaned].data_out = gpu_copy;
            PARSEC_DEBUG_VERBOSE(10, parsec_device_output,  "D2H[%d] task %p:\tdata %d -> %p [%p] readers %d",
                                 gpu_device->cuda_index, (void*)d2h_task,
                                 nb_cleaned, gpu_copy, gpu_copy->original, gpu_copy->readers);
            nb_cleaned++;
        }
    }

    if( 0 == nb_cleaned )
        return NULL;

    w2r_task = (parsec_gpu_context_t *)malloc(sizeof(parsec_gpu_context_t));
    OBJ_CONSTRUCT(w2r_task, parsec_list_item_t);
    d2h_task->super.priority   = INT32_MAX;
    d2h_task->super.task_class = &parsec_CUDA_d2h_task_class;
    d2h_task->super.status     = PARSEC_TASK_STATUS_NONE;
    d2h_task->super.taskpool   = NULL;
    d2h_task->nb_data          = nb_cleaned;
    w2r_task->ec               = (parsec_task_t*)d2h_task;
    w2r_task->task_type        = GPU_TASK_TYPE_D2HTRANSFER;
    (void)es;
    return w2r_task;
}

/**
 * Complete a data copy transfer originated from the engine.
 */
int parsec_gpu_W2R_task_fini(gpu_device_t *gpu_device,
                             parsec_gpu_context_t *gpu_task,
                             parsec_execution_stream_t *es)
{
    parsec_gpu_data_copy_t *gpu_copy, *cpu_copy;
    parsec_CUDA_d2h_task_t* task = (parsec_CUDA_d2h_task_t*)gpu_task->ec;
    parsec_data_t* original;

    PARSEC_DEBUG_VERBOSE(10, parsec_device_output,  "D2H[%d] task %p: %d data transferred to host",
                         gpu_device->cuda_index, (void*)task, task->nb_data);
    assert(gpu_task->task_type == GPU_TASK_TYPE_D2HTRANSFER);
    for( int i = 0; i < task->nb_data; i++ ) {
        gpu_copy = task->super.data[i].data_out;
        gpu_copy->coherency_state = DATA_COHERENCY_SHARED;
        original = gpu_copy->original;
        cpu_copy = original->device_copies[0];
        cpu_copy->coherency_state =  DATA_COHERENCY_SHARED;
        cpu_copy->version = gpu_copy->version;
        PARSEC_DEBUG_VERBOSE(10, parsec_cuda_output_stream,
                             "D2H[%d] task %p: copy %p [%p] now available",
                             gpu_device->cuda_index, (void*)task,
                             gpu_copy, gpu_copy->original);
        gpu_copy->readers--;
        assert(gpu_copy->readers >= 0);
        parsec_list_nolock_fifo_push(&gpu_device->gpu_mem_lru, (parsec_list_item_t*)gpu_copy);
    }
    parsec_thread_mempool_free(es->context_mempool, gpu_task->ec);
    free(gpu_task);
    return 0;
}

