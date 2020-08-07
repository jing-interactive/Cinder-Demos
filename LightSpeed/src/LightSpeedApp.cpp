#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/Json.h"

#include "AssetManager.h"
#include "MiniConfigImgui.h"
#include "implot/implot.h"

using namespace ci;
using namespace ci::app;
using namespace std;

struct Span
{

};

struct SpanSeries
{
    string name;
    vector<Span> span_array;
};

struct MetricSeries
{
    MetricSeries()
    {

    }

    MetricSeries(const JsonTree& stat)
    {
        name = stat["name"].getValue();
        auto& t_array_json = stat["t"].getChildren();
        for (auto& it : t_array_json)
        {
            auto v = it.getValue<float>();
            if (v > max_t) max_t = v;
            else if (v < min_t) min_t = v;
            t_array.push_back(v);
        }
        auto& x_array_json = stat["x"].getChildren();
        for (auto& it : x_array_json)
        {
            auto v = it.getValue<float>();
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
    JsonTree json;
    DataStorage storage;

    void setup() override
    {
        log::makeLogger<log::LoggerFileRotating>(fs::path(), "app.%Y.%m.%d.log");
        createConfigImgui();

        {
            json = JsonTree(loadAsset(DATA_FILENAME));
            auto& stats_array = json["stats"].getChildren();
            for (auto& stat : stats_array)
            {
                MetricSeries series(stat);
                storage.metric_storage[series.name] = series;
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
                for (const auto& kv : storage.metric_storage)
                {
                    auto& series = kv.second;

                    ImPlot::SetNextPlotLimits(series.min_t, series.max_t, series.min_x, series.max_x, ImGuiCond_Always);
                    ImPlot::SetNextPlotTicksX(series.min_t, series.max_t, PANEL_TICK_T);
                    ImPlot::SetNextPlotTicksY(series.min_x, series.max_x, PANEL_TICK_X);

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

        getSignalCleanup().connect([&] { writeConfig(); });

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
