/*
 * Copyright (c) 2012-2015 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#include "dague_config.h"
#include "pins_papi.h"
#include "dague/mca/pins/pins.h"
#include "dague/mca/pins/pins_papi_utils.h"
#include "dague/utils/output.h"
#include "dague/utils/mca_param.h"
#include <stdio.h>
#include <papi.h>
#include "dague/execution_unit.h"

static void pins_papi_read(dague_execution_unit_t* exec_unit,
                             dague_execution_context_t* exec_context,
                             parsec_pins_next_callback_t* cb_data);

static char* mca_param_string;
static parsec_pins_papi_events_t* pins_papi_events = NULL;

static void pins_init_papi(dague_context_t * master_context)
{
    pins_papi_init(master_context);

    dague_mca_param_reg_string_name("pins", "papi_event",
                                    "PAPI events to be gathered at both socket and core level.\n",
                                    false, false,
                                    "", &mca_param_string);
    if( NULL != mca_param_string ) {
        pins_papi_events = parsec_pins_papi_events_new(mca_param_string);
    }
}

static void pins_fini_papi(dague_context_t * master_context)
{
    if( NULL != pins_papi_events ) {
        parsec_pins_papi_events_free(&pins_papi_events);
        pins_papi_events = NULL;
    }
}

static void pins_thread_init_papi(dague_execution_unit_t * exec_unit)
{
    parsec_pins_papi_callback_t* event_cb = NULL;
    parsec_pins_papi_event_t* event;
    parsec_pins_papi_values_t info;
    int i, n, index, my_socket, my_core, err;
    bool started = false;
    char** conv_string = NULL;

    if( NULL == pins_papi_events )
        return;

    /* TODO: Fix this */
    my_socket = exec_unit->socket_id;
    my_core = exec_unit->core_id;

    event_cb = (parsec_pins_papi_callback_t*)malloc(sizeof(parsec_pins_papi_callback_t));
    event_cb->num_eventsets = 1;

    /* Allocate memory for event_cb->num_eventsets eventsets */
    event_cb->papi_eventset = (int*)malloc(sizeof(int));
    event_cb->num_counters = (int*)malloc(sizeof(int));
    event_cb->pins_prof_event = (int*)malloc(2 * sizeof(int));
    event_cb->begin_end = (int*)malloc(sizeof(int));
    event_cb->num_tasks = (int*)malloc(sizeof(int));
    event_cb->frequency = (int*)malloc(sizeof(int));
    event_cb->events_list = (parsec_pins_papi_events_t**)malloc(sizeof(parsec_pins_papi_events_t*));

    conv_string = (char**)malloc(sizeof(char*));

    /* Set all the matching events */
    for( i = 0; i < pins_papi_events->num_counters; i++ ) {
        event = &pins_papi_events->events[i];
        if( (event->socket != -1) && (event->socket != my_socket) )
            continue;
        if( (event->core != -1) && (event->core != my_core) )
            continue;

        if(!started) {  /* create the event and the PAPI eventset */
            pins_papi_thread_init(exec_unit);

            event_cb->papi_eventset[0] = PAPI_NULL;
            event_cb->num_counters[0] = 0;
            event_cb->events_list[0] = pins_papi_events;
            event_cb->frequency[0] = event->frequency;
            event_cb->begin_end[0] = 0;
            event_cb->num_tasks[0] = 0;

            conv_string[0] = NULL;

            /* Create an empty eventset */
            if( PAPI_OK != (err = PAPI_create_eventset(&event_cb->papi_eventset[0])) ) {
                dague_output(0, "%s: thread %d couldn't create the PAPI event set; ERROR: %s\n",
                             __func__, exec_unit->th_id, PAPI_strerror(err));
                /*
                parsec_pins_papi_event_cleanup(event_cb, &info);
                free(event_cb); event_cb = NULL;
                */
                continue;
            }
            started = true;
        }

        index = -1;
        for(n = 0; n < event_cb->num_eventsets; n++) {
            if(event_cb->frequency[n] == event->frequency) {
                index = n;
                break;
            }
        }

        if(index == -1) {
            index = event_cb->num_eventsets++;

            event_cb->papi_eventset = (int*)realloc(event_cb->papi_eventset, event_cb->num_eventsets * sizeof(int));
            event_cb->num_counters = (int*)realloc(event_cb->num_counters, event_cb->num_eventsets * sizeof(int));
            event_cb->pins_prof_event = (int*)realloc(event_cb->pins_prof_event, 2 * event_cb->num_eventsets * sizeof(int));
            event_cb->begin_end = (int*)realloc(event_cb->begin_end, event_cb->num_eventsets * sizeof(int));
            event_cb->num_tasks = (int*)realloc(event_cb->num_tasks, event_cb->num_eventsets * sizeof(int));
            event_cb->frequency = (int*)realloc(event_cb->frequency, event_cb->num_eventsets * sizeof(int));
            event_cb->events_list = (parsec_pins_papi_events_t**)realloc(event_cb->events_list, event_cb->num_eventsets * sizeof(parsec_pins_papi_events_t*));

            conv_string = (char**)realloc(conv_string, event_cb->num_eventsets * sizeof(char*));

            event_cb->papi_eventset[index] = PAPI_NULL;
            event_cb->num_counters[index] = 0;
            event_cb->events_list[index] = pins_papi_events;
            event_cb->frequency[index] = event->frequency;
            event_cb->begin_end[index] = 0;
            event_cb->num_tasks[index] = 0;

            conv_string[index] = NULL;
            printf("index = %d\n", index);
            /* Create an empty eventset */
            if( PAPI_OK != (err = PAPI_create_eventset(&event_cb->papi_eventset[index])) ) {
                dague_output(0, "%s: thread %d couldn't create the PAPI event set; ERROR: %s\n",
                             __func__, exec_unit->th_id, PAPI_strerror(err));
                /*
                parsec_pins_papi_event_cleanup(event_cb, &info);
                free(event_cb); event_cb = NULL;
                */
                continue;
            }
        }

        /* Add events to the eventset */
        if( PAPI_OK != (err = PAPI_add_event(event_cb->papi_eventset[index],
                                             event->pins_papi_native_event)) ) {
            dague_output(0, "%s: failed to add event %s; ERROR: %s\n",
                         __func__, event->pins_papi_event_name, PAPI_strerror(err));
            continue;
        }
        event_cb->num_counters[index]++;
        if( NULL == conv_string[index] )
            asprintf(&conv_string[index], "%s{int64_t}"PARSEC_PINS_SEPARATOR, event->pins_papi_event_name);
        else {
            char* tmp = conv_string[index];
            asprintf(&conv_string[index], "%s%s{int64_t}"PARSEC_PINS_SEPARATOR, tmp, event->pins_papi_event_name);
            free(tmp);
        }
    }

    if( NULL != event_cb ) {
        for(i = 0; i < event_cb->num_eventsets; i++) {
            if( 0 != event_cb->num_counters[i] ) {
                char* key_string;

                if(event_cb->num_eventsets == 1)
                    asprintf(&key_string, "PINS_PAPI_S%d_C%d", exec_unit->socket_id, exec_unit->core_id);
                else
                    asprintf(&key_string, "PINS_PAPI_S%d_C%d_F%d", exec_unit->socket_id, exec_unit->core_id, event_cb->frequency[i]);

                dague_profiling_add_dictionary_keyword(key_string, "fill:#00AAFF",
                                                       sizeof(long long) * event_cb->num_counters[i],
                                                       conv_string[i],
                                                       &event_cb->pins_prof_event[i*2],
                                                       &event_cb->pins_prof_event[(i*2)+1]);
                free(key_string);
                printf("Starting eventset: i = %d   eventset value: %d   PAPI_NULL = %d\n", i, event_cb->papi_eventset[i], PAPI_NULL);
                if( PAPI_OK != (err = PAPI_start(event_cb->papi_eventset[i])) ) {
                    dague_output(0, "couldn't start PAPI eventset for thread %d; ERROR: %s\n",
                                 exec_unit->th_id, PAPI_strerror(err));
                    event_cb->num_counters[i] = 0;
                    continue;
                    /*goto cleanup_and_return;*/
                }

                if( PAPI_OK != (err = PAPI_read(event_cb->papi_eventset[i], info.values)) ) {
                    dague_output(0, "couldn't read PAPI eventset for thread %d; ERROR: %s\n",
                                 exec_unit->th_id, PAPI_strerror(err));
                    continue;
                    /*goto cleanup_and_return;*/
                }

                (void)dague_profiling_trace(exec_unit->eu_profile, event_cb->pins_prof_event[event_cb->begin_end[i]],
                                            45, 0, (void *)&info);

                event_cb->begin_end[0] = (event_cb->begin_end[i] + 1) & 0x1;  /* aka. % 2 */

                free(conv_string[i]);
            }
            if(event_cb->frequency[i] == 1) {
                PINS_REGISTER(exec_unit, EXEC_BEGIN, pins_papi_read,
                              (parsec_pins_next_callback_t*)event_cb);
            }
            PINS_REGISTER(exec_unit, EXEC_END, pins_papi_read,
                          (parsec_pins_next_callback_t*)event_cb);
        }
        free(conv_string);
        return; /* we're done here */
    }
  cleanup_and_return:
    if( NULL != event_cb ) {
        parsec_pins_papi_event_cleanup(event_cb, &info);
        free(event_cb); event_cb = NULL;
    }
    if( NULL != conv_string )
        free(conv_string);
}

