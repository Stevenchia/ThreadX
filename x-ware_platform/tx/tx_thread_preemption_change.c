/**************************************************************************/ 
/*                                                                        */ 
/*            Copyright (c) 1996-2015 by Express Logic Inc.               */ 
/*                                                                        */ 
/*  This software is copyrighted by and is the sole property of Express   */ 
/*  Logic, Inc.  All rights, title, ownership, or other interests         */ 
/*  in the software remain the property of Express Logic, Inc.  This      */ 
/*  software may only be used in accordance with the corresponding        */ 
/*  license agreement.  Any unauthorized use, duplication, transmission,  */ 
/*  distribution, or disclosure of this software is expressly forbidden.  */ 
/*                                                                        */
/*  This Copyright notice may not be removed or modified without prior    */ 
/*  written consent of Express Logic, Inc.                                */ 
/*                                                                        */ 
/*  Express Logic, Inc. reserves the right to modify this software        */ 
/*  without notice.                                                       */ 
/*                                                                        */ 
/*  Express Logic, Inc.                     info@expresslogic.com         */
/*  11423 West Bernardo Court               www.expresslogic.com          */
/*  San Diego, CA  92127                                                  */
/*                                                                        */
/**************************************************************************/


/**************************************************************************/
/**************************************************************************/
/**                                                                       */ 
/** ThreadX Component                                                     */ 
/**                                                                       */
/**   Thread                                                              */
/**                                                                       */
/**************************************************************************/
/**************************************************************************/

#define TX_SOURCE_CODE


/* Include necessary system files.  */

