#pragma once

#include <json/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

namespace our
{
    // Remove trailing commas from JSON/JSONC text to make it valid JSON
    inline std::string removeTrailingCommas(std::string text)
    {
        std::string cleaned;
        cleaned.reserve(text.size());
        bool inString = false, escaping = false;
        for (size_t i = 0; i < text.size(); i++)
        {
            char c = text[i];
            if (escaping) { escaping = false; cleaned.push_back(c); continue; }
            if (c == '\\') { escaping = true; cleaned.push_back(c); continue; }
            if (c == '"') { inString = !inString; cleaned.push_back(c); continue; }
            if (!inString && c == ',')
            {
                size_t n = i + 1;
                while (n < text.size() && std::isspace(static_cast<unsigned char>(text[n]))) n++;
                if (n < text.size() && (text[n] == '}' || text[n] == ']')) continue;
            }
            cleaned.push_back(c);
        }
        return cleaned;
    }

    // Load a JSON/JSONC file, strip trailing commas, parse
    inline nlohmann::json loadJsonFile(const std::string &path)
    {
        std::ifstream file_in(path);
        if (!file_in)
        {
            std::cerr << "[JsonUtils] Couldn't open: " << path << std::endl;
            return nlohmann::json::object();
        }
        std::stringstream buf;
        buf << file_in.rdbuf();
        return nlohmann::json::parse(removeTrailingCommas(buf.str()), nullptr, true, true);
    }
}