static void pins_thread_fini_papi(dague_execution_unit_t* exec_unit)
{
    parsec_pins_papi_callback_t* event_cb;
    parsec_pins_papi_values_t info;
    int err, i;

    /* Should this be in a loop to unregister all the callbacks for this thread? */
    PINS_UNREGISTER(exec_unit, EXEC_END, pins_papi_read, (parsec_pins_next_callback_t**)&event_cb);

    if( NULL == event_cb )
        return;

    pins_papi_thread_fini(exec_unit);
    for(i = 0; i < event_cb->num_eventsets; i++) {
        if( PAPI_NULL == event_cb->papi_eventset[i] ) {
            parsec_pins_papi_event_cleanup(event_cb, &info);

            /* If the last profiling event was an 'end' event */
            if(event_cb->begin_end[i] == 0) {
                (void)dague_profiling_trace(exec_unit->eu_profile, event_cb->pins_prof_event[i*2],
                                            45, 0, (void *)&info);
            }
            (void)dague_profiling_trace(exec_unit->eu_profile, event_cb->pins_prof_event[(i*2)+1],
                                        45, 0, (void *)&info);
        }
    }

    free(event_cb->papi_eventset);
    free(event_cb->num_counters);
    free(event_cb->pins_prof_event);
    free(event_cb->begin_end);
    free(event_cb->num_tasks);
    free(event_cb->frequency);
    free(event_cb->events_list);

    free(event_cb);
}

