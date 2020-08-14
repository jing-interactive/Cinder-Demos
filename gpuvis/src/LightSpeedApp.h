#pragma once

#include <cinder/app/App.h>
#include <cinder/app/RendererGl.h>
#include <cinder/gl/gl.h>
#include <cinder/Log.h>
#include <imgui/imgui.h>
#include "implot/implot.h"

#include "../../blocks/rapidjson/rapidjson.h"
#include "../../blocks/rapidjson/document.h"

#include "gpuvis_macros.h"
#include "gpuvis.h"
#include "gpuvis_utils.h"

using namespace rapidjson;

struct Span
{
    std::string name;
    float start = 0;
    float end = 0;
};

struct SpanSeries
{
    std::string name;
    std::vector<Span> span_array;

    SpanSeries()
    {
    }

    SpanSeries(const Document::GenericValue& stat)
    {
        name = stat["name"].GetString();
        auto pid = stat["pid"].GetInt();
        auto& ph = stat["ph"];
        if (ph == "X")
        {
            auto tid = stat["tid"].GetInt();
            auto ts = stat["ts"].GetInt();
            auto dur = stat["dur"].GetInt();
            auto cat = stat["cat"].GetString();
        }
    }
};

struct MetricSeries
{
    MetricSeries()
    {

    }

    MetricSeries(const Document::GenericValue& stat)
    {
        name = stat["name"].GetString();
        auto& t_array_json = stat["t"].GetArray();
        for (auto& it : t_array_json)
        {
            auto v = it.GetFloat();
            if (v > max_t) max_t = v;
            else if (v < min_t) min_t = v;
            t_array.push_back(v);
        }
        auto& x_array_json = stat["x"].GetArray();
        for (auto& it : x_array_json)
        {
            auto v = it.GetFloat();
            if (v > max_x) max_x = v;
            else if (v < min_x) min_x = v;
            x_array.push_back(v);
        }
    }

    static ImPlotPoint getter(void* data, int idx)
    {
        auto* self = (MetricSeries*)data;
        return ImPlotPoint(self->t_array[idx], self->x_array[idx]);
    }

    float min_t = 0;
    float max_t = 1;
    float min_x = 0;
    float max_x = 1;

    std::string name;
    std::vector<float> t_array;
    std::vector< float > x_array;
};

struct DataStorage
{
    std::unordered_map<std::string, SpanSeries> span_storage;
    std::unordered_map< std::string, MetricSeries > metric_storage;
};

struct LightSpeedApp : public ci::app::App
{
    struct loading_info_t;

    void init( int argc, char **argv );
    void shutdown();

    bool load_file( const char *filename, bool last );
    void cancel_load_file();

    // Trace file loaded and viewing?
    bool is_trace_loaded();

    void render();
    void render_log();
    void render_console();
    void render_save_filename();
    void render_menu( const char *str_id );
    void render_menu_options();
    void render_color_picker();

    void update();

    void parse_cmdline( int argc, char **argv );

    void handle_hotkeys();

    void open_trace_dialog();

    enum state_t
    {
        State_Idle,
        State_Loading,
        State_CancelLoading
    };
    state_t get_state();
    void set_state( state_t state, const char *filename = nullptr );

    static int thread_func( void *data );

    static int load_trace_file( loading_info_t *loading_info, TraceEvents &trace_events, EventCallback trace_cb );
    static int load_etl_file( loading_info_t *loading_info, TraceEvents &trace_events, EventCallback trace_cb );
    static int load_i915_perf_file( loading_info_t *loading_info, TraceEvents &trace_events, EventCallback trace_cb );

    enum trace_type_t
    {
        trace_type_invalid,
        trace_type_trace,
        trace_type_etl,
        trace_type_i915_perf_trace,
    };

    struct loading_info_t
    {
        // State_Idle, Loading, Loaded, CancelLoading
        std::atomic< state_t > state = { State_Idle };

        // Which trace format are we loading
        trace_type_t type = trace_type_invalid;

        uint64_t tracestart = 0;
        uint64_t tracelen = 0;

        std::string filename;
        TraceWin *win = nullptr;
        std::thread thread;
        std::vector< std::string > inputfiles;

        bool last;
    };
    loading_info_t m_loading_info;

    struct save_info_t
    {
        char filename_buf[ PATH_MAX ] = { 0 };

        std::string title;
        std::string filename_new;
        std::string filename_orig;
        std::string errstr;

        std::function< bool( save_info_t &save_info ) > save_cb;
    };
    save_info_t m_saving_info;

    TraceWin *m_trace_win = nullptr;

    trace_type_t m_trace_type = trace_type_invalid;

    ImGuiTextFilter m_filter;
    size_t m_log_size = ( size_t )-1;
    std::vector< std::string > m_log;

    std::string m_colorpicker_event;
    colors_t m_colorpicker_color = 0;
    ColorPicker m_colorpicker;

    ImageBuf m_imagebuf;

    bool m_quit = false;
    bool m_focus_gpuvis_console = false;
    bool m_show_gpuvis_console = false;
    bool m_show_imgui_test_window = false;
    bool m_show_imgui_style_editor = false;
    bool m_show_imgui_metrics_editor = false;
    bool m_show_color_picker = false;
    bool m_show_scale_popup = false;
    bool m_show_help = false;

    std::string m_show_trace_info;

    DataStorage storage;

    void setup() override;
};

