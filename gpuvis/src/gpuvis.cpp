/*
 * Copyright 2019 Valve Software
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <string>
#include <array>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <sys/stat.h>
#include "LightSpeedApp.h"

#define YA_GETOPT_NO_COMPAT_MACRO
#include "ya_getopt.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"   // BeginColumns(), EndColumns(), PushColumnClipRect()

#include "MurmurHash3.h"

#define GPUVIS_TRACE_IMPLEMENTATION
#include "gpuvis_macros.h"

#include "tdopexpr.h"
#include "trace-cmd/trace-read.h"

#include "gpuvis_utils.h"
#include "gpuvis_etl.h"
#include "gpuvis.h"

#include "MiniConfig.h"

#include "miniz.h"

using namespace ci;

Clrs &s_clrs()
{
    static Clrs s_clrs;
    return s_clrs;
}

TextClrs &s_textclrs()
{
    static TextClrs s_textclrs;
    return s_textclrs;
}

Keybd &s_keybd()
{
    static Keybd s_keybd;
    return s_keybd;
}

Actions &s_actions()
{
    static Actions s_actions;
    return s_actions;
}

bool Opts::getcrtc( int crtc )
{
    return RenderCrtc0; // TODO:
}

uint64_t TraceLocationsRingCtxSeq::db_key( uint32_t ringno, uint32_t seqno, const char *ctxstr )
{
    if ( ringno != ( uint32_t )-1 )
    {
        uint64_t ctx = strtoul( ctxstr, NULL, 10 );

        // Try to create unique 64-bit key from ring/seq/ctx.
        struct {
            uint64_t ctx;
            uint64_t ringno;
            uint64_t seqno;
        } data = {
            ctx,
            ringno,
            seqno,
        };

        return MurmurHash3_x86_32( &data, sizeof(data), 0x42424242);
    }

    return 0;
}

uint64_t TraceLocationsRingCtxSeq::db_key( const trace_event_t &event )
{
    if ( event.seqno )
    {
        const char *ctxstr = get_event_field_val( event, "ctx", NULL );

        // i915:intel_engine_notify has only ring & seqno, so default ctx to "0"
        if ( !ctxstr && !strcmp( event.name, "intel_engine_notify" ) )
            ctxstr = "0";

        if ( ctxstr )
        {
            //return db_key( get_i915_ringno( event ), event.seqno, ctxstr );
        }
    }

    return 0;
}

bool TraceLocationsRingCtxSeq::add_location( const trace_event_t &event )
{
    uint64_t key = db_key( event );

    if ( key )
    {
        std::vector< uint32_t > *plocs = m_locs.get_val_create( key );

        plocs->push_back( event.id );
        return true;
    }

    return false;
}

std::vector< uint32_t > *TraceLocationsRingCtxSeq::get_locations( const trace_event_t &event )
{
    uint64_t key = db_key( event );

    return m_locs.get_val( key );
}

std::vector< uint32_t > *TraceLocationsRingCtxSeq::get_locations( uint32_t ringno, uint32_t seqno, const char *ctxstr )
{
    uint64_t key = db_key( ringno, seqno, ctxstr );

    return m_locs.get_val( key );
}

// See notes at top of gpuvis_graph.cpp for explanation of these events.
static bool is_amd_timeline_event( const trace_event_t &event )
{
    if ( !event.seqno )
        return false;

    const char *context = get_event_field_val( event, "context", NULL );
    const char *timeline = get_event_field_val( event, "timeline", NULL );

    if ( !context || !timeline )
        return false;

    return ( event.is_fence_signaled() ||
             !strcmp( event.name, "amdgpu_cs_ioctl" ) ||
             !strcmp( event.name, "amdgpu_sched_run_job" ) );
}

static bool is_drm_sched_timeline_event( const trace_event_t &event )
{
    return ( !strcmp( event.name, "drm_sched_job" ) ||
             !strcmp( event.name, "drm_run_job" ) ||
             !strcmp( event.name, "drm_sched_process_job" ) );
}

void add_sched_switch_pid_comm( trace_info_t &trace_info, const trace_event_t &event,
                                       const char *pidstr, const char *commstr )
{
    int pid = atoi( get_event_field_val( event, pidstr ) );

    if ( pid )
    {
        const char *comm = get_event_field_val( event, commstr );

        // If this pid is not in our pid_comm map or it is a sched_switch
        //  pid we already added, then map the pid -> the latest comm value
        if ( !trace_info.pid_comm_map.get_val( pid ) ||
             trace_info.sched_switch_pid_comm_map.get_val( pid ) )
        {
            // Add to pid_comm map
            trace_info.pid_comm_map.set_val( pid, comm );

            // And add to sched_switch pid_comm map
            trace_info.sched_switch_pid_comm_map.set_val( pid, comm );
        }
    }
}

static bool is_msm_timeline_event( const char *name )
{
    return ( !strcmp( name, "msm_gpu_submit_flush" ) ||
            !strcmp( name, "msm_gpu_submit_retired" ) );
}

TraceEvents::~TraceEvents()
{
    for ( trace_event_t &event : m_events )
    {
        if ( event.fields )
        {
            delete [] event.fields;
            event.fields = NULL;
            event.numfields = 0;
        }
    }
}

// Callback from trace_read.cpp. We mostly just store the events in our array
//  and then init_new_event() does the real work of initializing them later.
int TraceEvents::new_event_cb( const trace_event_t &event )
{
    // Add event to our m_events array
    m_events.push_back( event );

    // Record the maximum crtc value we've ever seen
    m_crtc_max = std::max< int >( m_crtc_max, event.crtc );

    // 1+ means loading events
    m_eventsloaded.fetch_add( 1 );

    // Return 1 to cancel loading
    return ( s_app().get_state() == LightSpeedApp::State_CancelLoading );
}

/*
 * TraceWin
 */
bool TraceWin::graph_marker_valid( int idx0 )
{
    return ( m_graph.ts_markers[ idx0 ] != INT64_MAX );
}

void TraceWin::graph_marker_set( size_t index, int64_t ts, const char *str )
{
    m_graph.ts_markers[ index ] = str ? timestr_to_ts( str ) : ts;

    if ( ts == INT64_MAX )
        m_graph.marker_bufs[ index ][ 0 ] = 0;
    else
        strcpy_safe( m_graph.marker_bufs[ index ],
                     ts_to_timestr( m_graph.ts_markers[ index ], 4 ).c_str() );

    if ( graph_marker_valid( 0 ) && graph_marker_valid( 1 ) )
    {
        strcpy_safe( m_graph.marker_delta_buf,
                     ts_to_timestr( m_graph.ts_markers[ 1 ] - m_graph.ts_markers[ 0 ], 4 ).c_str() );
    }
}

uint32_t TraceWin::ts_to_eventid( int64_t ts )
{
    // When running a debug build w/ ASAN on, the lower_bound function is
    //  horribly slow so we cache the timestamp to event ids.
    uint32_t *peventid = m_ts_to_eventid_cache.get_val( ts );
    if ( peventid )
        return *peventid;

    trace_event_t x;
    const std::vector< trace_event_t > &events = m_trace_events.m_events;

    x.ts = ts;

    auto eventidx = std::lower_bound( events.begin(), events.end(), x,
        []( const trace_event_t &f1, const trace_event_t &f2 ) {
            return f1.ts < f2.ts;
        } );

    uint32_t id = eventidx - events.begin();

    if ( id >= events.size() )
        id = events.size() - 1;

    m_ts_to_eventid_cache.set_val( ts, id );
    return id;
}

uint32_t TraceWin::timestr_to_eventid( const char *buf )
{
    int64_t ts = timestr_to_ts( buf );

    return ts_to_eventid( ts );
}

const char *filter_get_key_func( StrPool *strpool, const char *name, size_t len )
{
    return strpool->getstr( name, len );
}

const char *filter_get_keyval_func( trace_info_t *trace_info, const trace_event_t *event,
                                    const char *name, char ( &buf )[ 64 ] )
{
    if ( !strcasecmp( name, "name" ) )
    {
        return event->name;
    }
    else if ( !strcasecmp( name, "comm" ) )
    {
        return event->comm;
    }
    else if ( !strcasecmp( name, "user_comm" ) )
    {
        return event->user_comm;
    }
    else if ( !strcasecmp( name, "id" ) )
    {
        snprintf_safe( buf, "%u", event->id );
        return buf;
    }
    else if ( !strcasecmp( name, "pid" ) )
    {
        snprintf_safe( buf, "%d", event->pid );
        return buf;
    }
    else if ( !strcasecmp( name, "tgid" ) )
    {
        int *tgid = trace_info->pid_tgid_map.get_val( event->pid );

        snprintf_safe( buf, "%d", tgid ? *tgid : 0 );
        return buf;
    }
    else if ( !strcasecmp( name, "ts" ) )
    {
        snprintf_safe( buf, "%.6f", event->ts * ( 1.0 / NSECS_PER_MSEC ) );
        return buf;
    }
    else if ( !strcasecmp( name, "cpu" ) )
    {
        snprintf_safe( buf, "%u", event->cpu );
        return buf;
    }
    else if ( !strcasecmp( name, "duration" ) )
    {
        if ( !event->has_duration() )
            buf[ 0 ] = 0;
        else
            snprintf_safe( buf, "%.6f", event->duration * ( 1.0 / NSECS_PER_MSEC ) );
        return buf;
    }

    for ( uint32_t i = 0; i < event->numfields; i++ )
    {
        const event_field_t &field = event->fields[ i ];

        // We can compare pointers since they're from same string pool
        if ( name == field.key )
            return field.value;
    }

    return "";
}

const std::vector< uint32_t > *TraceEvents::get_tdopexpr_locs( const char *name, std::string *err )
{
    std::vector< uint32_t > *plocs;
    uint32_t hashval = hashstr32( name );

    if ( err )
        err->clear();

    // Try to find whatever our name hashed to. Name should be something like:
    //   $name=drm_vblank_event
    plocs = m_tdopexpr_locs.get_locations_u32( hashval );
    if ( plocs )
        return plocs;

    // Not found - check if we've tried and failed with this name before.
    if ( m_failed_commands.find( hashval ) != m_failed_commands.end() )
        return NULL;

    // If the name has a tdop expression variable prefix, try compiling it
    if ( strchr( name, '$' ) )
    {
        std::string errstr;
        tdop_get_key_func get_key_func = std::bind( filter_get_key_func, &m_strpool, _1, _2 );
        class TdopExpr *tdop_expr = tdopexpr_compile( name, get_key_func, errstr );

        if ( !tdop_expr )
        {
            if ( err )
                *err = errstr;
            else
                logf( "[Error] compiling '%s': %s", name, errstr.c_str() );
        }
        else
        {
            for ( trace_event_t &event : m_events )
            {
                const char *ret;
                tdop_get_keyval_func get_keyval_func = std::bind( filter_get_keyval_func, &m_trace_info, &event, _1, _2 );

                ret = tdopexpr_exec( tdop_expr, get_keyval_func );
                if ( ret[ 0 ] )
                    m_tdopexpr_locs.add_location_u32( hashval, event.id );
            }

            tdopexpr_delete( tdop_expr );
        }
    }

    // Try to find this name/expression again and add to failed list if we miss again
    plocs = m_tdopexpr_locs.get_locations_u32( hashval );
    if ( !plocs )
        m_failed_commands.insert( hashval );

    return plocs;
}

const std::vector< uint32_t > *TraceEvents::get_comm_locs( const char *name )
{
    return m_comm_locs.get_locations_str( name );
}