static void pins_papi_read(dague_execution_unit_t* exec_unit,
                             dague_execution_context_t* exec_context,
                             parsec_pins_next_callback_t* cb_data)
{
    parsec_pins_papi_callback_t* event_cb = (parsec_pins_papi_callback_t*)cb_data;
    parsec_pins_papi_values_t info;
    int i, err;

    for(i = 0; i < event_cb->num_eventsets; i++) {
        if(0 == event_cb->num_counters[i])
            continue;
        event_cb->num_tasks[i]++;
        if(event_cb->num_tasks[i] < event_cb->frequency[i])
            return;

        if( PAPI_OK != (err = PAPI_read(event_cb->papi_eventset[i], info.values)) ) {
            dague_output(0, "couldn't read PAPI eventset for thread %d; ERROR: %s\n",
                         exec_unit->th_id, PAPI_strerror(err));
            return;
        }
        (void)dague_profiling_trace(exec_unit->eu_profile, event_cb->pins_prof_event[event_cb->begin_end[i]],
                                    45, 0, (void *)&info);
        event_cb->begin_end[i] = (event_cb->begin_end[i] + 1) & 0x1;  /* aka. % 2 */
        event_cb->num_tasks[i] = 0;
    }
}

const dague_pins_module_t dague_pins_papi_module = {
    &dague_pins_papi_component,
    {
        pins_init_papi,
        pins_fini_papi,
        NULL,
        NULL,
        pins_thread_init_papi,
        pins_thread_fini_papi,
    }
};