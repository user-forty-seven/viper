#include "../viper.hpp"
#include <iostream>

using namespace viper;

FlagValidator expect69() 
{
    return [](const std::string& v) -> std::optional<std::string>
    {
        if (v.empty())
        {
            return "expected 69, got empty string";
        }
        if (v != "69")
        {
            return "expected 69, got " + v;
        }
        return std::nullopt;
    };
}

int main(int argc, char** argv)
{
    auto rootCmd = 
        Command { 
            "root", 
            "--funny <input>", 
            "example of a custom flag validator",
            // empty lambda so it doesn't error since we dont have any commands
            [] (const std::vector<std::string>& args)
            {
                return 0;
            }
        };

    std::string input;
    rootCmd.addFlag("funny", 'f', input, "takes in a funny number", expect69());
    return execute(&rootCmd, argc, argv);
}