const std::vector< uint32_t > *TraceEvents::get_sched_switch_locs( int pid, switch_t switch_type )
{
    return ( switch_type == SCHED_SWITCH_PREV ) ?
                m_sched_switch_prev_locs.get_locations_u32( pid ) :
                m_sched_switch_next_locs.get_locations_u32( pid );
}

const std::vector< uint32_t > *TraceEvents::get_timeline_locs( const char *name )
{
    return m_amd_timeline_locs.get_locations_str( name );
}

// Pass a string like "gfx_249_91446"
const std::vector< uint32_t > *TraceEvents::get_gfxcontext_locs( uint32_t gfxcontext_hash )
{
    return m_gfxcontext_locs.get_locations_u32( gfxcontext_hash );
}

uint32_t row_pos_t::get_row( int64_t min_ts, int64_t max_ts )
{
    uint32_t row = 0;

    for ( ; row < m_row_pos.size(); row++ )
    {
        int64_t ts_end_prev = INT64_MIN;
        int64_t ts_start_next = INT64_MAX;

        // Find first element with start time >= min_ts
        auto &rpos = m_row_pos[ row ];
        auto idx = rpos.lower_bound( min_ts );

        if ( idx != rpos.end() )
        {
            // Got item with start time >= our min_ts.
            ts_start_next = idx->first;

            if ( idx != rpos.begin() )
            {
                // Get previous item end time
                --idx;
                ts_end_prev = idx->second;
            }
        }
        else
        {
            // No items start after us - grab last item in map
            auto idx2 = rpos.rbegin();

            // Get end of last item ever
            if ( idx2 != rpos.rend() )
                ts_end_prev = idx2->second;
        }

        // If start of next item is greater than our end
        // And end of previous item is less than our start
        if ( ( ts_start_next >= max_ts ) && ( ts_end_prev <= min_ts ) )
            break;
    }

    if ( row >= m_row_pos.size() )
        row = 0;

    auto &rpos = m_row_pos[ row ];
    rpos[ min_ts ] = max_ts;

    m_rows = std::max< uint32_t >( m_rows, row + 1 );
    return row;
}

void TraceEvents::update_fence_signaled_timeline_colors()
{
    float label_sat = s_clrs().getalpha( col_Graph_TimelineLabelSat );
    float label_alpha = s_clrs().getalpha( col_Graph_TimelineLabelAlpha );

    for ( auto &timeline_locs : m_amd_timeline_locs.m_locs.m_map )
    {
        std::vector< uint32_t > &locs = timeline_locs.second;

        for ( uint32_t index : locs )
        {
            trace_event_t &fence_signaled = m_events[ index ];

            if ( fence_signaled.is_fence_signaled() &&
                 is_valid_id( fence_signaled.id_start ) )
            {
                uint32_t hashval = hashstr32( fence_signaled.user_comm );

                // Mark this event as autogen'd color so it doesn't get overwritten
                fence_signaled.flags |= TRACE_FLAG_AUTOGEN_COLOR;
                fence_signaled.color = imgui_col_from_hashval( hashval, label_sat, label_alpha );
            }
        }
    }
}

void TraceEvents::update_tgid_colors()
{
    float label_sat = s_clrs().getalpha( col_Graph_PrintLabelSat );
    float label_alpha = s_clrs().getalpha( col_Graph_PrintLabelAlpha );

    for ( auto &it : m_trace_info.tgid_pids.m_map )
    {
        tgid_info_t &tgid_info = it.second;

        tgid_info.color = imgui_col_from_hashval( tgid_info.hashval,
                                                  label_sat, label_alpha );

        TextClr clr( tgid_info.color );
        const char *commstr = comm_from_pid( tgid_info.tgid, "<...>" );

        tgid_info.commstr_clr = m_strpool.getstrf( "%s%s%s", clr.str(),
                                                   commstr, s_textclrs().str( TClr_Def ) );
        tgid_info.commstr = commstr;
    }

    for ( const auto &cpu_locs : m_sched_switch_cpu_locs.m_locs.m_map )
    {
        const std::vector< uint32_t > &locs = cpu_locs.second;

        for ( uint32_t idx : locs )
        {
            uint32_t hashval;
            float alpha = label_alpha;
            size_t len = ( size_t )-1;
            trace_event_t &sched_switch = m_events[ idx ];
            const char *prev_comm = get_event_field_val( sched_switch, "prev_comm" );
            const char *prev_pid = get_event_field_val( sched_switch, "prev_pid" );

            if ( !strncmp( prev_comm,      "swapper/", 8 ) )
                len = 8;
            else if ( !strncmp( prev_comm, "kworker/", 8 ) )
                len = 8;
            else if ( !strncmp( prev_comm, "watchdog/", 9 ) )
                len = 9;
            else if ( !strncmp( prev_comm, "ksoftirqd/", 10 ) )
                len = 10;
            else if ( !strncmp( prev_comm, "migration/", 10 ) )
                len = 10;

            if ( len != ( size_t )-1 )
            {
                sched_switch.flags |= TRACE_FLAG_SCHED_SWITCH_SYSTEM_EVENT;
                alpha = 0.3f;
            }

            // Hash comm and pid to get color
            hashval = hashstr32( prev_comm, len );

            // If this is a system event, just use the prev_comm entry
            if ( !( sched_switch.flags & TRACE_FLAG_SCHED_SWITCH_SYSTEM_EVENT ) )
                hashval = hashstr32( prev_pid, ( size_t )-1, hashval );

            sched_switch.flags |= TRACE_FLAG_AUTOGEN_COLOR;
            sched_switch.color = imgui_col_from_hashval( hashval, label_sat, alpha );
        }
    }
}

const char *TraceEvents::comm_from_pid( int pid, const char *def )
{
    const char *const *comm = m_trace_info.pid_comm_map.get_val( pid );

    if ( !comm && !def )
        return NULL;

    return m_strpool.getstrf( "%s-%d", comm ? *comm : def, pid );
}

const char *TraceEvents::tgidcomm_from_pid( int pid )
{
    const char **mapped_comm = m_pid_commstr_map.get_val( pid );

    if ( mapped_comm )
        return *mapped_comm;

    const tgid_info_t *tgid_info = tgid_from_pid( pid );
    const char *comm = comm_from_pid( pid, "<...>" );

    if ( tgid_info )
    {
        comm = m_strpool.getstrf( "%s (%s)", comm, tgid_info->commstr_clr );
    }

    // Add pid / comm mapping
    m_pid_commstr_map.get_val( pid, comm );

    return comm;
}

const char *TraceEvents::tgidcomm_from_commstr( const char *comm )
{
    // Parse comm string to get pid. Ie: mainthread-1324
    const char *pidstr = comm ? strrchr( comm, '-' ) : NULL;

    if ( pidstr )
    {
        int pid = atoi( pidstr + 1 );

        return tgidcomm_from_pid( pid );
    }

    return comm;
}

const tgid_info_t *TraceEvents::tgid_from_pid( int pid )
{
    int *tgid = m_trace_info.pid_tgid_map.get_val( pid );

    return tgid ? m_trace_info.tgid_pids.get_val( *tgid ) : NULL;
}

const tgid_info_t *TraceEvents::tgid_from_commstr( const char *comm )
{
    const char *pidstr = comm ? strrchr( comm, '-' ) : NULL;

    if ( pidstr )
    {
        int pid = atoi( pidstr + 1 );

        return tgid_from_pid( pid );
    }

    return NULL;
}

uint32_t TraceEvents::get_event_gfxcontext_hash( const trace_event_t &event )
{
    if ( is_msm_timeline_event( event.name ) )
    {
        return atoi( get_event_field_val( event, "seqno", "0" ) );
    }

    if ( is_drm_sched_timeline_event( event ) )
    {
        return event.seqno;
    }

    if ( event.seqno )
    {
        const char *context = get_event_field_val( event, "context", NULL );
        const char *timeline = get_event_field_val( event, "timeline", NULL );

        if ( timeline && context )
        {
            char buf[ 128 ];

            snprintf_safe( buf, "%s_%s_%u", timeline, context, event.seqno );
            return hashstr32( buf );
        }
    }

    return 0;
}

std::string TraceEvents::get_ftrace_ctx_str( const trace_event_t &event )
{
    if ( event.seqno != UINT32_MAX )
    {
        return string_format( " %s[ctx=%u]%s", s_textclrs().str( TClr_Bright ),
                              event.seqno, s_textclrs().str( TClr_Def ) );
    }

    return "";
}

void TraceEvents::init_sched_switch_event( trace_event_t &event )
{
    const char *prev_pid_str = get_event_field_val( event, "prev_pid" );
    const char *next_pid_str = get_event_field_val( event, "next_pid" );

    if ( *prev_pid_str && *next_pid_str )
    {
        int prev_pid = atoi( prev_pid_str );
        int next_pid = atoi( next_pid_str );
        const std::vector< uint32_t > *plocs;

        // Seems that sched_switch event.pid is equal to the event prev_pid field.
        // We're running with this in several bits of code in gpuvis_graph, so assert it's true.
        assert( prev_pid == event.pid );

        // Look in the sched_switch next queue for an event that said we were starting up.
        plocs = get_sched_switch_locs( prev_pid, TraceEvents::SCHED_SWITCH_NEXT );
        if ( plocs )
        {
            const trace_event_t &event_prev = m_events[ plocs->back() ];

            // TASK_RUNNING (0): On the run queue
            // TASK_INTERRUPTABLE (1): Sleeping but can be woken up
            // TASK_UNINTERRUPTABLE (2): Sleeping but can't be woken up by a signal
            // TASK_STOPPED (4): Stopped process by job control signal or ptrace
            // TASK_TRACED (8): Task is being monitored by other process (such as debugger)
            // TASK_ZOMBIE (32): Finished but waiting for parent to call wait() to cleanup
            int prev_state = atoi( get_event_field_val( event, "prev_state" ) );
            int task_state = prev_state & ( TASK_REPORT_MAX - 1 );

            if ( task_state == 0 )
                event.flags |= TRACE_FLAG_SCHED_SWITCH_TASK_RUNNING;

            event.duration = event.ts - event_prev.ts;

            m_sched_switch_time_total += event.duration;
            m_sched_switch_time_pid.m_map[ prev_pid ] += event.duration;

            // Add this event to the sched switch CPU timeline locs array
            m_sched_switch_cpu_locs.add_location_u32( event.cpu, event.id );
        }

        m_sched_switch_prev_locs.add_location_u32( prev_pid, event.id );
        m_sched_switch_next_locs.add_location_u32( next_pid, event.id );

        //$ TODO mikesart: This is messing up the m_comm_locs event counts
        if ( prev_pid != event.pid )
        {
            const char *comm = comm_from_pid( prev_pid );
            if ( comm )
                m_comm_locs.add_location_str( comm, event.id );
        }
        if ( next_pid != event.pid )
        {
            const char *comm = comm_from_pid( next_pid );
            if ( comm )
                m_comm_locs.add_location_str( comm, event.id );
        }
    }
}

