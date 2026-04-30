#include <iostream>
#include <fstream>
#include <flags/flags.h>

#include <application.hpp>
#include <json-utils.hpp>

#include "states/menu-state.hpp"
#include "states/play-state.hpp"

int main(int argc, char **argv)
{

    flags::args args(argc, argv); // Parse the command line arguments
    // config_path is the path to the json file containing the application configuration
    // Default: "config/app.json"
    std::string config_path = args.get<std::string>("c", "config/app.jsonc");
    // run_for_frames is how many frames to run the application before automatically closing
    // This is useful for testing multiple configurations in a batch
    // Default: 0 where the application runs indefinitely until manually closed
    int run_for_frames = args.get<int>("f", 0);

    // Open the config file and exit if failed
    nlohmann::json app_config = our::loadJsonFile(config_path);
    if (app_config.is_null()) return -1;

    // Create the application
    our::Application app(app_config);

    // Register all the states of the project in the application
    app.registerState<Menustate>("menu");
    app.registerState<Playstate>("play");
    // Then choose the state to run based on the option "start-scene" in the config
    if (app_config.contains(std::string{"start-scene"}))
    {
        app.changeState(app_config["start-scene"].get<std::string>());
    }

    // Finally run the application
    // Here, the application loop will run till the terminatio condition is statisfied
    return app.run(run_for_frames);
}