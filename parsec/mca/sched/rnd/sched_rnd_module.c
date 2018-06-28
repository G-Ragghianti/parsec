/**
 * Copyright (c) 2013-2018 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 *
 */

#include "parsec/parsec_config.h"
#include "parsec/parsec_internal.h"
#include "parsec/utils/debug.h"
#include "parsec/mca/sched/sched.h"
#include "parsec/mca/sched/rnd/sched_rnd.h"
#include "parsec/class/dequeue.h"
#include "parsec/mca/pins/pins.h"
#include "parsec/papi_sde.h"
#include "parsec/mca/sched/sched_local_queues_utils.h"

/**
 * Module functions
 */
static int sched_rnd_install(parsec_context_t* master);
static int sched_rnd_schedule(parsec_execution_stream_t* es,
                              parsec_task_t* new_context,
                              int32_t distance);
static parsec_task_t *sched_rnd_select(parsec_execution_stream_t *es,
                                       int32_t* distance);
static int flow_rnd_init(parsec_execution_stream_t* es, struct parsec_barrier_t* barrier);
static void sched_rnd_remove(parsec_context_t* master);
static void sched_rnd_register_sde_counters(parsec_execution_stream_t *es);

const parsec_sched_module_t parsec_sched_rnd_module = {
    &parsec_sched_rnd_component,
    {
        sched_rnd_install,
        flow_rnd_init,
        sched_rnd_schedule,
        sched_rnd_select,
        NULL,
        sched_rnd_register_sde_counters,
        sched_rnd_remove
    }
};

#define LOCAL_SCHED_OBJECT(eu_context) ((parsec_list_local_counter_t*)(eu_context)->scheduler_object)

static int sched_rnd_install( parsec_context_t *master )
{
    (void)master;
    return 0;
}

static void sched_rnd_register_sde_counters(parsec_execution_stream_t *es)
{
#if defined(PARSEC_PAPI_SDE)
    char event_name[PARSEC_PAPI_SDE_MAX_COUNTER_NAME_LEN];
    /* We register the counters only if the scheduler is installed, and only once per es */
    if( NULL != es && 0 == es->th_id ) {
        snprintf(event_name, PARSEC_PAPI_SDE_MAX_COUNTER_NAME_LEN,
                 "PARSEC::SCHEDULER::PENDING_TASKS::QUEUE=%d::SCHED=RND", es->virtual_process->vp_id);
        papi_sde_register_fp_counter(parsec_papi_sde_handle, event_name, PAPI_SDE_RO|PAPI_SDE_INSTANT,
                                     PAPI_SDE_int, (papi_sde_fptr_t)parsec_list_local_counter_length, es->virtual_process);
        papi_sde_add_counter_to_group(parsec_papi_sde_handle, event_name,
                                      "PARSEC::SCHEDULER::PENDING_TASKS", PAPI_SDE_SUM);
        papi_sde_add_counter_to_group(parsec_papi_sde_handle, event_name,
                                      "PARSEC::SCHEDULER::PENDING_TASKS::SCHED=RND", PAPI_SDE_SUM);
    }
    /* We describe the counters once if the scheduler is installed, or if we are called without
     * an execution stream (typically during papi_native_avail library load) */
    if( NULL == es || 0 == es->th_id ) {
        papi_sde_describe_counter(parsec_papi_sde_handle, "PARSEC::SCHEDULER::PENDING_TASKS::SCHED=RND",
                                  "the number of pending tasks for the RND scheduler");
        papi_sde_describe_counter(parsec_papi_sde_handle, "PARSEC::SCHEDULER::PENDING_TASKS::QUEUE=<VPID>::SCHED=RND",
                                  "the number of pending tasks for the RND scheduler on virtual process <VPID>");
    }
#else
    (void)es;
#endif
}

static int flow_rnd_init(parsec_execution_stream_t* es, struct parsec_barrier_t* barrier)
{
    parsec_vp_t *vp = es->virtual_process;
    
    if (es == vp->execution_streams[0]) {
        es->scheduler_object = allocate_parsec_list_local_counter(NULL);
    }

    parsec_barrier_wait(barrier);

    if( es != vp->execution_streams[0]) {
        es->scheduler_object = allocate_parsec_list_local_counter( LOCAL_SCHED_OBJECT(vp->execution_streams[0]) );
    }

    sched_rnd_register_sde_counters(es);
    
    return 0;
}

static parsec_task_t*
sched_rnd_select(parsec_execution_stream_t *es,
                 int32_t* distance)
{
    parsec_task_t * context = pop_from_parsec_list_local_counter(LOCAL_SCHED_OBJECT(es));
    *distance = 0;
    return context;
}

static int sched_rnd_schedule(parsec_execution_stream_t* es,
                              parsec_task_t* new_context,
                              int32_t distance)
{
    parsec_list_t tmp;
    parsec_list_item_t *it = (parsec_list_item_t*)new_context;
#if defined(PARSEC_DEBUG_NOISIER)
    char tmp_str[MAX_TASK_STRLEN];
#endif
    do {
#if defined(PARSEC_DEBUG_NOISIER)
        PARSEC_DEBUG_VERBOSE(20, parsec_debug_output, "RND:\t Pushing task %s",
                parsec_task_snprintf(tmp_str, MAX_TASK_STRLEN, (parsec_task_t*)it));
#endif
        /* randomly assign priority */
        (*((int*)(((uintptr_t)it)+parsec_execution_context_priority_comparator))) = rand() + distance;
        it = (parsec_list_item_t*)((parsec_list_item_t*)it)->list_next;
    } while( it != (parsec_list_item_t*)new_context );

    /* Re-sort new_context according to new priorities */
    OBJ_CONSTRUCT(&tmp, parsec_list_t);
    parsec_list_nolock_chain_front(&tmp, &new_context->super);
    parsec_list_nolock_sort(&tmp, parsec_execution_context_priority_comparator);
    new_context = (parsec_task_t*)parsec_list_nolock_unchain(&tmp);
    OBJ_DESTRUCT(&tmp);
    
    chain_to_parsec_list_local_counter(LOCAL_SCHED_OBJECT(es), new_context, parsec_execution_context_priority_comparator);

    /* We can ignore distance, the task will randomly get inserted in a place that
     * will prevent livelocks. */
    (void)distance;
    return 0;
}

static void sched_rnd_remove( parsec_context_t *master )
{
    int p, t;
    parsec_vp_t *vp;
    parsec_execution_stream_t *es;
    parsec_list_local_counter_t *sl;

    for(p = 0; p < master->nb_vp; p++) {
        vp = master->virtual_processes[p];
        for(t = 0; t < vp->nb_cores; t++) {
            es = vp->execution_streams[t];
            sl = LOCAL_SCHED_OBJECT(es);
            free_parsec_list_local_counter(sl);
            es->scheduler_object = NULL;
        }
        parsec_papi_sde_unregister_counter("PARSEC::SCHEDULER::PENDING_TASKS::QUEUE=%d::SCHED=RND", p);
    }
    parsec_papi_sde_unregister_counter("PARSEC::SCHEDULER::PENDING_TASKS::SCHED=RND");
}