void TraceEvents::init_sched_process_fork( trace_event_t &event )
{
    // parent_comm=glxgears parent_pid=23543 child_comm=glxgears child_pid=23544
    int tgid = atoi( get_event_field_val( event, "parent_pid", "0" ) );
    int pid = atoi( get_event_field_val( event, "child_pid", "0" ) );
    const char *tgid_comm = get_event_field_val( event, "parent_comm", NULL );
    const char *child_comm = get_event_field_val( event, "child_comm", NULL );

    if ( tgid && pid && tgid_comm && child_comm )
    {
        tgid_info_t *tgid_info = m_trace_info.tgid_pids.get_val_create( tgid );

        if ( !tgid_info->tgid )
        {
            tgid_info->tgid = tgid;
            tgid_info->hashval += hashstr32( tgid_comm );
        }
        tgid_info->add_pid( tgid );
        tgid_info->add_pid( pid );

        // Add to our pid --> comm map
        m_trace_info.pid_comm_map.get_val( tgid, tgid_comm );
        m_trace_info.pid_comm_map.get_val( pid, child_comm );

        // tgid --> tgid, pid --> tgid
        m_trace_info.pid_tgid_map.get_val( tgid, tgid );
        m_trace_info.pid_tgid_map.get_val( pid, tgid );
    }
}

void TraceEvents::init_amd_timeline_event( trace_event_t &event )
{
    uint32_t gfxcontext_hash = get_event_gfxcontext_hash( event );
    const char *timeline = get_event_field_val( event, "timeline" );

    // Add this event under the "gfx", "sdma0", etc timeline map
    m_amd_timeline_locs.add_location_str( timeline, event.id );

    // Add this event under our "gfx_ctx_seq" or "sdma0_ctx_seq", etc. map
    m_gfxcontext_locs.add_location_u32( gfxcontext_hash, event.id );

    // Grab the event locations for this event context
    const std::vector< uint32_t > *plocs = get_gfxcontext_locs( gfxcontext_hash );
    if ( plocs->size() > 1 )
    {
        // First event.
        const trace_event_t &event0 = m_events[ plocs->front() ];

        // Event right before the event we just added.
        auto it = plocs->rbegin() + 1;
        const trace_event_t &event_prev = m_events[ *it ];

        // Assume the user comm is the first comm event in this set.
        event.user_comm = event0.comm;

        // Point the event we just added to the previous event in this series
        event.id_start = event_prev.id;

        if ( event.is_fence_signaled() )
        {
            // Mark all the events in this series as timeline events
            for ( uint32_t idx : *plocs )
            {
                m_events[ idx ].flags |= TRACE_FLAG_TIMELINE;
            }
        }
    }
}

void TraceEvents::init_msm_timeline_event( trace_event_t &event )
{
    uint32_t gfxcontext_hash = get_event_gfxcontext_hash( event );

    int ringid = atoi( get_event_field_val( event, "ringid", "0" ) );
    std::string str = string_format( "msm ring%d", ringid );

    m_amd_timeline_locs.add_location_str( str.c_str(), event.id );

    m_gfxcontext_locs.add_location_u32( gfxcontext_hash, event.id );

    event.flags |= TRACE_FLAG_TIMELINE;

    event.id_start = INVALID_ID;

    if ( !strcmp( event.name, "msm_gpu_submit_retired" ) )
    {
        event.flags |= TRACE_FLAG_FENCE_SIGNALED;
    }
    else if ( !strcmp( event.name, "msm_gpu_submit_flush" ) )
    {
        event.flags |= TRACE_FLAG_HW_QUEUE;
    }

    const std::vector< uint32_t > *plocs = m_gfxcontext_locs.get_locations_u32( gfxcontext_hash );
    if ( plocs->size() > 1 )
    {
        // First event.
        trace_event_t &event0 = m_events[ plocs->front() ];

        // Assume the user comm is the first comm event in this set.
        event.user_comm = event0.comm;

        // We shouldn't recycle seqnos in the same trace hopefully?
        event.id_start = event0.id;
    }
}

void TraceEvents::init_drm_sched_timeline_event( trace_event_t &event )
{
    std::string str;
    const char *ring;
    uint32_t fence;

    fence = strtoul( get_event_field_val( event, "fence", "0" ), nullptr, 0 );
    event.flags |= TRACE_FLAG_TIMELINE;

    if ( !strcmp( event.name, "drm_sched_job" ) )
    {
        event.flags |= TRACE_FLAG_SW_QUEUE;
        event.id_start = INVALID_ID;
        event.graph_row_id = atoi( get_event_field_val( event, "job_count", "0" ) ) +
                             atoi( get_event_field_val( event, "hw_job_count", "0" ) );
        event.seqno = strtoul( get_event_field_val( event, "id", "0" ), nullptr, 0 );
        m_drm_sched.outstanding_jobs[fence] = event.seqno;
        ring = get_event_field_val( event, "name", "<unknown>" );
        str = string_format( "drm sched %s", ring );
        m_drm_sched.rings.insert(str);
        m_amd_timeline_locs.add_location_str( str.c_str(), event.id );
        m_gfxcontext_locs.add_location_u32( event.seqno, event.id );
        return;
    }

    auto job = m_drm_sched.outstanding_jobs.find( fence );
    if ( job == m_drm_sched.outstanding_jobs.end() ) {
        // no in flight job. This event will be dropped
        return;
    }

    const std::vector< uint32_t > *plocs = get_gfxcontext_locs( job->second );
    if ( plocs->size()  < 1 )
    {
        // no previous start event. This event will be dropped
        return;
    }

    if ( !strcmp( event.name, "drm_run_job" ) )
    {
        for ( auto it = plocs->rbegin(); it != plocs->rend(); ++it )
        {
            const trace_event_t &e = m_events[ *it ];
            if ( e.flags & TRACE_FLAG_FENCE_SIGNALED )
            {
                // if we hit an end event, we should have already found a start
                // event. Ignore this event, it will be dropped
                return;
            }

            if ( e.flags & TRACE_FLAG_SW_QUEUE )
            {
                ring = get_event_field_val( e, "name", "<unknown>" );
                str = string_format( "drm sched %s", ring );
                m_drm_sched.rings.insert( str );
                m_amd_timeline_locs.add_location_str( str.c_str(), event.id );
                event.user_comm = e.comm;
                event.id_start = e.id;
                event.flags |= TRACE_FLAG_HW_QUEUE;
                event.graph_row_id = e.graph_row_id;
                event.seqno = e.seqno;
                m_gfxcontext_locs.add_location_u32( event.seqno, event.id );
                break;
            }
        }
    }

    if ( !strcmp( event.name, "drm_sched_process_job" ) )
    {
        // fence can be reused across multiple jobs, but never at the same
        // time. Find the previous event with TRACE_FLAG_SW_QUEUE as start event.
        for ( auto it = plocs->rbegin(); it != plocs->rend(); ++it )
        {
            const trace_event_t &e = m_events[ *it ];
            if ( e.flags & TRACE_FLAG_FENCE_SIGNALED )
            {
                // if we hit an end event, we should have already found a start
                // event. Ignore this event, it will be dropped
                return;
            }

            if ( e.flags & TRACE_FLAG_HW_QUEUE )
            {
                ring = get_event_field_val( e, "name", "<unknown>" );
                str = string_format( "drm sched %s", ring );
                m_drm_sched.rings.insert( str );
                m_amd_timeline_locs.add_location_str( str.c_str(), event.id );
                event.user_comm = e.comm;
                event.id_start = e.id;
                event.flags |= TRACE_FLAG_FENCE_SIGNALED;
                event.graph_row_id = e.graph_row_id;
                event.seqno = e.seqno;
                m_gfxcontext_locs.add_location_u32( event.seqno, event.id );
                m_drm_sched.outstanding_jobs.erase( fence );
                break;
            }
        }
    }
}

static int64_t normalize_vblank_diff( int64_t diff )
{
    static const int64_t rates[] =
    {
        66666666, // 15Hz
        33333333, // 30Hz
        16666666, // 60Hz
        11111111, // 90Hz
        10526315, // 95Hz
        8333333,  // 120Hz
        6944444,  // 144Hz
        6060606,  // 165Hz
        4166666,  // 240Hz
    };

    for ( size_t i = 0; i < ARRAY_SIZE( rates ); i++ )
    {
        int64_t pct = 10000 * ( diff - rates[ i ]  ) / rates[ i ];

        // If the diff is < 1.0% off this common refresh rate, use it.
        if ( ( pct > -100 ) && ( pct < 100 ) )
            return rates[ i ];
    }

    return diff;
}

void TraceEvents::init_new_event_vblank( trace_event_t &event )
{
    // See if we have a drm_vblank_event_queued with the same seq number
    uint32_t seqno = strtoul( get_event_field_val( event, "seq" ), NULL, 10 );
    uint32_t *vblank_queued_id = m_drm_vblank_event_queued.get_val( seqno );

    if ( vblank_queued_id )
    {
        trace_event_t &event_vblank_queued = m_events[ *vblank_queued_id ];

        // If so, set the vblank queued time
        event_vblank_queued.duration = event.get_vblank_ts( VBlankHighPrecTimestamps ) - event_vblank_queued.ts;
    }

    m_tdopexpr_locs.add_location_str( "$name=drm_vblank_event", event.id );

    /*
     * vblank interval calculations
     */
    if ( m_vblank_info[ event.crtc ].last_vblank_ts )
    {
        int64_t diff = event.get_vblank_ts( VBlankHighPrecTimestamps ) - m_vblank_info[ event.crtc ].last_vblank_ts;

        // Normalize ts diff to known frequencies
        diff = normalize_vblank_diff( diff );

        // Bump count for this diff ts value
        m_vblank_info[ event.crtc ].diff_ts_count[ diff / 1000 ]++;
        m_vblank_info[ event.crtc ].count++;
    }

    m_vblank_info[ event.crtc ].last_vblank_ts = event.get_vblank_ts( VBlankHighPrecTimestamps );
}

// new_event_cb adds all events to array, this function initializes them.
void TraceEvents::init_new_event( trace_event_t &event )
{
    // If our pid is in the sched_switch pid map, update our comm to the sched_switch
    // value that it recorded.
    const char **comm = m_trace_info.sched_switch_pid_comm_map.get_val( event.pid );
    if ( comm )
    {
        event.comm = m_strpool.getstrf( "%s-%d", *comm, event.pid );
    }

    if ( event.is_vblank() )
    {
        init_new_event_vblank( event );
    }
    else if ( !strcmp( event.name, "drm_vblank_event_queued" ) )
    {
        uint32_t seqno = strtoul( get_event_field_val( event, "seq" ), NULL, 10 );

        if ( seqno )
            m_drm_vblank_event_queued.set_val( seqno, event.id );
    }

    // Add this event name to event name map
    if ( event.is_vblank() )
    {
        // Add vblanks as "drm_vblank_event1", etc
        uint32_t hashval = m_strpool.getu32f( "%s%d", event.name, event.crtc );

        m_eventnames_locs.add_location_u32( hashval, event.id );
    }
    else
    {
        m_eventnames_locs.add_location_str( event.name, event.id );
    }

    if ( !strcmp( event.name, "sched_process_exec" ) )
    {
        // pid, old_pid, filename
        const char *filename = get_event_field_val( event, "filename" );

        filename = strrchr( filename, '/' );
        if ( filename )
        {
            // Add pid --> comm map if it doesn't already exist
            filename = m_strpool.getstr( filename + 1 );
            m_trace_info.pid_comm_map.get_val( event.pid, filename );
        }
    }
    else if ( !strcmp( event.name, "sched_process_exit" ) )
    {
        const char *pid_comm = get_event_field_val( event, "comm", NULL );

        if ( pid_comm )
            m_trace_info.pid_comm_map.set_val( event.pid, pid_comm );
    }
#if 0
    // Disabled for now. Need to figure out how to prevent sudo, bash, etc from becoming the parent. Ie:
    //    <...>-7860  [021]  3726.235512: sched_process_fork:   comm=sudo pid=7860 child_comm=sudo child_pid=7861
    //    <...>-7861  [010]  3726.825033: sched_process_fork:   comm=glxgears pid=7861 child_comm=glxgears child_pid=7862
    //    <...>-7861  [010]  3726.825304: sched_process_fork:   comm=glxgears pid=7861 child_comm=glxgears child_pid=7863
    else if ( !strcmp( event.name, "sched_process_fork" ) )
    {
        init_sched_process_fork( event );
    }
#endif

    if ( event.is_sched_switch() )
    {
        init_sched_switch_event( event );
    }
    else if ( is_amd_timeline_event( event ) )
    {
        init_amd_timeline_event( event );
    }
    else if ( is_drm_sched_timeline_event( event ) )
    {
        init_drm_sched_timeline_event( event );
    }
    else if ( is_msm_timeline_event( event.name ) )
    {
        init_msm_timeline_event( event );
    }

    if ( !strcmp( event.name, "amdgpu_job_msg" ) )
    {
        const char *msg = get_event_field_val( event, "msg", NULL );
        uint32_t gfxcontext_hash = get_event_gfxcontext_hash( event );

        if ( msg && msg[ 0 ] && gfxcontext_hash )
            m_gfxcontext_msg_locs.add_location_u32( gfxcontext_hash, event.id );
    }

    // 1+ means loading events
    m_eventsloaded.fetch_add( 1 );
}

