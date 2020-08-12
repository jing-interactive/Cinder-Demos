#include "LightSpeedApp.h"
#include "gpuvis.h"
#include "gpuvis_etl.h"
#include "ya_getopt.h"

#include "MiniConfig.h"

// https://github.com/ocornut/imgui/issues/88
#if defined( __APPLE__ )
#define NOC_FILE_DIALOG_IMPLEMENTATION
#define NOC_FILE_DIALOG_OSX
#include "noc_file_dialog.h"
#elif defined( USE_GTK3 )
#define NOC_FILE_DIALOG_IMPLEMENTATION
#define NOC_FILE_DIALOG_GTK
#include "noc_file_dialog.h"
#elif defined( WIN32 )
#define NOC_FILE_DIALOG_IMPLEMENTATION
#define NOC_FILE_DIALOG_WIN32
#include "noc_file_dialog.h"
#endif

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace rapidjson;

LightSpeedApp &s_app()
{
    auto app = App::get();
    return (LightSpeedApp&)*app;
}

LightSpeedApp::state_t LightSpeedApp::get_state()
{
    return ( state_t )m_loading_info.state.load();
}

bool LightSpeedApp::is_trace_loaded()
{
    return m_trace_win && ( m_trace_win->m_trace_events.get_load_status() == TraceEvents::Trace_Loaded );
}

void LightSpeedApp::set_state( state_t state, const char *filename )
{
    if ( state == State_Loading )
        m_loading_info.filename = filename;
    else
        m_loading_info.filename.clear();

    m_loading_info.win = NULL;
    //m_loading_info.thread.detach();

    m_loading_info.state.store( state );
}

void LightSpeedApp::cancel_load_file()
{
    // Switch to cancel loading if we're currently loading
    if ( m_loading_info.state.load() == State_Loading )
        m_loading_info.state.store( State_CancelLoading );
    //m_loading_info.state.compare_exchange_weak( State_Loading, State_CancelLoading );
}

static std::string unzip_first_file( const char *zipfile )
{
    std::string ret;
#if 0
    mz_zip_archive zip_archive;

    memset( &zip_archive, 0, sizeof( zip_archive ) );

    if ( mz_zip_reader_init_file( &zip_archive, zipfile, 0 ) )
    {
        mz_uint fileCount = mz_zip_reader_get_num_files( &zip_archive );

        if ( fileCount )
        {
            mz_zip_archive_file_stat file_stat;

            if ( mz_zip_reader_file_stat( &zip_archive, 0, &file_stat ) )
            {
                for ( mz_uint i = 0; i < fileCount; i++ )
                {
                    if ( !mz_zip_reader_file_stat( &zip_archive, i, &file_stat ) )
                        continue;

                    if ( file_stat.m_is_directory )
                        continue;

#if defined( _WIN32 )
                    const char *filename = util_basename( file_stat.m_filename );

                    ret = string_format( "%s_%s", std::tmpnam( NULL ), filename );
#else
                    const std::string tstr = string_strftime();
                    const char *filename = util_basename( file_stat.m_filename );
                    const char *dot = strrchr( filename, '.' );
                    int len = dot ? ( dot - filename ) : strlen( filename );

                    ret = string_format( "%s/%.*s_%s.dat", P_tmpdir, len, filename, tstr.c_str() );
#endif
                    if ( mz_zip_reader_extract_to_file( &zip_archive, i, ret.c_str(), 0 ) )
                        break;

                    ret.clear();
                }
            }
        }

        mz_zip_reader_end( &zip_archive );
    }
#endif
    return ret;
}

bool LightSpeedApp::load_file( const char *filename, bool last )
{
    GPUVIS_TRACE_BLOCKF( "%s: %s", __func__, filename );

    std::string tmpfile;
    const char *ext = strrchr( filename, '.' );

    if ( get_state() != State_Idle )
    {
        logf( "[Error] %s failed, currently loading %s.", __func__, m_loading_info.filename.c_str() );
        return false;
    }

    if ( ext && !strcmp( ext, ".zip" ) )
    {
        tmpfile = unzip_first_file( filename );

        if ( !tmpfile.empty() )
            filename = tmpfile.c_str();
    }

    const char *real_ext = strrchr( filename, '.' );
    if ( real_ext && !strcmp( real_ext, ".etl" ) )
    {
        m_trace_type = trace_type_etl;
    }
    else if ( real_ext && ( !strcmp( real_ext, ".dat" ) || !strcmp( real_ext, ".trace" ) ) )
    {
        m_trace_type = trace_type_trace;
    }
    else if ( real_ext && ( !strcmp( real_ext, ".i915-dat" ) || !strcmp( real_ext, ".i915-trace" ) ) )
    {
        m_trace_type = trace_type_i915_perf_trace;
    }

    size_t filesize = get_file_size( filename );
    if ( !filesize )
    {
        logf( "[Error] %s (%s) failed: %s", __func__, filename, strerror( errno ) );
        return false;
    }

    set_state( State_Loading, filename );

    // delete m_trace_win;
    if ( !m_trace_win )
        m_trace_win = new TraceWin( filename, filesize );

    m_loading_info.win = m_trace_win;
    m_loading_info.type = m_trace_type;
    m_loading_info.thread = std::thread( thread_func, &m_loading_info );
    m_loading_info.last = last;
    if ( false /*m_loading_info.thread.get_id() == 0*/ )
    {
        logf( "[Error] %s: SDL_CreateThread failed.", __func__ );

        delete m_trace_win;
        m_trace_win = NULL;

        set_state( State_Idle );
        return false;
    }

    return true;
}


int LightSpeedApp::load_trace_file( loading_info_t *loading_info, TraceEvents &trace_events, EventCallback trace_cb )
{
    return read_trace_file( loading_info->filename.c_str(), trace_events.m_strpool,
        trace_events.m_trace_info, trace_cb );
}

