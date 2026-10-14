#include "app/autotest.h"

#include <sstream>

namespace uv {

std::vector<AutotestStep> parse_autotest(const std::string &text, std::vector<std::string> &errors) {
    static const char *kVerbs[] = {"frames", "wait", "key", "text", "expect", "screenshot", "window_screenshot",
                                   "window_size", "window_state", "log", "dump", "teleport", "fight", "quit"};
    std::vector<AutotestStep> steps;
    std::istringstream in(text);
    std::string line;
    int number = 0;
    while (std::getline(in, line)) {
        ++number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        std::istringstream words(line);
        AutotestStep step;
        step.line = number;
        if (!(words >> step.verb)) continue;
        bool known = false;
        for (const char *v : kVerbs) known = known || step.verb == v;
        if (!known) {
            errors.push_back("line " + std::to_string(number) + ": unknown command '" + step.verb + "'");
            continue;
        }
        if (step.verb == "text" || step.verb == "log" || step.verb == "screenshot" || step.verb == "window_screenshot") {
            // The rest of the line, verbatim (one leading space dropped).
            std::string rest;
            std::getline(words, rest);
            if (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);
            step.args.push_back(rest);
        } else if (step.verb == "expect") {
            std::string name, value;
            words >> name;
            std::getline(words, value);
            while (!value.empty() && value.front() == ' ') value.erase(0, 1);
            step.args = {name, value};
        } else {
            std::string word;
            while (words >> word) step.args.push_back(word);
        }
        const bool needs_arg = step.verb != "quit" && step.verb != "dump";
        if (needs_arg && (step.args.empty() || step.args[0].empty())) {
            errors.push_back("line " + std::to_string(number) + ": '" + step.verb + "' needs an argument");
            continue;
        }
        if (step.verb == "window_size" && step.args.size() != 2) {
            errors.push_back("line " + std::to_string(number) + ": 'window_size' needs a width and a height");
            continue;
        }
        if (step.verb == "window_state" && step.args[0] != "maximize" && step.args[0] != "restore") {
            errors.push_back("line " + std::to_string(number) + ": 'window_state' is maximize or restore");
            continue;
        }
        steps.push_back(step);
    }
    return steps;
}

} // namespace uv