TraceEvents::tracestatus_t TraceEvents::get_load_status( uint32_t *count )
{
    int eventsloaded = m_eventsloaded.load();

    if ( eventsloaded > 0 )
    {
        if ( count )
            *count = eventsloaded & ~0x40000000;

        return ( eventsloaded & 0x40000000 ) ?
                    Trace_Initializing : Trace_Loading;
    }
    else if ( !eventsloaded )
    {
        if ( count )
            *count = m_events.size();
        return Trace_Loaded;
    }

    if ( count )
        *count = 0;
    return Trace_Error;
}

void TraceEvents::calculate_vblank_info()
{
    // Go through all the vblank crtcs
    for ( uint32_t i = 0; i < m_vblank_info.size(); i++ )
    {
        if ( !m_vblank_info[ i ].count )
            continue;

        uint32_t median = m_vblank_info[ i ].count / 2;

        for ( const auto &x : m_vblank_info[ i ].diff_ts_count )
        {
            if ( x.second >= median )
            {
                // This is the median tsdiff
                int64_t diff = x.first * 1000;

                m_vblank_info[ i ].median_diff_ts = diff;

                std::string str = ts_to_timestr( diff, 2 );
                const std::string desc = string_format( "Show vblank crtc%u markers (~%s)", i, str.c_str() );

                // s_opts().setdesc( OPT_RenderCrtc0 + i, desc );
                break;
            }

            median -= x.second;
        }
    }
}

void TraceEvents::init()
{
    // Set m_eventsloaded initializing bit
    m_eventsloaded.store(0x40000000 );

    m_vblank_info.resize( m_crtc_max + 1 );

    //s_opts().set_crtc_max( m_crtc_max );

    {
        // Initialize events...
        GPUVIS_TRACE_BLOCKF( "init_new_events: %lu events", m_events.size() );

        for ( trace_event_t &event : m_events )
            init_new_event( event );
    }

    // Figure out median vblank intervals
    calculate_vblank_info();

    // Init amd event durations
    calculate_amd_event_durations();

    // Init print column information
    calculate_event_print_info();

    // Remove tgid groups with single threads
    remove_single_tgids();

    // Update tgid colors
    update_tgid_colors();

#if 0
    std::vector< INIEntry > entries = s_ini().GetSectionEntries( "$imgui_eventcolors$" );

    // Restore event colors
    for ( const INIEntry &entry : entries )
    {
        const std::string &eventname = entry.first;
        const std::string &val = entry.second;

        if ( !val.empty() )
        {
            uint32_t color = std::stoull( val, NULL, 0 );

            set_event_color( eventname.c_str(), color );
        }
    }
#endif
}

void TraceEvents::remove_single_tgids()
{
    std::unordered_map< int, tgid_info_t > &tgid_pids = m_trace_info.tgid_pids.m_map;

    for ( auto it = tgid_pids.begin(); it != tgid_pids.end(); )
    {
        tgid_info_t &tgid_info = it->second;

        // If this tgid has only itself as a thread, remove it
        if ( ( tgid_info.pids.size() == 1 ) && ( tgid_info.pids[ 0 ] == tgid_info.tgid ) )
            it = tgid_pids.erase( it );
        else
            it++;
    }
}

void TraceEvents::set_event_color( const std::string &eventname, ImU32 color )
{
    const std::vector< uint32_t > *plocs =
            m_eventnames_locs.get_locations_str( eventname.c_str() );

    if ( plocs )
    {
        //s_ini().PutUint64( eventname.c_str(), color, "$imgui_eventcolors$" );

        for ( uint32_t idx : *plocs )
        {
            trace_event_t &event = m_events[ idx ];

            // If it's not an autogen'd color, set new color
            if ( !( event.flags & TRACE_FLAG_AUTOGEN_COLOR ) )
                event.color = color;
        }
    }
}

/*
  From conversations with Andres and Pierre-Loup...

  These are the important events:

  amdgpu_cs_ioctl:
    this event links a userspace submission with a kernel job
    it appears when a job is received from userspace
    dictates the userspace PID for the whole unit of work
      ie, the process that owns the work executing on the gpu represented by the bar
    only event executed within the context of the userspace process

  amdgpu_sched_run_job:
    links a job to a dma_fence object, the queue into the HW event
    start of the bar in the gpu timeline; either right now if no job is running, or when the currently running job finishes

  *fence_signaled:
    job completed
    dictates the end of the bar

  notes:
    amdgpu_cs_ioctl and amdgpu_sched_run_job have a common job handle

  We want to match: timeline, context, seqno.

    There are separate timelines for each gpu engine
    There are two dma timelines (one per engine)
    And 8 compute timelines (one per hw queue)
    They are all concurrently executed
      Most apps will probably only have a gfx timeline
      So if you populate those lazily it should avoid clogging the ui

  Andres warning:
    btw, expect to see traffic on some queues that was not directly initiated by an app
    There is some work the kernel submits itself and that won't be linked to any cs_ioctl

  Example:

  ; userspace submission
    SkinningApp-2837 475.1688: amdgpu_cs_ioctl:      sched_job=185904, timeline=gfx, context=249, seqno=91446, ring_name=ffff94d7a00d4694, num_ibs=3

  ; gpu starting job
            gfx-477  475.1689: amdgpu_sched_run_job: sched_job=185904, timeline=gfx, context=249, seqno=91446, ring_name=ffff94d7a00d4694, num_ibs=3

  ; job completed
         <idle>-0    475.1690: fence_signaled:       driver=amd_sched timeline=gfx context=249 seqno=91446
 */
void TraceEvents::calculate_amd_event_durations()
{
    std::vector< uint32_t > erase_list;
    std::vector< trace_event_t > &events = m_events;
    float label_sat = s_clrs().getalpha( col_Graph_TimelineLabelSat );
    float label_alpha = s_clrs().getalpha( col_Graph_TimelineLabelAlpha );

    // Go through gfx, sdma0, sdma1, etc. timelines and calculate event durations
    for ( auto &timeline_locs : m_amd_timeline_locs.m_locs.m_map )
    {
        uint32_t graph_row_id = 0;
        int64_t last_fence_signaled_ts = 0;
        std::vector< uint32_t > &locs = timeline_locs.second;
        // const char *name = m_strpool.findstr( timeline_locs.first );

        // Erase all timeline events with single entries or no fence_signaled
        locs.erase( std::remove_if( locs.begin(), locs.end(),
                                    [&events]( const uint32_t index )
                                        { return !events[ index ].is_timeline(); }
                                  ),
                    locs.end() );

        if ( locs.empty() )
            erase_list.push_back( timeline_locs.first );

        for ( uint32_t index : locs )
        {
            trace_event_t &fence_signaled = events[ index ];

            if ( fence_signaled.is_fence_signaled() &&
                 is_valid_id( fence_signaled.id_start ) )
            {
                trace_event_t &amdgpu_sched_run_job = events[ fence_signaled.id_start ];
                int64_t start_ts = amdgpu_sched_run_job.ts;

                // amdgpu_cs_ioctl   amdgpu_sched_run_job   fence_signaled
                //       |-----------------|---------------------|
                //       |user-->          |hw-->                |
                //                                               |
                //          amdgpu_cs_ioctl  amdgpu_sched_run_job|   fence_signaled
                //                |-----------------|------------|--------|
                //                |user-->          |hwqueue-->  |hw->    |
                //                                                        |

                // Our starting location will be the last fence signaled timestamp or
                //  our amdgpu_sched_run_job timestamp, whichever is larger.
                int64_t hw_start_ts = std::max< int64_t >( last_fence_signaled_ts, amdgpu_sched_run_job.ts );

                // Set duration times
                fence_signaled.duration = fence_signaled.ts - hw_start_ts;
                amdgpu_sched_run_job.duration = hw_start_ts - amdgpu_sched_run_job.ts;

                if ( is_valid_id( amdgpu_sched_run_job.id_start ) )
                {
                    trace_event_t &amdgpu_cs_ioctl = events[ amdgpu_sched_run_job.id_start ];

                    amdgpu_cs_ioctl.duration = amdgpu_sched_run_job.ts - amdgpu_cs_ioctl.ts;

                    start_ts = amdgpu_cs_ioctl.ts;
                }

                // If our start time stamp is greater than the last fence time stamp then
                //  reset our graph row back to the top.
                if ( start_ts > last_fence_signaled_ts )
                    graph_row_id = 0;
                fence_signaled.graph_row_id = graph_row_id++;

                last_fence_signaled_ts = fence_signaled.ts;

                uint32_t hashval = hashstr32( fence_signaled.user_comm );

                // Mark this event as autogen'd color so it doesn't get overwritten
                fence_signaled.flags |= TRACE_FLAG_AUTOGEN_COLOR;
                fence_signaled.color = imgui_col_from_hashval( hashval, label_sat, label_alpha );
            }
        }
    }

    for ( uint32_t hashval : erase_list )
    {
        // Completely erase timeline rows with zero entries.
        m_amd_timeline_locs.m_locs.m_map.erase( hashval );
    }
}