int LightSpeedApp::load_etl_file( loading_info_t *loading_info, TraceEvents &trace_events, EventCallback trace_cb )
{
    return read_etl_file( loading_info->filename.c_str(), trace_events.m_strpool,
        trace_events.m_trace_info, trace_cb );
}

int LightSpeedApp::load_i915_perf_file( loading_info_t *loading_info, TraceEvents &trace_events, EventCallback trace_cb )
{
#ifdef USE_I915_PERF
    return read_i915_perf_file( loading_info->filename.c_str(), trace_events.m_strpool,
        trace_events.m_trace_info, &trace_events.i915_perf_reader, trace_cb );
#else
    return 0;
#endif
}

int LightSpeedApp::thread_func( void *data )
{
    util_time_t t0 = util_get_time();
    loading_info_t *loading_info = ( loading_info_t *)data;
    TraceEvents &trace_events = loading_info->win->m_trace_events;
    const char *filename = loading_info->filename.c_str();

    {
        GPUVIS_TRACE_BLOCKF( "read_trace_file: %s", filename );

        logf( "Reading trace file %s...", filename );

        EventCallback trace_cb = std::bind( &TraceEvents::new_event_cb, &trace_events, _1 );
        trace_events.m_trace_info.trim_trace = TrimTrace;
        trace_events.m_trace_info.m_tracestart = loading_info->tracestart;
        trace_events.m_trace_info.m_tracelen = loading_info->tracelen;
        loading_info->tracestart = 0;
        loading_info->tracelen = 0;

        int ret = 0;
        switch ( loading_info->type )
        {
        case trace_type_trace:
            ret = load_trace_file( loading_info, trace_events, trace_cb );
            break;
        case trace_type_etl:
            ret = load_etl_file( loading_info, trace_events, trace_cb );
            break;
        case trace_type_i915_perf_trace:
            ret = load_i915_perf_file( loading_info, trace_events, trace_cb );
            break;
        default:
            ret = -1;
            break;
        }

        if ( ret < 0 )
        {
            logf( "[Error] load_trace_file(%s) failed.", filename );

            // -1 means loading error
            trace_events.m_eventsloaded.store( -1 );
            s_app().set_state( State_Idle );
            return -1;
        }
    }

    if (loading_info->last)
    {
        GPUVIS_TRACE_BLOCK( "trace_init" );

        // Sort events (with multiple files, events are added out of order)
        std::sort( trace_events.m_events.begin(), trace_events.m_events.end(),
                   [=]( const trace_event_t& lx, const trace_event_t& rx )
                       {
                           return lx.ts < rx.ts;
                       } );

        // Assign event ids
        for ( uint32_t i = 0; i < trace_events.m_events.size(); i++ ) {
            trace_event_t& event = trace_events.m_events[i];

            event.id = i;

            // If this is a sched_switch event, see if it has comm info we don't know about.
            // This is the reason we're initializing events in two passes to collect all this data.
            if ( event.is_sched_switch() )
            {
                add_sched_switch_pid_comm( trace_events.m_trace_info, event, "prev_pid", "prev_comm" );
                add_sched_switch_pid_comm( trace_events.m_trace_info, event, "next_pid", "next_comm" );
            }
            else if ( event.is_ftrace_print() )
            {
                trace_events.new_event_ftrace_print( event );
            }
        }

        float time_load = util_time_to_ms( t0, util_get_time() );

        // Call TraceEvents::init() to initialize all events, etc.
        trace_events.init();

        float time_init = util_time_to_ms( t0, util_get_time() ) - time_load;

        const std::string str = string_format(
                    "Events read: %lu (Load:%.2fms Init:%.2fms) (string chunks:%lu size:%lu)",
                    trace_events.m_events.size(), time_load, time_init,
                    trace_events.m_strpool.m_alloc.m_chunks.size(), trace_events.m_strpool.m_alloc.m_totsize );
        logf( "%s", str.c_str() );

#if !defined( GPUVIS_TRACE_UTILS_DISABLE )
        printf( "%s\n", str.c_str() );
#endif

        // 0 means events have all all been loaded
        trace_events.m_eventsloaded.store( 0 );
    }

    s_app().set_state( State_Idle );

    return 0;
}

void LightSpeedApp::init( int argc, char **argv )
{
    parse_cmdline( argc, argv );

    imgui_set_custom_style( s_clrs().getalpha( col_ThemeAlpha ) );

    logf( "Welcome to gpuvis\n" );
    logf( " " );

    imgui_set_scale( Scale);
}

void LightSpeedApp::shutdown( )
{
    GPUVIS_TRACE_BLOCK( __func__ );

#if 0
    if ( window )
    {
        // Write main window position / size to ini file.
        int x, y, w, h;
        int top, left, bottom, right;

        SDL_GetWindowBordersSize( window, &top, &left, &bottom, &right );
        SDL_GetWindowPosition( window, &x, &y );
        SDL_GetWindowSize( window, &w, &h );

        save_window_pos( x - left, y - top, w, h );
    }
#endif

    //if ( m_loading_info.thread)
    {
        // Cancel any file loading going on.
        cancel_load_file();

        // Wait for our thread to die.
        if ( m_loading_info.thread.joinable() )
            m_loading_info.thread.join();
    }

    set_state( State_Idle );

    delete m_trace_win;
    m_trace_win = NULL;
}

