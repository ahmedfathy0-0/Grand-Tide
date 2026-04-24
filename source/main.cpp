#include <iostream>
#include <fstream>
#include <sstream>
#include <cctype>
#include <flags/flags.h>
#include <json/json.hpp>

#include <application.hpp>

#include "states/menu-state.hpp"
#include "states/play-state.hpp"

static std::string removeTrailingCommas(std::string text)
{
    std::string cleaned;
    cleaned.reserve(text.size());

    bool inString = false;
    bool escaping = false;

    for (size_t i = 0; i < text.size(); i++)
    {
        char current = text[i];

        if (escaping)
        {
            escaping = false;
            cleaned.push_back(current);
            continue;
        }

        if (current == '\\')
        {
            escaping = true;
            cleaned.push_back(current);
            continue;
        }

        if (current == '"')
        {
            inString = !inString;
            cleaned.push_back(current);
            continue;
        }

        if (!inString && current == ',')
        {
            size_t next = i + 1;
            while (next < text.size() && std::isspace(static_cast<unsigned char>(text[next])))
            {
                next++;
            }
            if (next < text.size() && (text[next] == '}' || text[next] == ']'))
            {
                continue;
            }
        }

        cleaned.push_back(current);
    }

    return cleaned;
}

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
    std::ifstream file_in(config_path);
    if (!file_in)
    {
        std::cerr << "Couldn't open file: " << config_path << std::endl;
        return -1;
    }
    std::stringstream buffer;
    buffer << file_in.rdbuf();
    std::string config_content = removeTrailingCommas(buffer.str());

    // Read the file into a json object then close the file
    nlohmann::json app_config = nlohmann::json::parse(config_content, nullptr, true, true);
    file_in.close();

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