const std::vector< uint32_t > *TraceEvents::get_locs( const char *name,
        loc_type_t *ptype, std::string *errstr )
{
    loc_type_t type = LOC_TYPE_Max;
    const std::vector< uint32_t > *plocs = NULL;

    if ( errstr )
        errstr->clear();

    if ( !strcmp( name, "cpu graph" ) )
    {
        type = LOC_TYPE_CpuGraph;

        // Let's try to find the first cpu with some events and give them that
        for ( uint32_t cpu = 0; cpu < m_trace_info.cpus; cpu++ )
        {
            plocs = m_sched_switch_cpu_locs.get_locations_u32( cpu );
            if ( plocs )
                break;
        }
    }
    else if ( get_ftrace_row_info( name ) )
    {
        type = LOC_TYPE_Print;
        plocs = &m_ftrace.print_locs;
    }
    else if ( !strncmp( name, "plot:", 5 ) )
    {
        GraphPlot *plot = get_plot_ptr( name );

        if ( plot )
        {
            type = LOC_TYPE_Plot;
            plocs = get_tdopexpr_locs( plot->m_filter_str.c_str() );
        }
    }
    else if ( !strncmp( name, "msm ring", 8 ) )
    {
        std::string timeline_name = name;
        size_t len = strlen( name );

        if ( !strcmp( name + len - 3, " hw" ) )
        {
            timeline_name.erase( len - 3 );
            type = LOC_TYPE_AMDTimeline_hw;
        }
        else
        {
            type = LOC_TYPE_AMDTimeline;
        }
        plocs = m_amd_timeline_locs.get_locations_str( timeline_name.c_str() );
    }
    else if ( !strncmp( name, "drm sched", 9 ) )
    {
        type = LOC_TYPE_AMDTimeline;
        plocs = m_amd_timeline_locs.get_locations_str( name );
    }
    else
    {
        size_t len = strlen( name );

        if ( ( len > 3 ) && !strcmp( name + len - 3, " hw" ) )
        {
            // Check for "gfx hw", "comp_1.1.1 hw", etc.
            uint32_t hashval = hashstr32( name, len - 3 );

            type = LOC_TYPE_AMDTimeline_hw;
            plocs = m_amd_timeline_locs.get_locations_u32( hashval );
        }

        if ( !plocs )
        {
            // Check for regular comm type rows
            type = LOC_TYPE_Comm;
            plocs = get_comm_locs( name );

            if ( !plocs )
            {
                // TDOP Expressions. Ie, $name = print, etc.
                type = LOC_TYPE_Tdopexpr;
                plocs = get_tdopexpr_locs( name );

                if ( !plocs )
                {
                    // Timelines: sdma0, gfx, comp_1.2.1, etc.
                    type = LOC_TYPE_AMDTimeline;
                    plocs = get_timeline_locs( name );
                }
            }
        }
    }

    if ( ptype )
        *ptype = plocs ? type : LOC_TYPE_Max;
    return plocs;
}

// Check if an expression is surrounded by parens: "( expr )"
// Assumes no leading / trailing whitespace in expr.
static bool is_surrounded_by_parens( const char *expr )
{
    if ( expr[ 0 ] == '(' )
    {
        int level = 1;

        for ( size_t i = 1; expr[ i ]; i++ )
        {
            if ( expr[ i ] == '(' )
            {
                level++;
            }
            else if ( expr[ i ] == ')' )
            {
                if ( --level == 0 )
                    return !expr[ i + 1 ];
            }
        }
    }

    return false;
}

template < size_t T >
static void add_event_filter( char ( &dest )[ T ], const char *fmt, ... ) ATTRIBUTE_PRINTF( 2, 3 );
template < size_t T >
static void add_event_filter( char ( &dest )[ T ], const char *fmt, ... )
{
    va_list args;
    char expr[ T ];

    va_start( args, fmt );
    vsnprintf_safe( expr, fmt, args );
    va_end( args );

    str_strip_whitespace( dest );

    if ( !dest[ 0 ] )
    {
        strcpy_safe( dest, expr );
    }
    else if ( !strstr_ignore_spaces( dest, expr ) )
    {
        char dest2[ T ];
        bool has_parens = is_surrounded_by_parens( dest );

        strcpy_safe( dest2, dest );
        snprintf_safe( dest, "%s%s%s && (%s)",
                       has_parens ? "" : "(", dest2, has_parens ? "" : ")",
                       expr );
    }
}

template < size_t T >
static void remove_event_filter( char ( &dest )[ T ], const char *fmt, ... ) ATTRIBUTE_PRINTF( 2, 3 );
template < size_t T >
static void remove_event_filter( char ( &dest )[ T ], const char *fmt, ... )
{
    va_list args;
    char expr[ T ];

    va_start( args, fmt );
    vsnprintf_safe( expr, fmt, args );
    va_end( args );

    // Remove '&& expr'
    remove_substrings( dest, "&& %s", expr );
    // Remove 'expr &&'
    remove_substrings( dest, "%s &&", expr );

    for ( int i = 6; i >= 1; i-- )
    {
        // Remove '&& (expr)' strings
        remove_substrings( dest, "&& %.*s%s%.*s", i, "((((((", expr, i, "))))))" );
        // Remove '(expr) &&'
        remove_substrings( dest, "%.*s%s%.*s &&", i, "((((((", expr, i, "))))))" );
    }

    // Remove 'expr'
    remove_substrings( dest, "%s", expr );

    // Remove empty parenthesis
    remove_substrings( dest, "%s", "()" );

    // Remove leading / trailing whitespace
    str_strip_whitespace( dest );
}

TraceWin::TraceWin( const char *filename, size_t filesize )
{
    // Note that m_trace_events is possibly being loaded in
    //  a background thread at this moment, so be sure to check
    //  m_eventsloaded before accessing it...

    m_title = string_format( "%s (%.2f MB)", filename, filesize / ( 1024.0f * 1024.0f ) );
    m_trace_events.m_filename = filename;
    m_trace_events.m_filesize = filesize;

    strcpy_safe( m_eventlist.timegoto_buf, "0.0" );

    // TODO:
    //strcpy_safe( m_filter.buf, s_ini().GetStr( "event_filter_buf", "" ) );
    m_filter.enabled = !!m_filter.buf[ 0 ];

    m_graph.saved_locs.resize( action_graph_save_location5 - action_graph_save_location1 + 1 );

    m_frame_markers.init();
    m_create_graph_row_dlg.init();
    m_create_row_filter_dlg.init();
}

TraceWin::~TraceWin()
{
    // TODO:
    //s_ini().PutStr( "event_filter_buf", m_filter.buf );

    m_graph.rows.shutdown();

    m_frame_markers.shutdown();
    m_create_graph_row_dlg.shutdown();
    m_create_row_filter_dlg.shutdown();

    //s_opts().set_crtc_max( -1 );
}

void TraceWin::render()
{
    GPUVIS_TRACE_BLOCK( __func__ );

    uint32_t count = 0;
    TraceEvents::tracestatus_t status = m_trace_events.get_load_status( &count );

    ImGui::Begin( m_title.c_str(), &m_open,
                  ImGuiWindowFlags_MenuBar |
                  ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoCollapse |
                  ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoBringToFrontOnFocus );

    s_app().render_menu( "menu_tracewin" );

    if ( status == TraceEvents::Trace_Loaded )
    {
        if ( count )
        {
            if ( !m_inited )
            {
                int64_t last_ts = m_trace_events.m_events.back().ts;

                // Initialize our graph rows first time through.
                m_graph.rows.init( m_trace_events );

                m_graph.length_ts = std::min< int64_t >( last_ts, 40 * NSECS_PER_MSEC );
                m_graph.start_ts = last_ts - m_graph.length_ts;
                m_graph.recalc_timebufs = true;

                m_eventlist.do_gotoevent = true;
                m_eventlist.goto_eventid = ts_to_eventid( m_graph.start_ts + m_graph.length_ts / 2 );
            }

            if ( !ShowEventList ||
                 imgui_collapsingheader( "Event Graph", &m_graph.has_focus, ImGuiTreeNodeFlags_DefaultOpen ) )
            {
                graph_render_options();
                graph_render();
            }

            if ( ShowEventList &&
                 imgui_collapsingheader( "Event List", &m_eventlist.has_focus, ImGuiTreeNodeFlags_DefaultOpen ) )
            {
                eventlist_render_options();
                eventlist_render();
                eventlist_handle_hotkeys();
            }

            // Render pinned tooltips
            m_ttip.tipwins.set_tooltip( "Pinned Tooltip", &m_ttip.visible, m_ttip.str.c_str() );

            // graph/eventlist didn't handle this action, so just toggle ttip visibility.
            if ( s_actions().get( action_graph_pin_tooltip ) )
                m_ttip.visible = !m_ttip.visible;

            // Render plot, graph rows, filter dialogs, etc
            graph_dialogs_render();

            m_inited = true;
        }
    }
    else if ( status == TraceEvents::Trace_Loading ||
              status == TraceEvents::Trace_Initializing )
    {
        bool loading = ( status == TraceEvents::Trace_Loading );

        ImGui::Text( "%s events %u...", loading ? "Loading" : "Initializing", count );

        if ( ImGui::Button( "Cancel" ) ||
             ( ImGui::IsWindowFocused() && s_actions().get( action_escape ) ) )
        {
            s_app().cancel_load_file();
        }
    }
    else
    {
        ImGui::Text( "Error loading file %s...\n", m_trace_events.m_filename.c_str() );
    }

    ImGui::End();
}

