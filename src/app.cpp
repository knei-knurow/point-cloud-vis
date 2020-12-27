#include "app.h"

App::App(std::vector<std::string>& args) {
  running_ = true;
  if (!parse_args(args)) {
    running_ = false;
  }
}

App::~App() = default;

int App::run() {
  Cloud cloud;

  while (running_) {
    // Grab a single cloud from the input...
    if (running_ && cloud_grabber_) {
      if (!cloud_grabber_->read(cloud)) {
        running_ = false;
      }
    }

    // ..and visualize it in the GUI
    // Save output files on user's request
    if (running_ && gui_) {
      if (!gui_->update(cloud)) {
        running_ = false;
      }
    }
  }

  return 0;
}

void App::print_help() {
  std::cout << "-----------------------------------------------------------\n"
            << "point cloud visualizations\n"
            << "-----------------------------------------------------------\n"
            << "Source: https://github.com/knei-knurow/point-cloud-vis\n"
            << "\n"
            << "Usage:\n"
            << "\tpcv [options]\n"
            << "\n"
            << "Options:\n"
            << "\tVisualization modes\n"
            << "\t[default]                      single point cloud"
            << "\t-cs  --cloud-series            point cloud series\n"
            << "\n"
            << "\tGeneral:\n"
            << "\t-h  --help                     display help\n"
            << "\t-o  --output-dir [dirname]     output dir\n"
            << "\t-s  --scenario [id]            scenario (default: 0)\n"
            << "\n"
            << "\tGUI options:\n"
            << "\t-H  --height [val]             window height (defualt: 1280)\n"
            << "\t-W  --width [val]              window width (defualt: 720)\n"
            << "\t-C  --colormap [id]            colormap (0, 1)\n"
            << "\t-M  --ptr-mode [id]            points display mode (0, 1, 2)\n"
            << "\t-B  --bold                     larger points\n"
            << "\t-S  --scale [scale]            scale (1mm -> 1px for scale = 1.0)\n"
            << "\n"
            << "Scenarios:\n"
            << "\t0    do nothing, just grab a cloud and visualize (default)\n"
            << "\t1    save each cloud as a part of cloud series\n"
            << "\t2    save each cloud as a new screenshot (extremely unoptimized)\n"
            << "\n"
            << "\tKeyboard Shortcuts:\n"
            << "\tT                 save cloud to a .txt file\n"
            << "\tS                 save screenshot\n"
            << "\tarrows            move cloud\n"
            << "\tmouse scroll      scale cloud\n"
            << "\tmouse middle      reset position, autoscale cloud\n"
            << "\tC                 switch colormap\n"
            << "\tM                 switch points display mode\n"
            << "";
}

bool App::check_arg(std::vector<std::string>& all_args,
                    const std::string& short_arg,
                    const std::string& long_arg) {
  auto it = std::find_if(
      all_args.begin(), all_args.end(),
      [short_arg, long_arg](const std::string& s) { return s == short_arg || s == long_arg; });

  if (it == all_args.end()) {
    return false;
  }

  all_args.erase(it);
  return true;
}

std::string App::get_arg_value(std::vector<std::string>& all_args,
                               const std::string& short_arg,
                               const std::string& long_arg,
                               const std::string& default_value) {
  auto it = std::find_if(
      all_args.begin(), all_args.end(),
      [short_arg, long_arg](const std::string& s) { return s == short_arg || s == long_arg; });

  std::string value = default_value;
  if (it == all_args.end()) {
  } else if (it + 1 == all_args.end()) {
    all_args.erase(it);
  } else {
    value = *(it + 1);
    all_args.erase(it, it + 2);
  }
  return value;
}

bool App::parse_args(std::vector<std::string>& args) {
  // Print help
  if (check_arg(args, "-h", "--help")) {
    print_help();
    return false;
  }

  bool cloud_series = check_arg(args, "-cs", "--cloud-series");

  // Scenario
  std::string scenario_val =
      get_arg_value(args, "-s", "--scenario", std::to_string(int(ScenarioType::IDLE)));
  ScenarioType scenario_type;
  if (scenario_val == std::to_string(int(ScenarioType::IDLE))) {
    scenario_type = ScenarioType::IDLE;
  } else if (scenario_val == std::to_string(int(ScenarioType::RECORD_SERIES))) {
    scenario_type = ScenarioType::RECORD_SERIES;
  } else if (scenario_val == std::to_string(int(ScenarioType::SCREENSHOT_SERIES))) {
    scenario_type = ScenarioType::SCREENSHOT_SERIES;
  } else {
    std::cerr << "error: invalid scenario id" << std::endl;
    return false;
  }

  // Initialize the cloud grabber
  if (!cloud_series) {
    cloud_grabber_ = std::make_unique<SingleCloudGrabber>(0.2);
    if (!cloud_grabber_->is_ok()) {
      cloud_grabber_.reset(nullptr);
    }
  } else {
    cloud_grabber_ = std::make_unique<CloudSeriesGrabber>();
    if (!cloud_grabber_->is_ok()) {
      cloud_grabber_.reset(nullptr);
    }
  }

  if (!cloud_grabber_ && !gui_ && !scenario_) {
    print_help();
    return false;
  }

  // Initialize the GUI
  GUISettings sfml_settings;

  unsigned colormap_temp = -1, display_mode_temp = -1;

  std::stringstream(get_arg_value(args, "-W", "--width")) >> sfml_settings.width;
  std::stringstream(get_arg_value(args, "-H", "--height")) >> sfml_settings.height;

  if (bool(std::stringstream(get_arg_value(args, "-C", "--colormap")) >> colormap_temp))
    sfml_settings.colormap =
        static_cast<GUISettings::Colormap>(colormap_temp % GUISettings::Colormap::COLORMAP_COUNT);

  if (bool(std::stringstream(get_arg_value(args, "-M", "--ptr-mode")) >> display_mode_temp))
    sfml_settings.pts_display_mode = static_cast<GUISettings::PtsDispayMode>(
        display_mode_temp % GUISettings::PtsDispayMode::PTS_DISPLAY_MODE_COUNT);

  if (bool(std::stringstream(get_arg_value(args, "-S", "--scale")) >> sfml_settings.scale))
    sfml_settings.autoscale = false;

  if (check_arg(args, "-B", "--bold"))
    sfml_settings.bold_mode = true;

  gui_ = std::make_unique<GUI>(sfml_settings);

  // Initialize the scenario
  if (scenario_type == ScenarioType::RECORD_SERIES) {
    scenario_ = std::make_unique<RecordSeriesScenario>();
  } else if (scenario_type == ScenarioType::SCREENSHOT_SERIES) {
    std::function<bool(void)> fn_ptr = std::bind(&GUI::save_screenshot, gui_.get());
    scenario_ = std::make_unique<ScreenshotSeriesScenario>(fn_ptr);
  }

  if (!args.empty()) {
    std::cerr << "warning: unused command line arguments: ";
    for (const auto& unused_arg : args) {
      std::cerr << "\"" << unused_arg << "\" ";
    }
    std::cerr << std::endl;
  }

  return true;
}
