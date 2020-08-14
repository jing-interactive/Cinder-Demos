#include "LightSpeedApp.h"
#include "AssetManager.h"
#include "MiniConfigImgui.h"
#include "implot/implot.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace rapidjson;

void LightSpeedApp::setup()
{
    log::makeLogger< log::LoggerFileRotating >( fs::path(), "app.%Y.%m.%d.log" );
    createConfigImgui();

    {

#if !defined( GPUVIS_TRACE_UTILS_DISABLE )
        int tracing = -1;

        for ( int i = 1; i < argc; i++ )
        {
            if ( !strcasecmp( argv[ i ], "--trace" ) )
            {
                if ( gpuvis_trace_init() == -1 )
                {
                    printf( "WARNING: gpuvis_trace_init() failed.\n" );
                }
                else
                {
                    printf( "Tracing enabled. Tracefs dir: %s\n", gpuvis_get_tracefs_dir() );

                    gpuvis_start_tracing( 0 );

                    tracing = gpuvis_tracing_on();
                    printf( "gpuvis_tracing_on: %d\n", tracing );
                }
                break;
            }
        }
#endif
        // Initialize colors
        s_clrs().init();
        // Init actions singleton
        s_actions().init();

        // Setup imgui default text color
        s_textclrs().update_colors();
    }

    {
        Document json;
        const auto &str = am::str( DATA_FILENAME );
        if ( !str.empty() )
        {
            json.Parse( str.c_str() );

            if ( json.HasMember( "stats" ) )
            {
                auto &stats_array = json[ "stats" ].GetArray();
                for ( auto &item : stats_array )
                {
                    MetricSeries series( item );
                    storage.metric_storage[ series.name ] = series;
                }
            }
            else if ( json.HasMember( "traceEvents" ) )
            {
                auto &event_array = json[ "traceEvents" ].GetArray();
                for ( auto &item : event_array )
                {
                    SpanSeries series( item );
                    storage.span_storage[ series.name ] = series;
                }
            }
        }
    }

    getWindow()->getSignalKeyUp().connect( [ & ]( KeyEvent &event ) {
        if ( event.getCode() == KeyEvent::KEY_ESCAPE )
            quit();
    } );

    getWindow()->getSignalResize().connect( [ & ] {
        WIN_W = getWindowWidth();
        WIN_H = getWindowHeight();
    } );

    getSignalUpdate().connect( [ & ] {
        bool open = false;
        ImPlot::ShowDemoWindow( &open );

        if ( ImGui::Begin( "Plot" ) )
        {
            for ( const auto &kv : storage.metric_storage )
            {
                auto &series = kv.second;

                ImPlot::SetNextPlotLimits( series.min_t, series.max_t, series.min_x, series.max_x, ImGuiCond_Always );
                ImPlot::SetNextPlotTicksX( series.min_t, series.max_t, PANEL_TICK_T );
                ImPlot::SetNextPlotTicksY( series.min_x, series.max_x, PANEL_TICK_X );

                if ( ImPlot::BeginPlot( series.name.c_str(), NULL, NULL, ImVec2( -1, PANEL_HEIGHT ), ImPlotFlags_Default | ImPlotFlags_NoChild ) )
                {
                    //ImPlot::PushStyleColor(ImPlotCol_Line, items[i].Col);
                    ImPlot::PlotLine( series.name.c_str(), MetricSeries::getter, ( void * )&series, series.t_array.size() );
                    //ImPlot::PopStyleColor();
                    ImPlot::EndPlot();
                }
            }
        }
        ImGui::End();

        {
            // Clear keyboard actions.
            s_actions().clear();

            //while ( SDL_PollEvent( &event ) )
            //{
            //    if ( ( event.type == SDL_KEYDOWN ) || ( event.type == SDL_KEYUP ) )
            //        s_keybd().update( event.key );
            //    else if ( ( event.type == SDL_WINDOWEVENT ) && ( event.window.event == SDL_WINDOWEVENT_FOCUS_LOST ) )
            //        s_keybd().clear();
            //    else if ( event.type == SDL_QUIT )
            //        done = true;
            //}

            // bool use_freetype = s_opts().getb( OPT_UseFreetype );
            // s_opts().setb( OPT_UseFreetype, use_freetype );

            // Handle global hotkeys
            handle_hotkeys();

            // Render trace windows
            render();

            // Update app font settings, scale, etc
            update();
        }
    } );

    getSignalCleanup().connect( [ & ] { 
        writeConfig(); 
            // Shut down app
        shutdown();

        // Save color entries
        s_clrs().shutdown();

        logf_clear();

#if !defined( GPUVIS_TRACE_UTILS_DISABLE )
        if ( tracing == 1 )
        {
            char filename[ PATH_MAX ];
            int ret = gpuvis_trigger_capture_and_keep_tracing( filename, sizeof( filename ) );

            printf( "Tracing wrote '%s': %d\n", filename, ret );
        }

        gpuvis_trace_shutdown();
#endif
        
        } );

    getWindow()->getSignalDraw().connect( [ & ] {
        gl::clear();
    } );
}

CINDER_APP( LightSpeedApp, RendererGl, []( App::Settings *settings ) {
    readConfig();
    settings->setWindowSize( WIN_W, WIN_H);
    settings->setMultiTouchEnabled( false );
} )