void TraceWin::trace_render_info()
{
    size_t event_count = m_trace_events.m_events.size();

    ImGui::Text( "Total Events: %lu\n", event_count );
    if ( !event_count )
        return;

    const trace_info_t &trace_info = m_trace_events.m_trace_info;

    ImGui::Text( "Trace time: %s",
                 ts_to_timestr( m_trace_events.m_events.back().ts, 4 ).c_str() );
    ImGui::Text( "Trace time start: %s",
                 ts_to_timestr( m_trace_events.m_trace_info.trimmed_ts, 4 ).c_str() );

    ImGui::Text( "Trace cpus: %u", trace_info.cpus );

    if ( !trace_info.uname.empty() )
        ImGui::Text( "Trace uname: %s", trace_info.uname.c_str() );
    if ( !trace_info.opt_version.empty() )
        ImGui::Text( "Trace version: %s", trace_info.opt_version.c_str() );

    if ( !m_graph.rows.m_graph_rows_list.empty() &&
         ImGui::CollapsingHeader( "Graph Row Info" ) )
    {
        int tree_tgid = -1;
        bool display_event = true;

        if ( imgui_begin_columns( "row_info", { "Row Name", "Events", "sched_switch %" } ) )
            ImGui::SetColumnWidth( 0, imgui_scale( 250.0f ) );

        for ( const GraphRows::graph_rows_info_t &info : m_graph.rows.m_graph_rows_list )
        {
            const tgid_info_t *tgid_info = NULL;
            const char *row_name = info.row_name.c_str();
            ftrace_row_info_t *ftrace_row_info = m_trace_events.get_ftrace_row_info( row_name );

            if ( info.type == LOC_TYPE_Comm )
                tgid_info = m_trace_events.tgid_from_commstr( info.row_name.c_str() );

            if ( ( tree_tgid >= 0 ) &&
                 ( !tgid_info || ( tgid_info->tgid != tree_tgid ) ) )
            {
                // Close the tree node
                if ( display_event )
                    ImGui::TreePop();

                tree_tgid = -1;
                display_event = true;
            }

            // If we have tgid_info and it isn't a current tree, create new treenode
            if ( tgid_info && ( tgid_info->tgid != tree_tgid ) )
            {
                size_t count = tgid_info->pids.size();

                // Store current tree tgid
                tree_tgid = tgid_info->tgid;

                display_event = ImGui::TreeNode( &info, "%s (%lu thread%s)",
                                                 tgid_info->commstr_clr, count,
                                                 ( count > 1 ) ? "s" : "" );
                ImGui::NextColumn();
                ImGui::NextColumn();
                ImGui::NextColumn();
            }

            if ( display_event )
            {
                // Indent if we're in a tgid tree node
                if ( tree_tgid >= 0 )
                    ImGui::Indent();

                ImGui::Text( "%s", row_name );

                ImGui::NextColumn();
                ImGui::Text( "%lu", ftrace_row_info ? ftrace_row_info->count : info.event_count );

                if ( info.type == LOC_TYPE_Plot )
                {
                    GraphPlot *plot = m_trace_events.get_plot_ptr( info.row_name.c_str() );

                    if ( plot )
                    {
                        ImGui::SameLine();
                        ImGui::Text( "(minval:%.2f maxval:%.2f)", plot->m_minval, plot->m_maxval );
                    }
                }
                ImGui::NextColumn();

                // Check if this is a comm entry, and we have sched_switch total time
                if ( ( info.type == LOC_TYPE_Comm ) && m_trace_events.m_sched_switch_time_total )
                {
                    const char *pidstr = strrchr( row_name, '-' );

                    if ( pidstr)
                    {
                        // See if we have a total sched_switch count for this pid
                        int64_t *val = m_trace_events.m_sched_switch_time_pid.get_val( atoi( pidstr + 1 ) );

                        if ( val )
                            ImGui::Text( "%.2f%%", *val * 100.0 / m_trace_events.m_sched_switch_time_total );
                    }
                }
                ImGui::NextColumn();

                if ( tree_tgid >= 0 )
                    ImGui::Unindent();
            }
        }

        if ( ( tree_tgid >= 0 ) && display_event )
            ImGui::TreePop();

        ImGui::EndColumns();
    }

    if ( ImGui::CollapsingHeader( "Event info" ) )
    {
        if ( imgui_begin_columns( "event_info", { "Event Name", "Count", "Pct" } ) )
        {
            ImGui::SetColumnWidth( 0, imgui_scale( 200.0f ) );
            ImGui::SetColumnWidth( 1, imgui_scale( 75.0f ) );
        }

        for ( auto item : m_trace_events.m_eventnames_locs.m_locs.m_map )
        {
            const char *eventname = m_trace_events.m_strpool.findstr( item.first );
            const std::vector< uint32_t > &locs = item.second;

            ImGui::Text( "%s", eventname );
            ImGui::NextColumn();
            ImGui::Text( "%lu", locs.size() );
            ImGui::NextColumn();
            ImGui::Text( "%.2f%%", 100.0f * locs.size() / event_count );
            ImGui::NextColumn();
        }

        ImGui::EndColumns();
    }

    if ( !trace_info.cpu_info.empty() &&
         ImGui::CollapsingHeader( "CPU Info" ) )
    {
        if ( imgui_begin_columns( "cpu_stats", { "CPU", "Stats", "Events", "Min ts", "Max ts", "File Size" } ) )
            ImGui::SetColumnWidth( 0, imgui_scale( 75.0f ) );

        for ( uint32_t cpu = 0; cpu < trace_info.cpu_info.size(); cpu++ )
        {
            const cpu_info_t &cpu_info = trace_info.cpu_info[ cpu ];

            // CPU: 0, CPU: 1, etc.
            ImGui::Text( "CPU: %u", cpu );
            ImGui::NextColumn();

            // Stats
            ImGui::BeginGroup();
            if ( cpu_info.entries )
                ImGui::Text( "Entries: %" PRIu64, cpu_info.entries );
            if ( cpu_info.overrun )
                ImGui::Text( "Overrun: %" PRIu64, cpu_info.overrun );
            if ( cpu_info.commit_overrun )
                ImGui::Text( "Commit overrun: %" PRIu64, cpu_info.commit_overrun );
            ImGui::Text( "Bytes: %" PRIu64, cpu_info.bytes );
            ImGui::Text( "Oldest event ts: %s", ts_to_timestr( cpu_info.oldest_event_ts, 6 ).c_str() );
            ImGui::Text( "Now ts: %s", ts_to_timestr( cpu_info.now_ts, 6 ).c_str() );
            if ( cpu_info.dropped_events )
                ImGui::Text( "Dropped events: %" PRIu64, cpu_info.dropped_events );
            ImGui::Text( "Read events: %" PRIu64, cpu_info.read_events );
            //$ ImGui::Text( "file offset: %" PRIu64 "\n", cpu_info.file_offset );
            ImGui::EndGroup();

            if ( ImGui::IsItemHovered() )
            {
                const char *text[] =
                {
                    "Ring buffer stats:",
                    "  Entries: The number of events that are still in the buffer.",
                    "  Overrun: The number of lost events due to overwriting when the buffer was full.",
                    "  Commit overrun: Should always be zero.",
                    "    This gets set if so many events happened within a nested event (ring buffer is re-entrant),",
                    "    that it fills the buffer and starts dropping events.",
                    "  Bytes: Bytes actually read (not overwritten).",
                    "  Oldest event ts: The oldest timestamp in the buffer.",
                    "  Now ts: The current timestamp.",
                    "  Dropped events: Events lost due to overwrite option being off.",
                    "  Read events: The number of events read."
                };
                const char *clr_bright = s_textclrs().str( TClr_Bright );
                const char *clr_def = s_textclrs().str( TClr_Def );

                ImGui::BeginTooltip();
                for ( size_t i = 0; i < ARRAY_SIZE( text ); i++ )
                {
                    const char *str = text[ i ];
                    const char *colon = strchr( str, ':' );

                    if ( colon )
                        ImGui::Text( "%s%.*s%s%s", clr_bright, ( int )( colon - str ), str, clr_def, colon );
                    else
                        ImGui::Text( "%s", str );
                }
                ImGui::EndTooltip();
            }
            ImGui::NextColumn();

            // Events
            ImGui::Text( "%" PRIu64 " / %" PRIu64, cpu_info.events, cpu_info.tot_events );
            ImGui::NextColumn();

            // Min ts
            if ( cpu_info.min_ts != INT64_MAX )
                ImGui::Text( "%s", ts_to_timestr( cpu_info.min_ts, 6 ).c_str() );
            ImGui::NextColumn();

            // Max ts
            if ( cpu_info.max_ts != INT64_MAX )
                ImGui::Text( "%s", ts_to_timestr( cpu_info.max_ts, 6 ).c_str() );
            ImGui::NextColumn();

            // File Size
            if  ( cpu_info.tot_events )
                ImGui::Text( "%" PRIu64 "\n%.2f b/event\n", cpu_info.file_size, ( float )cpu_info.file_size / cpu_info.tot_events );
            else
                ImGui::Text( "%" PRIu64 "\n", cpu_info.file_size );
            ImGui::NextColumn();

            ImGui::Separator();
        }

        ImGui::EndColumns();
    }
}

void TraceWin::graph_center_event( uint32_t eventid )
{
    trace_event_t &event = get_event( eventid );

    m_eventlist.selected_eventid = event.id;
    m_graph.start_ts = event.ts - m_graph.length_ts / 2;
    m_graph.recalc_timebufs = true;
    m_graph.show_row_name = event.comm;
}

bool TraceWin::eventlist_render_popupmenu( uint32_t eventid )
{
    if ( !ImGui::BeginPopup( "EventsListPopup" ) )
        return false;

    imgui_text_bg( ImGui::GetStyleColorVec4( ImGuiCol_Header ), "%s", "Options" );
    ImGui::Separator();

    trace_event_t &event = get_event( eventid );

    std::string label = string_format( "Center event %u on graph", event.id );
    if ( ImGui::MenuItem( label.c_str() ) )
        graph_center_event( eventid );

    // Set / Goto / Clear Markers
    {
        int idx = graph_marker_menuitem( "Set Marker", false, action_graph_set_markerA );
        if ( idx >= 0 )
            graph_marker_set( idx, event.ts );

        idx = graph_marker_menuitem( "Goto Marker", true, action_graph_goto_markerA );
        if ( idx >= 0 )
        {
            m_graph.start_ts = m_graph.ts_markers[ idx ] - m_graph.length_ts / 2;
            m_graph.recalc_timebufs = true;
        }

        idx = graph_marker_menuitem( "Clear Marker", true, action_nil );
        if ( idx >= 0 )
            graph_marker_set( idx, INT64_MAX );
    }

    ImGui::Separator();

    label = string_format( "Add '$name == %s' filter", event.name );
    if ( ImGui::MenuItem( label.c_str() ) )
    {
        remove_event_filter( m_filter.buf, "$name != \"%s\"", event.name );
        add_event_filter( m_filter.buf, "$name == \"%s\"", event.name );
        m_filter.enabled = true;
    }
    label = string_format( "Add '$name != %s' filter", event.name );
    if ( ImGui::MenuItem( label.c_str() ) )
    {
        remove_event_filter( m_filter.buf, "$name == \"%s\"", event.name );
        add_event_filter( m_filter.buf, "$name != \"%s\"", event.name );
        m_filter.enabled = true;
    }

    label = string_format( "Add '$pid == %d' filter", event.pid );
    if ( ImGui::MenuItem( label.c_str() ) )
    {
        remove_event_filter( m_filter.buf, "$pid != %d", event.pid );
        add_event_filter( m_filter.buf, "$pid == %d", event.pid );
        m_filter.enabled = true;
    }
    label = string_format( "Add '$pid != %d' filter", event.pid );
    if ( ImGui::MenuItem( label.c_str() ) )
    {
        remove_event_filter( m_filter.buf, "$pid == %d", event.pid );
        add_event_filter( m_filter.buf, "$pid != %d", event.pid );
        m_filter.enabled = true;
    }

    const tgid_info_t *tgid_info = m_trace_events.tgid_from_pid( event.pid );
    if ( tgid_info )
    {
        ImGui::Separator();

        label = string_format( "Filter process '%s' events", tgid_info->commstr_clr );
        if ( ImGui::MenuItem( label.c_str() ) )
        {
            remove_event_filter( m_filter.buf, "$tgid != %d", tgid_info->tgid );
            add_event_filter( m_filter.buf, "$tgid == %d", tgid_info->tgid );
            m_filter.enabled = true;
        }
        label = string_format( "Hide process '%s' events", tgid_info->commstr_clr );
        if ( ImGui::MenuItem( label.c_str() ) )
        {
            remove_event_filter( m_filter.buf, "$tgid == %d", tgid_info->tgid );
            add_event_filter( m_filter.buf, "$tgid != %d", tgid_info->tgid );
            m_filter.enabled = true;
        }
    }

    if ( !m_filter.events.empty() )
    {
        ImGui::Separator();

        if ( ImGui::MenuItem( "Clear Filter" ) )
        {
            m_filter.buf[ 0 ] = 0;
            m_filter.enabled = true;
        }
    }

    const std::string plot_str = CreatePlotDlg::get_plot_str( event );
    if ( !plot_str.empty() )
    {
        std::string plot_label = std::string( "Create Plot for " ) + plot_str;

        ImGui::Separator();
        if ( ImGui::MenuItem( plot_label.c_str() ) )
            m_create_plot_eventid = event.id;
    }

    ImGui::Separator();

    if ( ImGui::MenuItem( "Set Frame Markers..." ) )
        m_create_filter_eventid = event.id;

    if ( ImGui::MenuItem( "Edit Frame Markers..." ) )
        m_create_filter_eventid = m_trace_events.m_events.size();

    if ( m_frame_markers.m_left_frames.size() &&
         ImGui::MenuItem( "Clear Frame Markers" ) )
    {
        m_frame_markers.m_left_frames.clear();
        m_frame_markers.m_right_frames.clear();
    }

    if ( s_actions().get( action_escape ) )
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
    return true;
}

static std::string get_event_fields_str( const trace_event_t &event, const char *eqstr, char sep )
{
    std::string fieldstr;

    if ( event.user_comm != event.comm )
        fieldstr += string_format( "%s%s%s%c", "user_comm", eqstr, event.user_comm, sep );

    for ( uint32_t i = 0; i < event.numfields; i++ )
    {
        std::string buf;
        const char *key = event.fields[ i ].key;
        const char *value = event.fields[ i ].value;

        if ( event.is_ftrace_print() && !strcmp( key, "buf" ) )
        {
            buf = s_textclrs().mstr( value, event.color );
            value = buf.c_str();
        }

        fieldstr += string_format( "%s%s%s%c", key, eqstr, value, sep );
    }

    fieldstr += string_format( "%s%s%s", "system", eqstr, event.system );

    return fieldstr;
}