#include "tx_api.h"
#include "tx_trace.h"
#include "tx_thread.h"


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _tx_thread_preemption_change                        PORTABLE C      */ 
/*                                                           5.7          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    William E. Lamie, Express Logic, Inc.                               */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processes preemption-threshold change requests.  The  */ 
/*    previous preemption is returned to the caller.  If the new request  */ 
/*    allows a higher priority thread to execute, preemption takes place  */ 
/*    inside of this function.                                            */ 
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    thread_ptr                            Pointer to thread             */ 
/*    new_threshold                         New preemption threshold      */ 
/*    old_threshold                         Old preemption threshold      */ 
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    status                                Service return status         */ 
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    _tx_thread_system_preempt_check       Check for preemption          */ 
/*                                                                        */ 
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    Application Code                                                    */ 
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  12-12-2005     William E. Lamie         Initial Version 5.0           */ 
/*  04-02-2007     William E. Lamie         Modified comment(s), added    */ 
/*                                            logic to update and use the */ 
/*                                            original threshold, and     */ 
/*                                            corrected problem of not    */ 
/*                                            setting preemption-threshold*/ 
/*                                            bit when lowering the       */ 
/*                                            thread preemption-threshold,*/ 
/*                                            resulting in version 5.1    */ 
/*  12-12-2008     William E. Lamie         Modified comment(s), added    */ 
/*                                            filter option to trace      */ 
/*                                            insert, changed MOD32 macro */ 
/*                                            to new TX_MOD32_BIT_SET     */ 
/*                                            macro, removed code for     */ 
/*                                            interrupt preemption        */ 
/*                                            performance counter updates,*/ 
/*                                            added logic to handle       */ 
/*                                            restoring preemption-       */ 
/*                                            threshold values during     */ 
/*                                            priority inheritance, and   */ 
/*                                            added TX_DIV32_BIT_SET      */ 
/*                                            macro, resulting in         */ 
/*                                            version 5.2                 */ 
/*  07-04-2009     William E. Lamie         Modified comment(s),          */ 
/*                                            resulting in version 5.3    */ 
/*  12-12-2009     William E. Lamie         Modified comment(s), changed  */ 
/*                                            logic to use a macro to get */ 
/*                                            the system state, changed   */ 
/*                                            logic to ensure the highest */ 
/*                                            preemption-threshold, merged*/ 
/*                                            event logging support, and  */ 
/*                                            removed compound            */ 
/*                                            conditionals, resulting     */ 
/*                                            in version 5.4              */ 
/*  07-15-2011     William E. Lamie         Modified comment(s),          */ 
/*                                            resulting in version 5.5    */ 
/*  11-01-2012     William E. Lamie         Modified comment(s),          */ 
/*                                            resulting in version 5.6    */ 
/*  05-01-2015     William E. Lamie         Modified comment(s), and      */ 
/*                                            modified code for MISRA     */ 
/*                                            compliance, resulting in    */ 
/*                                            version 5.7                 */ 
/*                                                                        */ 
/**************************************************************************/ 
UINT  _tx_thread_preemption_change(TX_THREAD *thread_ptr, UINT new_threshold, UINT *old_threshold)
{

TX_INTERRUPT_SAVE_AREA

#ifndef TX_DISABLE_PREEMPTION_THRESHOLD
ULONG       priority_bit;
#if TX_MAX_PRIORITIES > 32
UINT        map_index;
#endif
#endif
UINT        status;


    /* Default status to success.  */
    status =  TX_SUCCESS;

#ifdef TX_DISABLE_PREEMPTION_THRESHOLD

    /* Only allow 0 (disable all preemption) and returning preemption-threshold to the 
       current thread priority if preemption-threshold is disabled. All other threshold
       values are converted to 0.  */
    if (thread_ptr -> tx_thread_user_priority != new_threshold)
    {
    
        /* Is the new threshold zero?  */
        if (new_threshold != ((UINT) 0))
        {
        
            /* Convert the new threshold to disable all preemption, since preemption-threshold is
               not supported.  */
            new_threshold =  ((UINT) 0);
        }
    }
#endif

    /* Lockout interrupts while the thread is being resumed.  */
    TX_DISABLE

    /* If trace is enabled, insert this event into the trace buffer.  */
    TX_TRACE_IN_LINE_INSERT(TX_TRACE_THREAD_PREEMPTION_CHANGE, thread_ptr, new_threshold, thread_ptr -> tx_thread_preempt_threshold, thread_ptr -> tx_thread_state, TX_TRACE_THREAD_EVENTS)

    /* Log this kernel call.  */
    TX_EL_THREAD_PREEMPTION_CHANGE_INSERT

    /* Determine if the new threshold is greater than the current user priority.  */
    if (new_threshold > thread_ptr -> tx_thread_user_priority)
    {
            
        /* Return error.  */
        status =  TX_THRESH_ERROR;
    }
    else
    {

#ifndef TX_DISABLE_PREEMPTION_THRESHOLD

        /* Determine if the new threshold is the same as the priority.  */
        if (thread_ptr -> tx_thread_user_priority == new_threshold)
        {

            /* Determine if this thread is at the head of the list.  */
            if (_tx_thread_priority_list[thread_ptr -> tx_thread_priority] == thread_ptr)
            {

#if TX_MAX_PRIORITIES > 32

                /* Calculate the index into the bit map array.  */
                map_index =  (thread_ptr -> tx_thread_priority)/((UINT) 32);
#endif

                /* Yes, this thread is at the front of the list.  Make sure
                   the preempted bit is cleared for this thread.  */
                TX_MOD32_BIT_SET(thread_ptr -> tx_thread_priority, priority_bit)
                _tx_thread_preempted_maps[MAP_INDEX] =  _tx_thread_preempted_maps[MAP_INDEX] & (~(priority_bit));

#if TX_MAX_PRIORITIES > 32

                /* Determine if there are any other bits set in this preempt map.  */
                if (_tx_thread_preempted_maps[MAP_INDEX] == ((ULONG) 0))
                {

                    /* No, clear the active bit to signify this preempt map has nothing set.  */
                    TX_DIV32_BIT_SET(thread_ptr -> tx_thread_priority, priority_bit)
                    _tx_thread_preempted_map_active =  _tx_thread_preempted_map_active & (~(priority_bit));
                }
#endif
            }
        }
#endif

        /* Return the user's preemption-threshold.   */
        *old_threshold =  thread_ptr -> tx_thread_user_preempt_threshold;

        /* Setup the new threshold.  */
        thread_ptr -> tx_thread_user_preempt_threshold =  new_threshold;

        /* Determine if the new threshold represents a higher priority than the priority inheritance threshold.  */
        if (new_threshold < thread_ptr -> tx_thread_inherit_priority)
        {
    
            /* Update the actual preemption-threshold with the new threshold.  */
            thread_ptr -> tx_thread_preempt_threshold =  new_threshold;
        }
        else
        {
    
            /* Update the actual preemption-threshold with the priority inheritance.  */
            thread_ptr -> tx_thread_preempt_threshold =  thread_ptr -> tx_thread_inherit_priority;
        }

        /* Is the thread priority less than the current highest priority?  If not, no preemption is required.  */
        if (_tx_thread_highest_priority < thread_ptr -> tx_thread_priority)
        {

            /* Is the new thread preemption-threshold less than the current highest priority?  If not, no preemption is required.  */
            if (_tx_thread_highest_priority < new_threshold)
            {

                /* If the current execute pointer is the same at this thread, preemption needs to take place.  */
                if (_tx_thread_execute_ptr == thread_ptr) 
                {

                    /* Preemption needs to take place.  */

#ifndef TX_DISABLE_PREEMPTION_THRESHOLD

                    /* Determine if this thread has preemption threshold set.  */
                    if (thread_ptr -> tx_thread_preempt_threshold != thread_ptr -> tx_thread_priority)
                    {

#if TX_MAX_PRIORITIES > 32

                        /* Calculate the index into the bit map array.  */
                        map_index =  (thread_ptr -> tx_thread_priority)/((UINT) 32);

                        /* Set the active bit to remember that the preempt map has something set.  */
                        TX_DIV32_BIT_SET(thread_ptr -> tx_thread_priority, priority_bit)
                        _tx_thread_preempted_map_active =  _tx_thread_preempted_map_active | priority_bit;
#endif

                        /* Remember that this thread was preempted by a thread above the thread's threshold.  */
                        TX_MOD32_BIT_SET(thread_ptr -> tx_thread_priority, priority_bit)
                        _tx_thread_preempted_maps[MAP_INDEX] =  _tx_thread_preempted_maps[MAP_INDEX] | priority_bit;
                    }
#endif

#ifdef TX_THREAD_ENABLE_PERFORMANCE_INFO

                    /* Determine if the caller is an interrupt or from a thread.  */
                    if (TX_THREAD_GET_SYSTEM_STATE() == ((ULONG) 0))
                    {

                        /* Caller is a thread, so this is a solicited preemption.  */
                        _tx_thread_performance_solicited_preemption_count++;

                        /* Increment the thread's solicited preemption counter.  */
                        thread_ptr -> tx_thread_performance_solicited_preemption_count++;
                    }

                    /* Remember the thread that preempted this thread.  */
                    thread_ptr -> tx_thread_performance_last_preempting_thread =  _tx_thread_priority_list[_tx_thread_highest_priority];

                    /* Is the execute pointer different?  */
                    if (_tx_thread_performance_execute_log[_tx_thread_performance__execute_log_index] != _tx_thread_execute_ptr)
                    {
                     
                        /* Move to next entry.  */
                        _tx_thread_performance__execute_log_index++;
            
                        /* Check for wrap condition.  */
                        if (_tx_thread_performance__execute_log_index >= TX_THREAD_EXECUTE_LOG_SIZE)
                        {
          
                            /* Set the index to the beginning.  */                  
                            _tx_thread_performance__execute_log_index =  ((UINT) 0);
                        }
            
                        /* Log the new execute pointer.  */
                        _tx_thread_performance_execute_log[_tx_thread_performance__execute_log_index] =  _tx_thread_execute_ptr;
                    }
#endif

                    /* Setup the highest priority thread to execute.  */
                    _tx_thread_execute_ptr =  _tx_thread_priority_list[_tx_thread_highest_priority];

                    /* Restore interrupts.  */
                    TX_RESTORE

                    /* Check for preemption.  */
                    _tx_thread_system_preempt_check();
                    
                    /* Disable interrupts.  */
                    TX_DISABLE
                }
            }
        }
    }

    /* Restore interrupts.  */
    TX_RESTORE
    
    /* Return completion status.  */
    return(status);
}