void LightSpeedApp::render_save_filename()
{
    struct stat st;
    float w = imgui_scale( 300.0f );
    bool window_appearing = ImGui::IsWindowAppearing();
    bool do_save = s_actions().get( action_return );

    // Text label
    ImGui::Text( "%s", m_saving_info.title.c_str() );

    // New filename input text field
    if ( imgui_input_text2( "New Filename:", m_saving_info.filename_buf, w, 0 ) || window_appearing )
    {
        m_saving_info.errstr.clear();
        m_saving_info.filename_new = get_realpath( m_saving_info.filename_buf );

        if ( !m_saving_info.filename_new.empty() &&
             ( m_saving_info.filename_new != m_saving_info.filename_orig ) &&
             !stat( m_saving_info.filename_new.c_str(), &st ) )
        {
            m_saving_info.errstr = string_format( "WARNING: %s exists", m_saving_info.filename_new.c_str() );
        }
    }

    // Set focus to input text on first pass through
    if ( window_appearing )
        ImGui::SetKeyboardFocusHere( -1 );

    // Spew out any error / warning messages
    if ( !m_saving_info.errstr.empty() )
        ImGui::TextColored( ImVec4( 1, 0, 0, 1 ), "%s", m_saving_info.errstr.c_str() );

    bool disabled = m_saving_info.filename_new.empty() ||
            ( m_saving_info.filename_new == m_saving_info.filename_orig );

    // Save button
    {
        ImGuiCol idx = disabled ? ImGuiCol_TextDisabled : ImGuiCol_Text;
        ImGuiButtonFlags flags = disabled ? ImGuiButtonFlags_Disabled : 0;

        ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( idx ) );
        do_save |= ImGui::ButtonEx( "Save", ImVec2( w / 3.0f, 0 ), flags );
        ImGui::PopStyleColor();
    }

    bool close_popup = false;
    if ( do_save && !disabled )
        close_popup = m_saving_info.save_cb( m_saving_info );

    // Cancel button (or escape key)
    ImGui::SameLine();
    if ( ImGui::Button( "Cancel", ImVec2( w / 3.0f, 0 ) ) ||
         s_actions().get( action_escape ) )
    {
        close_popup = true;
    }

    if ( close_popup )
    {
        ImGui::CloseCurrentPopup();

        m_saving_info.filename_buf[ 0 ] = 0;
        m_saving_info.title.clear();
        m_saving_info.filename_new.clear();
        m_saving_info.filename_orig.clear();
        m_saving_info.errstr.clear();
    }
}

static void imgui_setnextwindowsize( float w, float h, float x = -1.0f, float y = -1.0f )
{
    if ( x >= 0.0f )
    {
        ImGui::SetNextWindowPos( ImVec2( imgui_scale( x ), imgui_scale( y ) ),
                                 ImGuiCond_FirstUseEver );
    }

    ImGui::SetNextWindowSize( ImVec2( imgui_scale( w ), imgui_scale( h ) ),
                              ImGuiCond_FirstUseEver );
}

static void render_help_entry( const char *hotkey, const char *desc )
{
    const char *colon = strchr( desc, ':' );

    ImGui::Text( "%s", s_textclrs().bright_str( hotkey ).c_str() );
    ImGui::NextColumn();

    if ( colon )
    {
        int len = ( int )( colon - desc );
        const char *clr_def = s_textclrs().str( TClr_Def );
        const char *clr_brightcomp = s_textclrs().str( TClr_BrightComp );

        ImGui::Text( "%s%.*s%s:%s", clr_brightcomp, len, desc, clr_def, colon + 1 );
    }
    else
    {
        ImGui::Text( "%s", desc );
    }
    ImGui::NextColumn();

    ImGui::Separator();
}