static float get_keyboard_scroll_lines( float visible_rows )
{
    float scroll_lines = 0.0f;

    if ( ImGui::IsWindowFocused() && s_actions().count() )
    {
        if ( s_actions().get( action_scroll_pagedown ) )
            scroll_lines = std::max< float>( visible_rows - 5, 1 );
        else if ( s_actions().get( action_scroll_pageup ) )
            scroll_lines = std::min< float >( -( visible_rows - 5), -1 );
        else if ( s_actions().get( action_scroll_down ) )
            scroll_lines = 1;
        else if ( s_actions().get( action_scroll_up ) )
            scroll_lines = -1;
        else if ( s_actions().get( action_scroll_home ) )
            scroll_lines = -ImGui::GetScrollMaxY();
        else if ( s_actions().get( action_scroll_end ) )
            scroll_lines = ImGui::GetScrollMaxY();
    }

    return scroll_lines;
}

bool TraceWin::eventlist_handle_mouse( const trace_event_t &event, uint32_t i )
{
    bool popup_shown = false;

    // Check if item is hovered and we don't have a popup menu going already.
    if ( !is_valid_id( m_eventlist.popup_eventid ) &&
         ImGui::IsItemHovered() )
    {
        // Store the hovered event id.
        m_eventlist.hovered_eventid = event.id;
        m_graph.last_hovered_eventid = event.id;

        if ( ImGui::IsMouseClicked( 1 ) )
        {
            // If they right clicked, show the context menu.
            m_eventlist.popup_eventid = i;

            // Open the popup for eventlist_render_popupmenu().
            ImGui::OpenPopup( "EventsListPopup" );
        }
        else
        {
            // Otherwise show a tooltip.
            std::string ttip = s_textclrs().str( TClr_Def );
            std::string ts_str = ts_to_timestr( event.ts, 6 );
            const char *commstr = m_trace_events.tgidcomm_from_pid( event.pid );

            if ( graph_marker_valid( 0 ) || graph_marker_valid( 1 ) )
            {
                if ( graph_marker_valid( 0 ) )
                    ttip += "Marker A: " + ts_to_timestr( m_graph.ts_markers[ 0 ] - event.ts, 2, " ms\n" );
                if ( graph_marker_valid( 1 ) )
                    ttip += "Marker B: " + ts_to_timestr( m_graph.ts_markers[ 1 ] - event.ts, 2, " ms\n" );
                ttip += "\n";
            }

            ttip += string_format( "Id: %u\nTime: %s\nComm: %s\nCpu: %u\nEvent: %s\n",
                                   event.id,
                                   ts_str.c_str(),
                                   commstr,
                                   event.cpu,
                                   event.name );

            if ( event.has_duration() )
                ttip += "Duration: " + ts_to_timestr( event.duration, 4, " ms\n" );

            ttip += "\n";
            ttip += get_event_fields_str( event, ": ", '\n' );

            ImGui::SetTooltip( "%s", ttip.c_str() );

            if ( s_actions().get( action_graph_pin_tooltip ) )
            {
                m_ttip.str = ttip;
                m_ttip.visible = true;
            }
        }
    }

    // If we've got an active popup menu, render it.
    if ( m_eventlist.popup_eventid == i )
    {
        ImGui::PushStyleColor( ImGuiCol_Text, s_clrs().getv4( col_ImGui_Text ) );

        uint32_t eventid = !m_filter.events.empty() ?
                    m_filter.events[ m_eventlist.popup_eventid ] :
                    m_eventlist.popup_eventid;

        if ( !TraceWin::eventlist_render_popupmenu( eventid ) )
            m_eventlist.popup_eventid = INVALID_ID;

        popup_shown = true;

        ImGui::PopStyleColor();
    }

    return popup_shown;
}

static void draw_ts_line( const ImVec2 &pos, ImU32 color )
{
    ImGui::PopClipRect();

    float max_x = ImGui::GetWindowDrawList()->GetClipRectMax().x;
    float spacing_U = ( float )( int )( ImGui::GetStyle().ItemSpacing.y * 0.5f );
    float pos_y = pos.y - spacing_U;

    ImGui::GetWindowDrawList()->AddLine(
                ImVec2( pos.x, pos_y ), ImVec2( max_x, pos_y ),
                color, imgui_scale( 2.0f ) );

    int columIdx = 0;
    ImGui::PushColumnClipRect( columIdx );
}

static bool imgui_input_uint32( uint32_t *pval, float w, const char *label, const char *label2, ImGuiInputTextFlags flags = 0 )
{
    int val = *pval;
    bool ret = ImGui::Button( label );

    ImGui::SameLine();
    ImGui::PushItemWidth( imgui_scale( w ) );
    ret |= ImGui::InputInt( label2, &val, 0, 0, flags );
    ImGui::PopItemWidth();

    if ( ret )
        *pval = val;

    return ret;
}

void TraceWin::eventlist_render_options()
{
    // Goto event
    m_eventlist.do_gotoevent |= imgui_input_uint32( &m_eventlist.goto_eventid, 75.0f, "Goto Event:", "##GotoEvent" );
    if ( ImGui::IsItemActive() )
        m_eventlist.ts_marker_mouse_sync = m_graph.ts_marker_mouse;

    ImGui::SameLine();
    if ( imgui_input_text2( "Goto Time:", m_eventlist.timegoto_buf, 120.0f, ImGuiInputText2FlagsLeft_LabelIsButton ) )
    {
        m_eventlist.do_gotoevent = true;
        m_eventlist.goto_eventid = timestr_to_eventid( m_eventlist.timegoto_buf );
    }

    if ( !m_inited ||
         m_eventlist.hide_sched_switch_events_val != HideSchedSwitchEvents )
    {
        bool hide_sched_switch = HideSchedSwitchEvents;
        static const char filter_str[] = "$name != \"sched_switch\"";

        remove_event_filter( m_filter.buf, "$name == \"sched_switch\"" );
        remove_event_filter( m_filter.buf, filter_str );

        if ( hide_sched_switch )
            add_event_filter( m_filter.buf, filter_str );

        m_filter.enabled = true;
        m_eventlist.hide_sched_switch_events_val = hide_sched_switch;
    }

    if ( m_filter.enabled ||
         imgui_input_text2( "Event Filter:", m_filter.buf, 500.0f,
                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputText2FlagsLeft_LabelIsButton ) )
    {
        m_filter.events.clear();
        m_filter.pid_eventcount.m_map.clear();
        m_filter.errstr.clear();
        m_filter.enabled = false;

        if ( m_filter.buf[ 0 ] )
        {
            tdop_get_key_func get_key_func = std::bind( filter_get_key_func, &m_trace_events.m_strpool, _1, _2 );
            class TdopExpr *tdop_expr = tdopexpr_compile( m_filter.buf, get_key_func, m_filter.errstr );

            util_time_t t0 = util_get_time();

            if ( tdop_expr )
            {
                for ( trace_event_t &event : m_trace_events.m_events )
                {
                    tdop_get_keyval_func get_keyval_func = std::bind( filter_get_keyval_func,
                                                                      &m_trace_events.m_trace_info, &event, _1, _2 );

                    const char *ret = tdopexpr_exec( tdop_expr, get_keyval_func );

                    event.is_filtered_out = !ret[ 0 ];
                    if ( !event.is_filtered_out )
                    {
                        m_filter.events.push_back( event.id );

                        // Bump up count of !filtered events for this pid
                        uint32_t *count = m_filter.pid_eventcount.get_val( event.pid, 0 );
                        (*count)++;
                    }
                }

                if ( m_filter.events.empty() )
                    m_filter.errstr = "WARNING: No events found.";

                tdopexpr_delete( tdop_expr );
                tdop_expr = NULL;
            }

            float time = util_time_to_ms( t0, util_get_time() );
            if ( time > 1000.0f )
                logf( "tdopexpr_compile(\"%s\"): %.2fms\n", m_filter.buf, time );
        }
    }

    if ( ImGui::IsItemHovered() )
    {
        std::string ttip;

        ttip += s_textclrs().bright_str( "Event Filter\n\n" );
        ttip += "Vars: Any field in Info column plus:\n";
        ttip += "    $name, $comm, $user_comm, $id, $pid, $tgid, $ts, $cpu, $duration\n";
        ttip += "Operators: &&, ||, !=, =, >, >=, <, <=, =~\n\n";

        ttip += "Examples:\n";
        ttip += "  $pid = 4615\n";
        ttip += "  $ts >= 11.1 && $ts < 12.5\n";
        ttip += "  $ring_name = 0xffff971e9aa6bdd0\n";
        ttip += "  $buf =~ \"[Compositor] Warp\"\n";
        ttip += "  ( $timeline = gfx ) && ( $id < 10 || $id > 100 )";

        ImGui::SetTooltip( "%s", ttip.c_str() );

        if ( s_actions().get( action_graph_pin_tooltip ) )
        {
            m_ttip.str = ttip;
            m_ttip.visible = true;
        }
    }

    ImGui::SameLine();
    if ( ImGui::Button( "Clear Filter" ) )
    {
        m_filter.events.clear();
        m_filter.pid_eventcount.m_map.clear();
        m_filter.errstr.clear();
        m_filter.buf[ 0 ] = 0;
    }

    if ( !m_filter.errstr.empty() )
    {
        ImGui::SameLine();
        ImGui::TextColored( ImVec4( 1, 0, 0, 1 ), "%s", m_filter.errstr.c_str() );
    }
    else if ( !m_filter.events.empty() )
    {
        std::string label = string_format( "Graph only filtered (%lu events)", m_filter.events.size() );

        ImGui::SameLine();
        //s_opts().render_imgui_opt( OPT_GraphOnlyFiltered );

        if ( GraphOnlyFiltered )
        {
            ImGui::SameLine();
            //s_opts().render_imgui_opt( OPT_Graph_HideEmptyFilteredRows );
        }
    }
}

