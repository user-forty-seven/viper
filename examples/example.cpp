/*
In the below example, we try to generate a command line program
which has a structure like this:
 
    example
    ├── [flags]
    │   ├── -v, --verbose
    │   └── -c, --config <path>
    │
    ├── config
    │   ├── show
    │   ├── set <key> <value>
    │   └── reset
    │
    ├── output
    │   ├── capture <name>
    │   │   ├── -f, --format <png|jpg|webp>   (required)
    │   │   └── -q, --quality <1-100>
    │   └── crop <x> <y> [w] [h]
    │
    ├── region
    │   ├── select <x> <y> <w> <h>
    │   └── clear
    │
    ├── window
    │   ├── list [args...]
    │   └── focus <name>
    │
    ├── server
    │   ├── start
    │   │   └── -p, --port <1-65535> (required)
    │   └── stop
    │
    └── tag <tags...>

 */

#include "../viper.hpp"
#include <iostream>

using namespace viper;

static bool g_verbose = false;
static std::string g_configPath;
static std::string g_format;
static std::string g_quality;
static std::string g_port;
static bool g_force = false;

int main(int argc, char** argv) {
    Command root{ "example", "[flags...] <command> [args...]", "an example for viper" };

    // global persistent flags on root
    root.addFlagPersistent("verbose", 'v', g_verbose);
    root.addFlagPersistent("config", 'c', g_configPath, "", /* validator */ NonEmpty());

    // The config command group
    auto config = root.addCmd("config", "", "manage configuration");

    config->addCmd("show", "", "show current configuration",
        []() {
            std::cout << "config show\n";
        });

    config->addCmd("set", "<key> <value>", "set a configuration value",
        [](std::vector<std::string> args) {
            std::cout << "set " << args[0] << " = " << args[1] << '\n';
        },
        ExactArgs(2));

    config->addCmd("reset", "", "reset configuration to defaults",
        []() {
            std::cout << "config reset\n";
        },
        NoArgs());

    // The output command group
    auto output = root.addCmd("output", "", "manage snip output");

    auto outputCapture = output->addCmd("capture", "<name>", "capture the current output",
        [](std::vector<std::string> args) {
            std::cout << "capturing output: " << args[0]
                       << " format=" << g_format << " quality=" << g_quality << '\n';
        },
        ExactArgs(1));

    outputCapture->addFlagRequired("format", 'f', g_format, "",
        OneOf({"png", "jpg", "webp"}));

    outputCapture->addFlag("quality", 'q', g_quality, "",
        AllOf({ Int(), IntRange(1, 100) }));

    output->addCmd("crop", "<x> <y> [w] [h]", "crop the output region",
        [](std::vector<std::string> args) {
            std::cout << "cropping with " << args.size() << " coordinate(s)\n";
        },
        RangeArgs(2, 4));

    // The region command group
    auto region = root.addCmd("region", "", "manage custom regions");

    region->addCmd("select", "<x> <y> <w> <h>", "select a region by coordinates",
        [](std::vector<std::string> args) {
            std::cout << "region select: " << args[0] << "," << args[1]
                       << " " << args[2] << "x" << args[3] << '\n';
        },
        ExactArgs(4));

    region->addCmd("clear", "", "clear the saved region",
        []() {
            std::cout << "region cleared\n";
        },
        NoArgs());

    // The window command group
    auto window = root.addCmd("window", "", "manage window snipping");

    window->addCmd("list", "", "list available windows",
        [](std::vector<std::string> args) {
            std::cout << "listing windows (filter args: " << args.size() << ")\n";
        },
        AnyArgs());

    window->addCmd("focus", "<name>", "snip a specific window by name",
        [](std::vector<std::string> args) {
            std::cout << "focusing window: " << args[0] << '\n';
        },
        ExactArgs(1));

    // The server command group
    auto server = root.addCmd("server", "", "run as a background server");

    auto serverStart = server->addCmd("start", "", "start the server",
        []() {
            std::cout << "server starting on port " << g_port
                       << " verbose=" << g_verbose << '\n';
        });

    serverStart->addFlagRequired("port", 'p', g_port, "",
        AllOf({ Int(), IntRange(1, 65535) }));

    server->addCmd("stop", "", "stop the server",
        []() {
            std::cout << "server stopping\n";
        },
        NoArgs());

    // The tag command group
    root.addCmd("tag", "<tags...>", "tag the last capture",
        [](std::vector<std::string> args) {
            std::cout << "tagging with " << args.size() << " tag(s): ";
            for (auto& t : args) std::cout << t << " ";
            std::cout << '\n';
        },
        MinArgs(1));

    return execute(&root, argc, argv);
}