void LightSpeedApp::render()
{
    if ( m_trace_win && m_trace_win->m_open )
    {
        float w = ImGui::GetIO().DisplaySize.x;
        float h = ImGui::GetIO().DisplaySize.y;

        ImGui::SetNextWindowPos( ImVec2( 0, 0 ), ImGuiCond_Always );
        ImGui::SetNextWindowSizeConstraints( ImVec2( w, h ), ImVec2( w, h ) );

        m_trace_win->render();
    }
    else if ( m_trace_win )
    {
        delete m_trace_win;
        m_trace_win = NULL;
    }
    else if ( !m_show_scale_popup && m_loading_info.inputfiles.empty() )
    {
        // If we have no main window and nothing to load, show the console
        m_show_gpuvis_console = true;
    }

    // Render dialogs only if the scale popup dialog isn't up
    if ( !m_show_scale_popup )
    {
        if ( m_focus_gpuvis_console )
        {
            ImGui::SetWindowFocus( "Gpuvis Console" );
            m_show_gpuvis_console = true;
            m_focus_gpuvis_console = false;
        }
        if ( m_show_gpuvis_console )
        {
            imgui_setnextwindowsize( 600, 600, 4, 4 );

            render_console();
        }

        if ( m_show_imgui_test_window )
        {
            imgui_setnextwindowsize( 800, 600 );

            ImGui::ShowDemoWindow( &m_show_imgui_test_window );
        }

        if ( m_show_imgui_style_editor )
        {
            imgui_setnextwindowsize( 800, 600 );

            ImGui::Begin( "Style Editor", &m_show_imgui_style_editor );
            ImGui::ShowStyleEditor();
            ImGui::End();
        }

        if ( m_show_imgui_metrics_editor )
        {
            ImGui::ShowMetricsWindow( &m_show_imgui_metrics_editor );
        }

        if ( m_show_font_window )
        {
            imgui_setnextwindowsize( 800, 600 );

            ImGui::Begin( "Font Options", &m_show_font_window );
            render_font_options();
            ImGui::End();
        }

        if ( m_show_color_picker )
        {
            imgui_setnextwindowsize( 800, 600 );

            ImGui::Begin( "Color Configuration", &m_show_color_picker );
            render_color_picker();
            ImGui::End();
        }

        if ( !m_show_trace_info.empty() && is_trace_loaded() )
        {
            bool show_trace_info = !!m_trace_win;

            if ( show_trace_info )
            {
                imgui_setnextwindowsize( 800, 600 );

                ImGui::Begin( m_show_trace_info.c_str(), &show_trace_info );
                m_trace_win->trace_render_info();
                ImGui::End();

                if ( s_actions().get( action_escape ) )
                    show_trace_info = false;
            }

            if ( !show_trace_info )
                m_show_trace_info.clear();
        }

        if ( !m_saving_info.title.empty() && !ImGui::IsPopupOpen( "Save Filename" ) )
            ImGui::OpenPopup( "Save Filename" );
        if ( ImGui::BeginPopupModal( "Save Filename", NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
        {
            render_save_filename();
            ImGui::EndPopup();

            m_show_help = false;
        }

        if ( m_show_help && !ImGui::IsPopupOpen( "GpuVis Help" ) )
        {
            ImGui::OpenPopup( "GpuVis Help" );

            imgui_setnextwindowsize( 600, 600 );
        }
        if ( ImGui::BeginPopupModal( "GpuVis Help", &m_show_help, 0 ) )
        {
            static const struct
            {
                const char *hotkey;
                const char *desc;
            } s_help[] =
            {
                { "Ctrl+click drag", "Graph: Select area" },
                { "Shift+click drag", "Graph: Zoom selected area" },
                { "Mousewheel", "Graph: Zoom in / out" },
                { "Alt down", "Graph: Hide labels" },
            };
            int graph_entry_count = 0;

            if ( imgui_begin_columns( "gpuvis_help", { "Hotkey", "Description" } ) )
                ImGui::SetColumnWidth( 0, imgui_scale( 170.0f ) );

            for ( const Actions::actionmap_t &map : s_actions().m_actionmap )
            {
                if ( map.desc )
                {
                    bool is_graph_section = !strncmp( map.desc, "Graph: ", 7 );
                    const std::string hotkey = s_actions().hotkey_str( map.action );

                    if ( is_graph_section && !graph_entry_count )
                    {
                        // If this is the first "Graph: " entry, sneak the mouse actions in
                        for ( size_t i = 0; i < ARRAY_SIZE( s_help ); i++ )
                            render_help_entry( s_help[ i ].hotkey, s_help[ i ].desc );
                    }

                    render_help_entry( hotkey.c_str(), map.desc );

                    graph_entry_count += is_graph_section;
                }
            }

            ImGui::EndColumns();

            if ( s_actions().get( action_escape ) )
            {
                m_show_help = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    if ( m_show_scale_popup && !ImGui::IsPopupOpen( "Display Scaling" ) )
        ImGui::OpenPopup( "Display Scaling" );
    if ( ImGui::BeginPopupModal( "Display Scaling", NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
    {
        ImGui::Text( "Are you running on a high resolution display?" );
        ImGui::Text( " You can update settings in Font Options dialog." );

        ImGui::Separator();

        if ( ImGui::Button( "Yes", ImVec2( 150, 0 ) ) )
        {
            ImGui::CloseCurrentPopup();
            m_show_scale_popup = false;
        }

        ImGui::SameLine();
        if ( ImGui::Button( "No", ImVec2( 150, 0 ) ) )
        {
            Scale = 1.0f;
            m_font_main.m_changed = true;
            ImGui::CloseCurrentPopup();
            m_show_scale_popup = false;
        }

        ImGui::EndPopup();
    }
}

void LightSpeedApp::update()
{
    if ( !m_loading_info.inputfiles.empty() && ( get_state() == State_Idle ) )
    {
        const char *filename = m_loading_info.inputfiles[ 0 ].c_str();

        load_file( filename, m_loading_info.inputfiles.size() == 1 );

        m_loading_info.inputfiles.erase( m_loading_info.inputfiles.begin() );
    }

    if ( ( m_font_main.m_changed || m_font_small.m_changed ) &&
         !ImGui::IsMouseDown( 0 ) )
    {
        imgui_set_scale( Scale);

        load_fonts();
    }
}

void LightSpeedApp::load_fonts()
{
    // Clear all font texture data, ttf data, glyphs, etc.
    ImGui::GetIO().Fonts->Clear();

    // Add main font
    m_font_main.load_font( "$imgui_font_main$", "Roboto Regular", 14.0f );

    // Add small font
    m_font_small.load_font( "$imgui_font_small$", "Roboto Condensed", 14.0f );


    // Reset max rect size for the print events so they'll redo the CalcTextSize for the
    //  print graph row backgrounds (in graph_render_print_timeline).
    if ( m_trace_win )
        m_trace_win->m_trace_events.invalidate_ftraceprint_colors();

    //if ( s_ini().GetFloat( "scale", -1.0f ) == -1.0f )
    {
        //s_ini().PutFloat( "scale", Scale  );

        m_show_scale_popup = true;
    }
}

static const std::string trace_info_label( TraceEvents &trace_events )
{
    const char *basename = get_path_filename( trace_events.m_filename.c_str() );

    return string_format( "Info for '%s'", s_textclrs().bright_str( basename ).c_str() );
}

void LightSpeedApp::render_menu_options()
{
    if ( s_actions().get( action_escape ) )
        ImGui::CloseCurrentPopup();

    {
        ImGui::TextColored( s_clrs().getv4( col_BrightText ), "%s", "Windows" );
        ImGui::Indent();

        if ( ImGui::MenuItem( "GpuVis Help", s_actions().hotkey_str( action_help ).c_str() ) )
        {
            ImGui::SetWindowFocus( "GpuVis Help" );
            m_show_help = true;
        }

        if ( ImGui::MenuItem( "Gpuvis Console" ) )
            m_focus_gpuvis_console = true;

        if ( ImGui::MenuItem( "Font Options" ) )
        {
            ImGui::SetWindowFocus( "Font Options" );
            m_show_font_window = true;
        }

        if ( ImGui::MenuItem( "Color Configuration" ) )
        {
            ImGui::SetWindowFocus( "Color Configuration" );
            m_show_color_picker = true;
        }

        // If we have a trace window and the events are loaded, show Trace Info menu item
        if ( is_trace_loaded() )
        {
            const std::string label = trace_info_label( m_trace_win->m_trace_events );

            ImGui::Separator();

            if ( ImGui::MenuItem( label.c_str(), s_actions().hotkey_str( action_trace_info ).c_str() ) )
            {
                ImGui::SetWindowFocus( label.c_str() );
                m_show_trace_info = label;
            }
        }

        ImGui::Separator();

        if ( ImGui::MenuItem( "ImGui Style Editor" ) )
        {
            ImGui::SetWindowFocus( "Style Editor" );
            m_show_imgui_style_editor = true;
        }

        if ( ImGui::MenuItem( "ImGui Metrics" ) )
        {
            ImGui::SetWindowFocus( "ImGui Metrics" );
            m_show_imgui_metrics_editor = true;
        }

        if ( ImGui::MenuItem( "ImGui Test Window" ) )
        {
            ImGui::SetWindowFocus( "ImGui Demo" );
            m_show_imgui_test_window = true;
        }

        ImGui::Unindent();
    }

    ImGui::Separator();

    ImGui::TextColored( s_clrs().getv4( col_BrightText ), "%s", "Gpuvis Settings" );
    ImGui::Indent();

    //s_opts().render_imgui_options();

    ImGui::Unindent();
}

void LightSpeedApp::render_font_options()
{
    static const char lorem_str[] =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do"
        "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim"
        "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo"
        "consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse"
        "cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non"
        "proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

    ImGui::Indent();
    ImGui::PushID( "font_options" );

    {
        bool changed = false;

#ifdef USE_FREETYPE
        changed |= s_opts().render_imgui_opt( OPT_UseFreetype );
#endif
        //changed |= s_opts().render_imgui_opt( OPT_Scale );
        //changed |= s_opts().render_imgui_opt( OPT_Gamma );

        if ( ImGui::Button( "Reset to Defaults" ) )
        {
            m_font_main.m_reset = true;
            m_font_small.m_reset = true;

            Gamma = 1.4f;
            changed = true;
        }

        if ( changed )
        {
            // Ping font change so this stuff will reload in main loop.
            m_font_main.m_changed = true;
        }
    }

    if ( ImGui::TreeNodeEx( "Main Font", ImGuiTreeNodeFlags_DefaultOpen ) )
    {
        std::string font_name = s_textclrs().bright_str( m_font_main.m_name );

        ImGui::TextWrapped( "%s: %s", font_name.c_str(), lorem_str );

        m_font_main.render_font_options( UseFreetype );
        ImGui::TreePop();
    }

    if ( ImGui::TreeNodeEx( "Small Font", ImGuiTreeNodeFlags_DefaultOpen ) )
    {
        std::string font_name = s_textclrs().bright_str( m_font_small.m_name );

        ImGui::BeginChild( "small_font", ImVec2( 0, ImGui::GetTextLineHeightWithSpacing() * 4 ) );

        imgui_push_smallfont();
        ImGui::TextWrapped( "%s: %s", font_name.c_str(), lorem_str );
        imgui_pop_font();

        ImGui::EndChild();

        m_font_small.render_font_options( UseFreetype);

        ImGui::TreePop();
    }

    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    if (ImGui::TreeNode( "Font atlas texture", "Atlas texture (%dx%d pixels)", atlas->TexWidth, atlas->TexHeight ) )
    {
        ImGui::Image( atlas->TexID,
                      ImVec2( (float )atlas->TexWidth, ( float )atlas->TexHeight),
                      ImVec2( 0, 0 ), ImVec2( 1, 1 ),
                      ImVec4( 1, 1, 1, 1 ), ImVec4( 1, 1, 1, 0.5f ) );
        ImGui::TreePop();
    }

    ImGui::PopID();
    ImGui::Unindent();
}

static void render_color_items( colors_t i0, colors_t i1,
        colors_t *selected_color, std::string *selected_color_event )
{
    const float w = imgui_scale( 32.0f );
    const float text_h = ImGui::GetTextLineHeight();

    for ( colors_t i = i0; i < i1; i++ )
    {
        ImGui::BeginGroup();

        ImU32 color = s_clrs().get( i );
        const char *name = s_clrs().name( i );
        bool selected = ( i == *selected_color );
        ImVec2 pos = ImGui::GetCursorScreenPos();

        // Draw colored rectangle
        ImGui::GetWindowDrawList()->AddRectFilled( pos, ImVec2( pos.x + w, pos.y + text_h ), color );

        // Draw color name
        ImGui::Indent( imgui_scale( 40.0f ) );
        if ( ImGui::Selectable( name, selected, 0 ) )
        {
            *selected_color = i;
            selected_color_event->clear();
        }
        ImGui::Unindent( imgui_scale( 40.0f ) );

        ImGui::EndGroup();

        // Tooltip with description
        if ( ImGui::IsItemHovered() )
            ImGui::SetTooltip( "%s", s_clrs().desc( i ) );
    }
}

static trace_event_t *get_first_colorable_event(
        TraceEvents &trace_events, const char *eventname )
{
    const std::vector< uint32_t > *plocs =
            trace_events.m_eventnames_locs.get_locations_str( eventname );

    if ( plocs )
    {
        for ( uint32_t idx : *plocs )
        {
            trace_event_t &event = trace_events.m_events[ idx ];

            if ( event.is_ftrace_print() )
                break;
            if ( !( event.flags & TRACE_FLAG_AUTOGEN_COLOR ) )
                return &event;
        }
    }

    return NULL;
}

static void render_color_event_items( TraceEvents &trace_events,
        colors_t *selected_color, std::string *selected_color_event )
{
    const float w = imgui_scale( 32.0f );
    const float text_h = ImGui::GetTextLineHeight();

    // Iterate through all the event names
    for ( auto item : trace_events.m_eventnames_locs.m_locs.m_map )
    {
        const char *eventname = trace_events.m_strpool.findstr( item.first );
        const trace_event_t *event = get_first_colorable_event( trace_events, eventname );

        if ( event )
        {
            ImU32 color = event->color ? event->color : s_clrs().get( col_Graph_1Event );

            ImGui::BeginGroup();

            bool selected = ( eventname == *selected_color_event );
            ImVec2 pos = ImGui::GetCursorScreenPos();

            // Draw colored rectangle
            ImGui::GetWindowDrawList()->AddRectFilled( pos, ImVec2( pos.x + w, pos.y + text_h ), color );

            // Draw event name
            ImGui::Indent( imgui_scale( 40.0f ) );
            if ( ImGui::Selectable( eventname, selected, 0 ) )
            {
                *selected_color = col_Max;
                *selected_color_event = eventname;
            }
            ImGui::Unindent( imgui_scale( 40.0f ) );

            ImGui::EndGroup();
        }
    }
}

static bool render_color_picker_colors( ColorPicker &colorpicker, colors_t selected_color )
{
    bool changed = false;
    ImU32 color = s_clrs().get( selected_color );
    const char *name = s_clrs().name( selected_color );
    const char *desc = s_clrs().desc( selected_color );
    std::string brightname = s_textclrs().bright_str( name );
    bool is_alpha = s_clrs().is_alpha_color( selected_color );
    ImU32 def_color = s_clrs().getdef( selected_color );

    // Color name and description
    imgui_text_bg( ImGui::GetStyleColorVec4( ImGuiCol_Header ),
                   "%s: %s", brightname.c_str(), desc );

    ImGui::NewLine();
    if ( colorpicker.render( color, is_alpha, def_color ) )
    {
        s_clrs().set( selected_color, colorpicker.m_color );
        changed = true;
    }

    return changed;
}

static bool render_color_picker_event_colors( ColorPicker &colorpicker,
        TraceWin *win, const std::string &selected_color_event )
{
    bool changed = false;
    TraceEvents &trace_events = win->m_trace_events;
    const trace_event_t *event = get_first_colorable_event( trace_events, selected_color_event.c_str() );

    if ( event )
    {
        std::string brightname = s_textclrs().bright_str( selected_color_event.c_str() );
        ImU32 color = event->color ? event->color : s_clrs().get( col_Graph_1Event );
        ImU32 def_color = s_clrs().get( col_Graph_1Event );

        // Color name and description
        imgui_text_bg( ImGui::GetStyleColorVec4( ImGuiCol_Header ), "%s", brightname.c_str() );

        ImGui::NewLine();
        changed = colorpicker.render( color, false, def_color );

        if ( changed && ( colorpicker.m_color == def_color ) )
            colorpicker.m_color = 0;
    }

    return changed;
}

static void update_changed_colors( TraceEvents &trace_events, colors_t color )
{
    switch( color )
    {
    case col_FtracePrintText:
        trace_events.invalidate_ftraceprint_colors();
        break;

    case col_Graph_PrintLabelSat:
    case col_Graph_PrintLabelAlpha:
        // ftrace print label color changes - invalidate current colors
        trace_events.invalidate_ftraceprint_colors();
        trace_events.update_tgid_colors();
        break;

    case col_Graph_TimelineLabelSat:
    case col_Graph_TimelineLabelAlpha:
        // fence_signaled event color change - update event fence_signaled colors
        trace_events.update_fence_signaled_timeline_colors();
        break;
    }
}

static void reset_colors_to_default( TraceWin *win )
{
    for ( colors_t i = 0; i < col_Max; i++ )
        s_clrs().reset( i );

    if ( win )
    {
        win->m_trace_events.invalidate_ftraceprint_colors();
        win->m_trace_events.update_tgid_colors();
        win->m_trace_events.update_fence_signaled_timeline_colors();
    }

    imgui_set_custom_style( s_clrs().getalpha( col_ThemeAlpha ) );

    s_textclrs().update_colors();
}

static void reset_event_colors_to_default( TraceWin *win )
{
    #if 0
    std::vector< INIEntry > entries = s_ini().GetSectionEntries( "$imgui_eventcolors$" );

    for ( const INIEntry &entry : entries )
    {
        s_ini().PutStr( entry.first.c_str(), "", "$imgui_eventcolors$" );
    }
    #endif

    if ( win )
    {
        for ( trace_event_t &event : win->m_trace_events.m_events )
        {
            // If it's not an autogen'd color, reset color back to 0
            if ( !( event.flags & TRACE_FLAG_AUTOGEN_COLOR ) )
                event.color = 0;
        }
    }
}

void LightSpeedApp::render_color_picker()
{
    bool changed = false;
    TraceWin *win = is_trace_loaded() ? m_trace_win : NULL;

    if ( ImGui::Button( "Reset All to Defaults" ) )
    {
        reset_colors_to_default( win );
        reset_event_colors_to_default( win );
    }

    ImGui::Separator();

    if ( imgui_begin_columns( "color_picker", 2, 0 ) )
        ImGui::SetColumnWidth( 0, imgui_scale( 250.0f ) );

    /*
     * Column 1: draw our graph items and their colors
     */
    {
        ImGui::BeginChild( "color_list" );

        if ( ImGui::CollapsingHeader( "GpuVis Colors" ) )
        {
            render_color_items( 0, col_ImGui_Text,
                                &m_colorpicker_color, &m_colorpicker_event );
        }

        if ( ImGui::CollapsingHeader( "ImGui Colors" ) )
        {
            render_color_items( col_ImGui_Text, col_Max,
                                &m_colorpicker_color, &m_colorpicker_event );
        }

        if ( !win )
        {
            m_colorpicker_event.clear();
        }
        else if ( ImGui::CollapsingHeader( "Event Colors" ) )
        {
            render_color_event_items( win->m_trace_events,
                    &m_colorpicker_color, &m_colorpicker_event );
        }

        ImGui::EndChild();
    }
    ImGui::NextColumn();

    /*
     * Column 2: Draw our color picker
     */
    if ( m_colorpicker_color < col_Max )
    {
        changed |= render_color_picker_colors( m_colorpicker, m_colorpicker_color );
    }
    else if ( !m_colorpicker_event.empty() && win )
    {
        changed |= render_color_picker_event_colors( m_colorpicker,
                win, m_colorpicker_event );
    }

    ImGui::NextColumn();
    ImGui::EndColumns();

    if ( changed )
    {
        if ( m_colorpicker_color < col_Max )
        {
            if ( win )
                update_changed_colors( win->m_trace_events, m_colorpicker_color );

            // imgui color change - set new imgui colors
            if ( s_clrs().is_imgui_color( m_colorpicker_color ) )
                imgui_set_custom_style( s_clrs().getalpha( col_ThemeAlpha ) );

            s_textclrs().update_colors();
        }
        else if ( !m_colorpicker_event.empty() && win )
        {
            win->m_trace_events.set_event_color( m_colorpicker_event, m_colorpicker.m_color );
        }
    }
}

void LightSpeedApp::render_log()
{
    ImGui::Text( "Log Filter:" );
    ImGui::SameLine();
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 0 ) );
    m_filter.Draw( "##log-filter", 180 );
    ImGui::PopStyleVar();

    ImGui::SameLine();
    if ( ImGui::SmallButton( "Clear" ) )
        logf_clear();

    ImGui::SameLine();
    if ( ImGui::SmallButton( "Scroll to bottom" ) )
        m_log_size = ( size_t )-1;

    ImGui::Separator();

    {
        ImGui::BeginChild( "ScrollingRegion",
                           ImVec2( 0, -ImGui::GetTextLineHeightWithSpacing() ),
                           false, ImGuiWindowFlags_HorizontalScrollbar );

        // Log popup menu
        if ( ImGui::BeginPopupContextWindow() )
        {
            if ( ImGui::Selectable( "Clear" ) )
                logf_clear();
            ImGui::EndPopup();
        }

        // Display every line as a separate entry so we can change their color or add custom widgets. If you only want raw text you can use ImGui::TextUnformatted(log.begin(), log.end());
        // NB- if you have thousands of entries this approach may be too inefficient and may require user-side clipping to only process visible items.
        // You can seek and display only the lines that are visible using the ImGuiListClipper helper, if your elements are evenly spaced and you have cheap random access to the elements.
        // To use the clipper we could replace the 'for (int i = 0; i < Items.Size; i++)' loop with:
        //     ImGuiListClipper clipper(Items.Size);
        //     while (clipper.Step())
        //         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        // However take note that you can not use this code as is if a filter is active because it breaks the 'cheap random-access' property. We would need random-access on the post-filtered list.
        // A typical application wanting coarse clipping and filtering may want to pre-compute an array of indices that passed the filtering test, recomputing this array when user changes the filter,
        // and appending newly elements as they are inserted. This is left as a task to the user until we can manage to improve this example code!
        // If your items are of variable size you may want to implement code similar to what ImGuiListClipper does. Or split your data into fixed height items to allow random-seeking into your list.

        // Tighten spacing
        ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 4, 1 ) );

        const std::vector< char * > &log = logf_get();
        for ( const char *item : log )
        {
            if ( !m_filter.PassFilter( item ) )
                continue;

            ImVec4 col = ImVec4( 1, 1, 1, 1 );

            if ( !strncasecmp( item, "[error]", 7 ) )
                col = ImVec4( 1.0f, 0.4f, 0.4f, 1.0f );
            else if ( strncmp( item, "# ", 2 ) == 0 )
                col = ImVec4( 1.0f, 0.78f, 0.58f, 1.0f );

            ImGui::PushStyleColor( ImGuiCol_Text, col );
            ImGui::TextUnformatted( item );
            ImGui::PopStyleColor();
        }

        if ( m_log_size != log.size() )
        {
            ImGui::SetScrollHere();

            m_log_size = log.size();
        }

        ImGui::PopStyleVar();
        ImGui::EndChild();
    }
}

void LightSpeedApp::render_console()
{
    if ( !ImGui::Begin( "Gpuvis Console", &m_show_gpuvis_console, ImGuiWindowFlags_MenuBar ) )
    {
        ImGui::End();
        return;
    }

    render_menu( "menu_console" );

    render_log();

    ImGui::End();
}

#if !defined( NOC_FILE_DIALOG_IMPLEMENTATION )
static const char *noc_file_init()
{
    return "File open dialog NYI";
}

#define NOC_FILE_DIALOG_OPEN 0
static const char *noc_file_dialog_open(int flags, const char *filters,
        const char *default_path, const char *default_name)
{
    return NULL;
}
#endif

void LightSpeedApp::open_trace_dialog()
{
    const char *errstr = noc_file_init();

    if ( errstr )
    {
        logf( "[Error] Open Trace: %s\n", errstr );
    }
    else
    {
        const char *file = noc_file_dialog_open( NOC_FILE_DIALOG_OPEN,
                                 "trace-cmd files (*.dat;*.trace;*.etl;*.zip)\0*.dat;*.trace;*.etl;*.zip\0",
                                 NULL, "trace.dat" );

        if ( file && file[ 0 ] )
            m_loading_info.inputfiles.push_back( file );
    }
}

void LightSpeedApp::render_menu( const char *str_id )
{
    ImGui::PushID( str_id );

    if ( !ImGui::BeginMenuBar() )
    {
        ImGui::PopID();
        return;
    }

    if ( ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) )
    {
        if ( s_actions().get( action_menu_file ) )
            ImGui::OpenPopup( "File" );
        else if ( s_actions().get( action_menu_options ) )
            ImGui::OpenPopup( "Options" );
    }

    if ( ImGui::BeginMenu( "File" ) )
    {
        if ( s_actions().get( action_escape ) )
            ImGui::CloseCurrentPopup();

#if defined( NOC_FILE_DIALOG_IMPLEMENTATION )
        if ( ImGui::MenuItem( "Open Trace File...", s_actions().hotkey_str( action_open ).c_str() ) )
            open_trace_dialog();
#endif

        if ( m_saving_info.title.empty() && is_trace_loaded() )
        {
            std::string &filename = m_trace_win->m_trace_events.m_filename;
            const char *basename = get_path_filename( filename.c_str() );
            std::string label = string_format( "Save '%s' as...", basename );

            if ( ImGui::MenuItem( label.c_str() ) )
            {
                m_saving_info.filename_orig = get_realpath( filename.c_str() );
                m_saving_info.title = string_format( "Save '%s' as:", m_saving_info.filename_orig.c_str() );
                strcpy_safe( m_saving_info.filename_buf, "blah.trace" );

                // Lambda for copying filename_orig to filename_new
                m_saving_info.save_cb = []( save_info_t &save_info )
                {
                    bool close_popup = copy_file( save_info.filename_orig.c_str(), save_info.filename_new.c_str() );

                    if ( !close_popup )
                    {
                        save_info.errstr = string_format( "ERROR: copy_file to %s failed",
                                                              save_info.filename_new.c_str() );
                    }
                    return close_popup;
                };
            }
        }

        if ( ImGui::MenuItem( "Quit", s_actions().hotkey_str( action_quit ).c_str() ) )
        {
            this->quit();
        }

        ImGui::EndMenu();
    }

    if ( ImGui::BeginMenu( "Options") )
    {
        render_menu_options();
        ImGui::EndMenu();
    }

    if ( ShowFps)
    {
        ImGui::Text( "%s%.2f ms/frame (%.1f FPS)%s",
                     s_textclrs().str( TClr_Bright ),
                     1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate,
                     s_textclrs().str( TClr_Def ) );
    }

    ImGui::EndMenuBar();

    ImGui::PopID();
}

void LightSpeedApp::handle_hotkeys()
{
    if ( s_actions().get( action_help ) )
    {
        ImGui::SetWindowFocus( "GpuVis Help" );
        m_show_help = true;
    }

    if ( s_actions().get( action_open ) )
        open_trace_dialog();

    if ( s_actions().get( action_quit ) )
    {
        this->quit();
    }

    if ( s_actions().get( action_trace_info ) && is_trace_loaded() )
    {
        const std::string label = trace_info_label( m_trace_win->m_trace_events );

        ImGui::SetWindowFocus( label.c_str() );
        m_show_trace_info = label;
    }

    if ( s_actions().get( action_toggle_vblank0 ) )
        RenderCrtc0 = !RenderCrtc0;
    if ( s_actions().get( action_toggle_vblank1 ) )
        RenderCrtc1 = !RenderCrtc1;
    if ( s_actions().get( action_toggle_vblank_hardware_timestamps ) )
        VBlankHighPrecTimestamps = !VBlankHighPrecTimestamps;
    if ( s_actions().get( action_toggle_framemarkers ) )
        RenderFrameMarkers = !RenderFrameMarkers;

    if ( s_actions().get( action_toggle_show_eventlist ) )
        ShowEventList = !ShowEventList;

    if (  s_actions().get( action_save_screenshot ) )
    {
        ImGuiIO& io = ImGui::GetIO();
        int w = ( int )io.DisplaySize.x;
        int h = ( int )io.DisplaySize.y;

        // Capture image
        m_imagebuf.CreateFromCaptureGL( 0, 0, w, h );
        m_imagebuf.FlipVertical();

        m_saving_info.filename_orig.clear();
        m_saving_info.title = string_format( "Save gpuvis screenshot (%dx%d) as:", w, h );
        strcpy_safe( m_saving_info.filename_buf, "gpuvis.png" );

        // Lambda for copying filename_orig to filename_new
        m_saving_info.save_cb = [&]( save_info_t &save_info )
        {
            bool close_popup = !!m_imagebuf.SaveFile( save_info.filename_new.c_str() );

            if ( !close_popup )
            {
                save_info.errstr = string_format( "ERROR: save_file to %s failed",
                                                  save_info.filename_new.c_str() );
            }
            return close_popup;
        };
    }
}

void LightSpeedApp::parse_cmdline( int argc, char **argv )
{
    static struct option long_opts[] =
    {
        { "scale", ya_required_argument, 0, 0 },
        { "tracestart", ya_required_argument, 0, 0 },
        { "tracelen", ya_required_argument, 0, 0 },
#if !defined( GPUVIS_TRACE_UTILS_DISABLE )
        { "trace", ya_no_argument, 0, 0 },
#endif
        { 0, 0, 0, 0 }
    };

    int c;
    int opt_ind = 0;
    while ( ( c = ya_getopt_long( argc, argv, "i:",
                                  long_opts, &opt_ind ) ) != -1 )
    {
        switch ( c )
        {
        case 0:
            if ( !strcasecmp( "scale", long_opts[ opt_ind ].name ) )
                Scale = atof(a_optarg ) );
            else if ( !strcasecmp( "tracestart", long_opts[ opt_ind ].name ) )
                m_loading_info.tracestart = timestr_to_ts( ya_optarg );
            else if ( !strcasecmp( "tracelen", long_opts[ opt_ind ].name ) )
                m_loading_info.tracelen = timestr_to_ts( ya_optarg );
            break;
        case 'i':
            m_loading_info.inputfiles.clear();
            m_loading_info.inputfiles.push_back( ya_optarg );
            break;

        default:
            break;
        }
    }

    for ( ; ya_optind < argc; ya_optind++ )
    {
        //m_loading_info.inputfiles.clear();
        m_loading_info.inputfiles.push_back( argv[ ya_optind ] );
    }
}