void TraceWin::eventlist_render()
{
    GPUVIS_TRACE_BLOCK( __func__ );

    size_t event_count;
    std::vector< uint32_t > *filtered_events = NULL;

    if ( ImGui::GetIO().KeyShift && !m_eventlist.highlight_ids.empty() )
    {
        // If shift is down and we have a tooltip with highlighted events,
        // show only those
        filtered_events = &m_eventlist.highlight_ids;
        event_count = filtered_events->size();
    }
    else if ( !m_filter.events.empty() )
    {
        // Otherwise display filtered events
        filtered_events = &m_filter.events;
        event_count = filtered_events->size();
    }
    else
    {
        // Display all events
        event_count = m_trace_events.m_events.size();
    }

    // Set focus on event list first time we open.
    if ( s_actions().get( action_focus_eventlist ) ||
         ( !m_inited && ImGui::IsWindowFocused() ) )
    {
        ImGui::SetNextWindowFocus();
    }

    // Events list
    {
        float lineh = ImGui::GetTextLineHeightWithSpacing();
        const ImVec2 content_avail = ImGui::GetContentRegionAvail();

        int eventlist_row_count = EventListRowCount;

        // If the user has set the event list row count to 0 (auto size), make
        //  sure we always have at least 20 rows.
        if ( !eventlist_row_count && ( content_avail.y / lineh < 20 ) )
            eventlist_row_count = 20;

        // Set the child window size to hold count of items + header + separator
        float sizey = eventlist_row_count * lineh;

        ImGui::SetNextWindowContentSize( { 0.0f, ( event_count + 1 ) * lineh + 1 } );
        ImGui::BeginChild( "eventlistbox", ImVec2( 0.0f, sizey ) );

        m_eventlist.has_focus = ImGui::IsWindowFocused();

        float winh = ImGui::GetWindowHeight();
        uint32_t visible_rows = ( winh + 1 ) / lineh;

        float scroll_lines = get_keyboard_scroll_lines( visible_rows );
        if ( scroll_lines )
            ImGui::SetScrollY( ImGui::GetScrollY() + scroll_lines * lineh );

        if ( SyncEventListToGraph &&
             !m_eventlist.do_gotoevent &&
             ( m_graph.ts_marker_mouse != -1 ) &&
             ( m_graph.ts_marker_mouse != m_eventlist.ts_marker_mouse_sync ) )
        {
            m_eventlist.do_gotoevent = true;
            m_eventlist.goto_eventid = ts_to_eventid( m_graph.ts_marker_mouse );
        }

        if ( m_eventlist.do_gotoevent )
        {
            uint32_t pos;

            if ( filtered_events )
            {
                auto i = std::lower_bound( filtered_events->begin(), filtered_events->end(),
                                           m_eventlist.goto_eventid );

                pos = i - filtered_events->begin();
            }
            else
            {
                pos = m_eventlist.goto_eventid;
            }
            pos = std::min< uint32_t >( pos, event_count - 1 );

            ImGui::SetScrollY( ( ( float )pos - visible_rows / 2 + 1 ) * lineh );

            // Select the event also
            m_eventlist.selected_eventid = std::min< uint32_t >( m_eventlist.goto_eventid,
                                                                 event_count - 1 );

            m_eventlist.do_gotoevent = false;
            m_eventlist.ts_marker_mouse_sync = m_graph.ts_marker_mouse;
        }

        float scrolly = ImGui::GetScrollY();
        uint32_t start_idx = Clamp< uint32_t >( scrolly / lineh, 1, event_count ) - 1;
        uint32_t end_idx = std::min< uint32_t >( start_idx + 2 + visible_rows, event_count );

        // Draw columns
        imgui_begin_columns( "event_list", { "Id", "Time Stamp", "Comm", "Cpu", "Event", "Duration", "Info" },
                             &m_eventlist.columns_resized );
        {
            bool popup_shown = false;

            // Reset our hovered event id
            m_eventlist.hovered_eventid = INVALID_ID;

            // Move cursor position down to where we've scrolled.
            if ( start_idx > 0 )
                ImGui::SetCursorPosY( ImGui::GetCursorPosY() + lineh * ( start_idx - 1 ) );

            if ( filtered_events )
            {
                m_eventlist.start_eventid = filtered_events->at( start_idx );
                m_eventlist.end_eventid = filtered_events->at( end_idx - 1 );
            }
            else
            {
                m_eventlist.start_eventid = start_idx;
                m_eventlist.end_eventid = end_idx;
            }

            int64_t prev_ts = INT64_MIN;

            // Loop through and draw events
            for ( uint32_t i = start_idx; i < end_idx; i++ )
            {
                std::string markerbuf;
                trace_event_t &event = filtered_events ?
                        m_trace_events.m_events[ filtered_events->at( i ) ] :
                        m_trace_events.m_events[ i ];
                bool selected = ( m_eventlist.selected_eventid == event.id );
                ImVec2 cursorpos = ImGui::GetCursorScreenPos();
                ImVec4 color = s_clrs().getv4( col_EventList_Text );

                ImGui::PushID( i );

                if ( event.ts == m_graph.ts_markers[ 1 ] )
                {
                    color = s_clrs().getv4( col_Graph_MarkerB );
                    markerbuf = s_textclrs().mstr( "(B)", ( ImColor )color );
                }
                if ( event.ts == m_graph.ts_markers[ 0 ] )
                {
                    color = s_clrs().getv4( col_Graph_MarkerA );
                    markerbuf = s_textclrs().mstr( "(A)", ( ImColor )color ) + markerbuf;
                }
                if ( event.is_vblank() )
                {
                    uint32_t idx = Clamp< uint32_t >( col_VBlank0 + event.crtc, col_VBlank0, col_VBlank2 );

                    color = s_clrs().getv4( idx );
                }

                ImGui::PushStyleColor( ImGuiCol_Text, color );

                if ( selected )
                {
                    ImGui::PushStyleColor( ImGuiCol_Header, s_clrs().getv4( col_EventList_Sel ) );
                }
                else
                {
                    // If this event is in the highlighted list, give it a bit of a colored background
                    selected = std::binary_search(
                                m_eventlist.highlight_ids.begin(), m_eventlist.highlight_ids.end(), event.id );

                    if ( selected )
                        ImGui::PushStyleColor( ImGuiCol_Header, s_clrs().getv4( col_EventList_Hov ) );
                }

                // column 0: event id
                {
                    std::string label = std::to_string( event.id ) + markerbuf;
                    ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick;

                    if ( ImGui::Selectable( label.c_str(), selected, flags ) )
                    {
                        if ( ImGui::IsMouseDoubleClicked( 0 ) )
                            graph_center_event( event.id );

                        m_eventlist.selected_eventid = event.id;
                    }

                    // Columns bug workaround: selectable with SpanAllColumns & overlaid button does not work.
                    // https://github.com/ocornut/imgui/issues/684
                    ImGui::SetItemAllowOverlap();

                    // Handle popup menu / tooltip
                    popup_shown |= eventlist_handle_mouse( event, i );

                    ImGui::NextColumn();
                }

                // column 1: time stamp
                {
                    std::string ts_str = ts_to_timestr( event.ts, 6 );

                    // Show time delta from previous event
                    if ( prev_ts != INT64_MIN )
                        ts_str += " (+" + ts_to_timestr( event.ts - prev_ts, 4, "" ) + ")";

                    ImGui::Text( "%s", ts_str.c_str() );
                    ImGui::NextColumn();
                }

                // column 2: comm
                {
                    const tgid_info_t *tgid_info = m_trace_events.tgid_from_pid( event.pid );

                    if ( tgid_info )
                        ImGui::Text( "%s (%s)", event.comm, tgid_info->commstr_clr );
                    else
                        ImGui::Text( "%s", event.comm );
                    ImGui::NextColumn();
                }

                // column 3: cpu
                {
                    ImGui::Text( "%u", event.cpu );
                    ImGui::NextColumn();
                }

                // column 4: event name
                {
                    ImGui::Text( "%s", event.name );
                    ImGui::NextColumn();
                }

                // column 5: duration
                {
                    if ( event.has_duration() )
                        ImGui::Text( "%s", ts_to_timestr( event.duration, 4 ).c_str() );
                    ImGui::NextColumn();
                }

                // column 6: event fields
                {
                    if ( event.is_ftrace_print() )
                    {
                        const char *buf = get_event_field_val( event, "buf" );
                        std::string seqno = m_trace_events.get_ftrace_ctx_str( event );

                        ImGui::TextColored( ImColor( event.color ), "%s%s", buf, seqno.c_str() );
                    }
                    else
                    {
                        std::string fieldstr = get_event_fields_str( event, "=", ' ' );

                        ImGui::Text( "%s", fieldstr.c_str() );
                    }
                    ImGui::NextColumn();
                }

                if ( ( prev_ts < m_graph.ts_marker_mouse ) &&
                     ( event.ts > m_graph.ts_marker_mouse ) )
                {
                    // Draw time stamp marker diff line if we're right below ts_marker_mouse
                    draw_ts_line( cursorpos, s_clrs().get( col_Graph_MousePos ) );
                }
                else
                {
                    for ( size_t idx = 0; idx < ARRAY_SIZE( m_graph.ts_markers ); idx++ )
                    {
                        if ( ( prev_ts < m_graph.ts_markers[ idx ] ) &&
                             ( event.ts > m_graph.ts_markers[ idx ] ) )
                        {
                            draw_ts_line( cursorpos, s_clrs().get( col_Graph_MarkerA + idx ) );
                            break;
                        }
                    }
                }

                ImGui::PopStyleColor( 1 + selected );
                ImGui::PopID();

                prev_ts = event.ts;
            }

            if ( !popup_shown )
            {
                // When we modify a filter via the context menu, it can hide the item
                //  we right clicked on which means eventlist_render_popupmenu() won't get
                //  called. Check for that case here.
                m_eventlist.popup_eventid = INVALID_ID;
            }
        }
        if ( imgui_end_columns() )
            m_eventlist.columns_resized = true;

        ImGui::EndChild();
    }

    // If we are displaying highlighted events only, reset the mouse marker so the
    // next render frame we'll recalc our event list location
    if ( filtered_events == &m_eventlist.highlight_ids )
        m_eventlist.ts_marker_mouse_sync = -1;
}

void TraceWin::eventlist_handle_hotkeys()
{
    if ( m_eventlist.has_focus )
    {
        if ( is_valid_id( m_eventlist.hovered_eventid ) )
        {
            int marker = -1;

            if ( s_actions().get( action_graph_set_markerA ) )
                marker = 0;
            else if ( s_actions().get( action_graph_set_markerB ) )
                marker = 1;

            if ( marker != -1 )
            {
                const trace_event_t &event = get_event( m_eventlist.hovered_eventid );
                graph_marker_set( marker, event.ts );
            }
        }
    }
}

void TraceWin::graph_dialogs_render()
{
    // Plots
    if ( is_valid_id( m_create_plot_eventid ) )
    {
        m_create_plot_dlg.init( m_trace_events, m_create_plot_eventid );
        m_create_plot_eventid = INVALID_ID;
    }
    if ( m_create_plot_dlg.render_dlg( m_trace_events ) )
        m_graph.rows.add_row( m_create_plot_dlg.m_plot_name, m_create_plot_dlg.m_plot_name );

    // Graph rows
    if ( is_valid_id( m_create_graph_row_eventid ) )
    {
        m_create_graph_row_dlg.show_dlg( m_trace_events, m_create_graph_row_eventid );
        m_create_graph_row_eventid = INVALID_ID;
    }
    if ( m_create_graph_row_dlg.render_dlg( m_trace_events ) )
    {
        m_graph.rows.add_row( m_create_graph_row_dlg.m_name_buf,
                              m_create_graph_row_dlg.m_filter_buf );
    }

    // Row filters
    if ( m_create_row_filter_dlg_show )
    {
        m_create_row_filter_dlg.show_dlg( m_trace_events );
        m_create_row_filter_dlg_show = false;
    }
    if ( m_create_row_filter_dlg.render_dlg( m_trace_events ) )
    {
        // If the context menu was over a graph row
        if ( !m_create_row_filter_dlg_rowname.empty() )
        {
            RowFilters rowfilters( m_graph_row_filters, m_create_row_filter_dlg_rowname );
            const std::string &filter = m_create_row_filter_dlg.m_previous_filters[ 0 ];
            size_t idx = rowfilters.find_filter( filter );

            if ( idx == ( size_t )-1 )
            {
                // If this filter isn't already set for this graph row, add it
                rowfilters.toggle_filter( m_trace_events, idx, filter );
            }
        }
    }

    // Filter events
    if ( is_valid_id( m_create_filter_eventid ) )
    {
        m_frame_markers.show_dlg( m_trace_events, m_create_filter_eventid );
        m_create_filter_eventid = INVALID_ID;
    }
    m_frame_markers.render_dlg( m_trace_events );
}
