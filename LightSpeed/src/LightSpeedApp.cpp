#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "../../blocks/rapidjson/rapidjson.h"
#include "../../blocks/rapidjson/document.h"

#include "AssetManager.h"
#include "MiniConfigImgui.h"
#include "implot/implot.h"
#include "implot/implot_internal.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace rapidjson;

float global_min_t = 0;
float global_max_t = 1;

struct Span
{
    string name;
    string label;
    float start = 0;
    float end = 0;
};

struct SpanSeries
{
    string name;
    vector<Span> span_array;

    SpanSeries()
    {
    }

    SpanSeries(const Document::GenericValue& stat)
    {
        name = stat["name"].GetString();
        if (stat.HasMember("pid"))
        {
            // Chrome Event Format
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
        else if (stat.HasMember("regions"))
        {
            // XXX Format
            const auto& regions = stat["regions"].GetArray();
            for (const auto& region : regions)
            {
                Span span;
                span.name = region["name"].GetString();
                span.label = region["label"].GetString();
                span.start = region["start"].GetFloat();
                span.end = region["end"].GetFloat();
                span_array.emplace_back(span);
            }
        }
    }

    // Plots axis-aligned, filled rectangles. Every two consecutive points defines opposite corners of a single rectangle.
    static ImPlotPoint getter(void* data, int idx)
    {
        auto* self = (SpanSeries*)data;
        int span_idx = idx / 2;
        int tag = idx % 2;
        if (tag == 0)
        {
            float start_t = self->span_array[span_idx].start/*  / TIME_UNIT_SCALE */;
            return ImPlotPoint(start_t, 0);
        }
        else
        {
            float end_t = self->span_array[span_idx].end/*  / TIME_UNIT_SCALE */;
            return ImPlotPoint(end_t, 10);
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
            if (v > global_max_t) global_max_t = v;
            else if (v < global_min_t) global_min_t = v;
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

    float min_x = 0;
    float max_x = 1;

    string name;
    vector<float> t_array;
    vector<float> x_array;
};

struct DataStorage
{
    unordered_map<string, SpanSeries> span_storage;
    unordered_map<string, MetricSeries> metric_storage;
};

struct LightSpeedApp : public App
{
    DataStorage storage;
    ImPlotContext* implotCtx = nullptr;

    void setup() override
    {
        log::makeLogger<log::LoggerFileRotating>(fs::path(), "app.%Y.%m.%d.log");
        createConfigImgui();
        implotCtx = ImPlot::CreateContext();

        {
            Document json;
            const auto& str = am::str(DATA_FILENAME);
            if (!str.empty())
            {
                json.Parse(str.c_str());

                if (json.HasMember("stats"))
                {
                    auto& stats_array = json["stats"].GetArray();
                    for (auto& item : stats_array)
                    {
                        MetricSeries series(item);
                        storage.metric_storage[series.name] = series;
                    }
                }

                if (json.HasMember("regionTypes"))
                {
                    auto& span_series_array = json["regionTypes"].GetArray();
                    for (auto& item : span_series_array)
                    {
                        SpanSeries series(item);
                        storage.span_storage[series.name] = series;
                    }
                }
                
                if (json.HasMember("traceEvents"))
                {
                    auto& event_array = json["traceEvents"].GetArray();
                    for (auto& item : event_array)
                    {
                        SpanSeries series(item);
                        storage.span_storage[series.name] = series;
                    }
                }
            }
        }

        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE) quit();
        });

        getWindow()->getSignalResize().connect([&] {
            APP_WIDTH = getWindowWidth();
            APP_HEIGHT = getWindowHeight();
        });

        getSignalUpdate().connect([&] {
            bool open = false;
            ImPlot::ShowDemoWindow(&open);

            if (ImGui::Begin("Plot"))
            {
                for (const auto& kv : storage.span_storage)
                {
                    auto& series = kv.second;
                    if (!series.name._Starts_with("AC") && !series.name._Starts_with("3D"))
                        continue;

                    ImPlot::SetNextPlotTicksX(global_min_t, global_max_t, PANEL_TICK_T);
                    ImPlot::SetNextPlotLimitsX(global_min_t, global_max_t, ImGuiCond_Always);
                    ImPlot::SetNextPlotTicksY(0, 10, 2);

                    if (ImPlot::BeginPlot(series.name.c_str(), NULL, NULL, ImVec2(-1, PANEL_HEIGHT), ImPlotFlags_Default | ImPlotFlags_NoChild))
                    {
                        //ImPlot::PushStyleColor(ImPlotCol_Line, items[i].Col);
                        ImPlot::PlotRects(series.name.c_str(), SpanSeries::getter, (void*)&series, series.span_array.size() * 2);
                        for (const auto& span : series.span_array)
                        {
                            ImPlot::PlotText(span.name.c_str(), span.start/*  / TIME_UNIT_SCALE */, 0, false, ImVec2(0, -PANEL_HEIGHT/2));
                        }
                        //ImPlot::PopStyleColor();
                        ImPlot::EndPlot();
                    }
                }

                for (const auto& kv : storage.metric_storage)
                {
                    auto& series = kv.second;

                    ImPlot::SetNextPlotTicksX(global_min_t, global_max_t, PANEL_TICK_T);
                    ImPlot::SetNextPlotLimitsX(global_min_t, global_max_t, ImGuiCond_Always);
                    ImPlot::SetNextPlotTicksY(series.min_x, series.max_x, PANEL_TICK_X);
                    ImPlot::SetNextPlotLimitsY(series.min_x, series.max_x, ImGuiCond_Always);

                    if (ImPlot::BeginPlot(series.name.c_str(), NULL, NULL, ImVec2(-1, PANEL_HEIGHT), ImPlotFlags_Default | ImPlotFlags_NoChild))
                    {
                        //ImPlot::PushStyleColor(ImPlotCol_Line, items[i].Col);
                        ImPlot::PlotLine(series.name.c_str(), MetricSeries::getter, (void*)&series, series.t_array.size());
                        //ImPlot::PopStyleColor();
                        ImPlot::EndPlot();
                    }
                }
            }
            ImGui::End();

        });

        getSignalCleanup().connect([&] {
            ImPlot::DestroyContext(implotCtx);
            writeConfig();
        });

        getWindow()->getSignalDraw().connect([&] {
            gl::clear();
        });
    }
};

CINDER_APP(LightSpeedApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
})
