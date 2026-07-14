/* viper.hpp - v0.5.0 */
/* 
 * A simple and lean command line parser
 * inspired from [cobra](https://github.com/spf13/cobra)
 */

/*
 * 14 July 2026: v0.5.0 - initial release
 */

// clang-format off
#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace viper {

enum class ConfigErrorType {
    DUPLICATE_FLAG,
    DUPLICATE_CHILD,
    NONE
};

struct ConfigError {
    ConfigErrorType type = ConfigErrorType::NONE;
    std::string msg;

    explicit operator bool() const { return type != ConfigErrorType::NONE; }
};

std::vector<ConfigError> g_ConfigErrors;

/*===============================================*/
/*================= MODELS ===================== */
/*===============================================*/
using FlagValidator = std::function<std::optional<std::string>(const std::string& value)>;
inline FlagValidator NonEmpty();
inline FlagValidator Int();
inline FlagValidator IntRange(int min, int max);
inline FlagValidator OneOf(const std::vector<std::string>& choices);
inline FlagValidator AnyOf(std::vector<FlagValidator> validators) ;
inline FlagValidator AllOf(std::vector<FlagValidator> validators);
struct Flag {
    std::string longName;
    char shortName = '\0';
    std::string desc;
    bool takesValue = false;
    bool persistent = false;
    bool required = false;
    bool wasSet = false;

    std::variant<
        std::monostate,
        std::reference_wrapper<std::string>,
        std::reference_wrapper<bool>
    > value;
    FlagValidator validator;
};

//
// Some provided validators for flags
//
inline FlagValidator NonEmpty() {
    return [](const std::string& v) -> std::optional<std::string> {
        if (v.empty()) return "must not be empty";
        return std::nullopt;
    };
}

inline FlagValidator Int() {
    return [](const std::string& v) -> std::optional<std::string> {
        if (v.empty()) return "must be an integer, got empty string";
        std::size_t pos = 0;
        try {
            std::stoll(v, &pos);
        } catch (...) {
            return "must be an integer (got \"" + v + "\")";
        }
        if (pos != v.size()) return "must be an integer (got \"" + v + "\")";
        return std::nullopt;
    };
}

inline FlagValidator IntRange(const int min, const int max) {
    return [min, max](const std::string& v) -> std::optional<std::string> {
        const std::string msg = "must be an integer between " + std::to_string(min) + " and " + std::to_string(max);
        if (v.empty()) {
            return msg + ", got empty string";
        }
        std::size_t pos = 0;
        long long result;
        try {
            result = std::stoll(v, &pos);
        } catch (...) {
            return msg + " (got \"" + v + "\")";
        }
        if (pos != v.size() || result < min || result > max) {
            return msg + " (got \"" + v + "\")";
        }

        return std::nullopt;
    };
}

inline FlagValidator OneOf(const std::vector<std::string>& choices) {
    return [choices](const std::string& v) -> std::optional<std::string> {
        bool has = false;
        std::string choiceStr;
        for (const auto& c : choices) {
            if (c == v) {
                has = true;
            }
            choiceStr += ' ';
            choiceStr += c;
        }
        if (!has) {
            return "must of one of:" + choiceStr;
        }
        return std::nullopt;
    };
}

inline FlagValidator AllOf(std::vector<FlagValidator> validators) {
    return [validators = std::move(validators)](const std::string& v) -> std::optional<std::string> {
        for (auto& validator : validators) {
            if (auto err = validator(v)) {
                return err;
            }
        }
        return std::nullopt;
    };
}

inline FlagValidator AnyOf(std::vector<FlagValidator> validators) {
    return [validators = std::move(validators)](const std::string& v) -> std::optional<std::string> {
        std::string last;
        for (auto& validator : validators) {
            if (auto err = validator(v)) {
                last = *err;
            }
        }
        if (!last.empty()) {
            return last;
        }
        return std::nullopt;
    };
}

//
// Just a simple little thing to wrap the main
// run function, for the convenience of the user
// changes the following:
//  <void()>, <void<vector<string>>, <int()>, <int(vector<string>)
// into:
//  <int(vector<string>)>
//
template<class> inline constexpr bool always_false_v = false;

template<typename F>
std::function<int(const std::vector<std::string>&)> wrapRun(F&& f) {
    using DecayF = std::decay_t<F>;
    if constexpr (std::is_invocable_r_v<int, DecayF, const std::vector<std::string>&>) {
        return std::function<int(const std::vector<std::string>&)>(std::forward<F>(f));
    }
    else if constexpr (std::is_invocable_v<DecayF, const std::vector<std::string>&>) {
        // no code provided so we assume success
        return [f = std::forward<F>(f)](const std::vector<std::string>& args) mutable -> int {
            f(args);
            return 0;
        };
    }
    else if constexpr (std::is_invocable_r_v<int, DecayF>) {
        // the user doesn't want args
        return [f = std::forward<F>(f)](const std::vector<std::string>&) mutable -> int {
            return f();
        };
    }
    else if constexpr (std::is_invocable_v<DecayF>) {
        // the user doesn't want args and we assume success
        return [f = std::forward<F>(f)](const std::vector<std::string>&) mutable -> int {
            f();
            return 0;
        };
    }
    else {
        static_assert(always_false_v<F>, "run callback must be callable(either as () or (const std::vector<std::string>&), and must return either void or int");
    }
}
//
// Returns error message if count is invalid, else return std::nullopt
//
using ArgValidator = std::function<std::optional<std::string>(std::size_t count)>;
inline ArgValidator ExactArgs(std::size_t n);
inline ArgValidator MinArgs(std::size_t min);
inline ArgValidator MaxArgs(std::size_t max);
inline ArgValidator RangeArgs(std::size_t min, std::size_t max);
inline ArgValidator AnyArgs();
inline ArgValidator NoArgs();
inline ArgValidator AllOf(std::vector<ArgValidator> validators);
inline ArgValidator AnyOf(std::vector<ArgValidator> validators);
struct Command {
    std::string name;
    std::string use;
    std::string desc;
    std::function<int(const std::vector<std::string>&)> run = nullptr;
    ArgValidator args = AnyArgs();

    Command* parent = nullptr;
    std::vector<std::unique_ptr<Command>> children = {};
    std::vector<std::unique_ptr<Flag>> flags = {};

    bool helpRequested = false;

    template<typename F>
    Command* addCmd(std::string cmdName, std::string cmdUsage, std::string cmdDesc,
           F&& cmdRun, const ArgValidator& cmdArgs = AnyArgs()) {
        if (childDuplicate(cmdName)) {
            g_ConfigErrors.push_back(ConfigError{ConfigErrorType::DUPLICATE_CHILD,
            "redefinition of child: " + cmdName});
        }
        auto c = std::make_unique<Command>();
        c->name = std::move(cmdName);
        c->use = std::move(cmdUsage);
        c->desc = std::move(cmdDesc);
        c->run = wrapRun(std::forward<F>(cmdRun));
        c->args = cmdArgs;
        c->parent = this;
        children.push_back(std::move(c));
        return children.back().get();
    }
    //
    // overload for cmdRun == nullptr
    //
    Command* addCmd(std::string cmdName, std::string cmdUsage, std::string cmdDesc, const ArgValidator& cmdArgs = AnyArgs()) {
        if (childDuplicate(cmdName)) {
            g_ConfigErrors.push_back(ConfigError{ConfigErrorType::DUPLICATE_CHILD,
            "redefinition of child: " + cmdName});
        }
        auto c = std::make_unique<Command>();
        c->name = std::move(cmdName);
        c->use = std::move(cmdUsage);
        c->desc = std::move(cmdDesc);
        c->args = cmdArgs;
        c->parent = this;
        children.push_back(std::move(c));
        return children.back().get();
    }

    template<
        typename T,
        typename = std::enable_if<
            std::is_same_v<T, std::string> ||
            std::is_same_v<T, bool>
        >
    >
    Flag* addFlag(const std::string &flagName, const char shortName,
        T& var, const std::string& cmdDesc = "",
        FlagValidator flagValidator = nullptr,
        const bool isPersistent = false,
        const bool isRequired = false
        ) {
        if (flagDuplicate(flagName)) {
            g_ConfigErrors.push_back(ConfigError{ConfigErrorType::DUPLICATE_CHILD,
            "redefinition of flag: " + flagName});
        }
        auto f = std::make_unique<Flag>();
        f->takesValue = std::is_same_v<T, std::string>;
        f->persistent = isPersistent;
        f->required = isRequired;
        f->value = var;
        f->longName = flagName;
        f->shortName = shortName;
        f->desc = cmdDesc;
        f->validator = std::move(flagValidator);
        flags.push_back(std::move(f));
        return flags.back().get();
    }
    template<
        typename T,
        typename = std::enable_if<
            std::is_same_v<T, std::string> ||
            std::is_same_v<T, bool>
        >
    >
    Flag* addFlagPersistent(const std::string &flagName, const char shortName, T& var, const std::string& cmdDesc = "", FlagValidator flagValidator = nullptr) {
        return addFlag(flagName, shortName, var, cmdDesc, flagValidator, true);
    }
    template<
        typename T,
        typename = std::enable_if<
            std::is_same_v<T, std::string> ||
            std::is_same_v<T, bool>
        >
    >
    Flag* addFlagRequired(const std::string &flagName, const char shortName, T& var, const std::string& cmdDesc = "", FlagValidator flagValidator = nullptr) {
        return addFlag(flagName, shortName, var, cmdDesc, flagValidator, false, true);
    }
    template<
        typename T,
        typename = std::enable_if<
            std::is_same_v<T, std::string> ||
            std::is_same_v<T, bool>
        >
    >
    Flag* addFlagPersistentRequired(const std::string &flagName, const char shortName, T& var, const std::string& cmdDesc = "", FlagValidator flagValidator = nullptr) {
        return addFlag(flagName, shortName, var, cmdDesc, flagValidator, true, true);
    }

    Command* childLookup(const std::string& childName) const {
        for (auto& c : children) {
            if (c->name == childName) return c.get();
        }
        return nullptr;
    }

    bool childDuplicate(const std::string& childName) const {
        return childLookup(childName) != nullptr;
    }

    Flag* flagLookup(const std::string& flagName) const {
        for (auto& f : flags) {
            if (f->longName == flagName || (flagName.length() == 1 && flagName[0] == f->shortName)) return f.get();
        }
        for (auto c = this->parent; c; c = c->parent) {
            for (auto& pf : c->flags) {
                if (pf->persistent && (pf->longName == flagName || (flagName.length() == 1 && flagName[0] == pf->shortName))) return pf.get();
            }
        }
        return nullptr;
    }

    bool flagDuplicate(const std::string& flagName) const {
        return flagLookup(flagName) != nullptr;
    }

    std::string fullName() const {
        std::vector<std::string> parts;
        for (auto c = this; c; c = c->parent) {
            parts.push_back(c->name);
        }
        std::reverse(parts.begin(), parts.end());
        std::string result;

        for (auto& p : parts) {
            result += p;
            result += ' ';
        }
        return result;
    }
};

//
// Some provided validators for args
//
inline ArgValidator ExactArgs(std::size_t n) {
    return [n](std::size_t count) -> std::optional<std::string> {
        if (count != n) {
            return "expected exactly " + std::to_string(n) +
                " arg(s), but got " + std::to_string(count);
        }
        return std::nullopt;
    };
}
inline ArgValidator MinArgs(std::size_t min) {
    return [min](std::size_t count) -> std::optional<std::string> {
        if (count < min) {
            return "expected at least " + std::to_string(min) +
                " arg(s), but got " + std::to_string(count);
        }
        return std::nullopt;
    };
}
inline ArgValidator MaxArgs(std::size_t max) {
    return [max](std::size_t count) -> std::optional<std::string> {
        if (count > max) {
            return "expected at most " + std::to_string(max) +
                " arg(s), but got " + std::to_string(count);
        }
        return std::nullopt;
    };
}
inline ArgValidator RangeArgs(std::size_t min, std::size_t max) {
    return [min, max](std::size_t count) -> std::optional<std::string> {
        if (count < min || count > max) {
            return "expected at least " + std::to_string(min) +
                " and at most " + std::to_string(max) +
                " arg(s), but got " + std::to_string(count);
        }
        return std::nullopt;
    };
}
inline ArgValidator AnyArgs() {
    return [](std::size_t count) -> std::optional<std::string> {
        (void)count;
        return std::nullopt;
    };
}
inline ArgValidator NoArgs() {
    return [](std::size_t count) -> std::optional<std::string> {
        if (count != 0) {
            return "expected no args, but got " + std::to_string(count);
        }
        return std::nullopt;
    };
}

inline ArgValidator AllOf(std::vector<ArgValidator> validators) {
    return [validators = std::move(validators)](std::size_t count) -> std::optional<std::string> {
        for (auto& validator : validators) {
            if (auto err = validator(count)) {
                return err;
            }
        }
        return std::nullopt;
    };
}

inline ArgValidator AnyOf(std::vector<ArgValidator> validators) {
    return [validators = std::move(validators)](std::size_t count) -> std::optional<std::string> {
        std::string last = "";
        for (auto& validator : validators) {
            if (auto err = validator(count)) {
                last = *err;
            }
        }
        if (!last.empty()) {
            return last;
        }
        return std::nullopt;
    };
}

/*===============================================*/
/*+++++++++++++++ TOKENIZER +++++++++++++++++++++*/
/*===============================================*/
enum class TokenType {
    POSITIONAL,
    SHORT_FLAG,
    LONG_FLAG,
    TERMINATOR
};

struct Token {
    std::string content;
    std::string raw;
    TokenType type;
};

inline std::vector<Token> tokenize(int argc, char** argv) {
    // TODO: make use of program name

    std::vector<Token> result;
    for (int i = 1; i < argc; i++) {
        if (std::string arg = argv[i]; arg.length() > 2 && arg[0] == '-' && arg[1] == '-') {
            result.push_back(Token{arg.substr(2), arg, TokenType::LONG_FLAG});
        }
        else if (arg.length() == 2 && arg == "--") {
            result.push_back(Token{"--", "--", TokenType::TERMINATOR});
        }
        else if (arg.length() > 1 && arg[0] == '-') {
            for (auto c : arg) {
                if (c == '-') continue;
                result.push_back(Token{std::string(1, c), arg, TokenType::SHORT_FLAG});
            }
        }
        else {
            result.push_back(Token{arg, arg, TokenType::POSITIONAL});
        }
    }
    return result;
}

/*===============================================*/
/*+++++++++++++++++ PARSING +++++++++++++++++++++*/
/*===============================================*/
enum class ParseErrorType {
    UNKNOWN_FLAG,
    MISSING_FLAG,
    MISSING_FLAG_VALUE,
    INVALID_FLAG_VALUE,
    INVALID_ARGS,
    NONE
};

struct ParseError {
    ParseErrorType type = ParseErrorType::NONE;
    std::string msg;

    explicit operator bool() const { return type != ParseErrorType::NONE; }
};

inline std::string helpMsg(Command* cmd);
inline void addDefaults(Command* cmd);

inline ParseError parse(Command*& current, const std::vector<Token>& tokenized, std::vector<std::string>& args) {
    std::size_t i = 0;
    bool terminated = false;
    while (i < tokenized.size()) {
        auto& token = tokenized[i];
        if (terminated) {
            if (token.type == TokenType::POSITIONAL) {
                args.push_back(token.content);
            }
            else if (token.type == TokenType::LONG_FLAG) {
                args.push_back(token.raw);
            }
            else if (token.type == TokenType::SHORT_FLAG) {
                args.push_back("-" + token.content);
            }
            else {
                args.push_back("--");
            }
            i++;
            continue;
        }
        if (token.type == TokenType::POSITIONAL) {
            if (const auto found = current->childLookup(token.content)) {
                current = found;
            } else {
                args.push_back(token.content);
            }
        }
        else if (token.type == TokenType::LONG_FLAG) {
            const auto found = current->flagLookup(token.content);
            if (!found) {
                return ParseError{ParseErrorType::UNKNOWN_FLAG,
                    "unknown flag: --" + token.content};
            }
            if (found->takesValue) {
                if (i + 1 >= tokenized.size()) {
                    return ParseError{ParseErrorType::MISSING_FLAG_VALUE,
                        "flag --" + token.content + " requires a value"};
                }
                if (tokenized[i + 1].type != TokenType::POSITIONAL) {
                    return ParseError{ParseErrorType::INVALID_FLAG_VALUE,
                      "flag --" + token.content + " requires a positional value, not another flag"};
                }
                auto& value = tokenized[++i].content;
                if (found->validator) {
                    if (const auto err = found->validator(value)) {
                        return ParseError{ParseErrorType::MISSING_FLAG_VALUE,
                        "flag --" + token.content + ": " + *err};
                    }
                }

                std::get<std::reference_wrapper<std::string>>(found->value).get() = value;
                found->wasSet = true;
            }
            else {
                std::get<std::reference_wrapper<bool>>(found->value).get() = true;
                found->wasSet = true;
            }
        }
        else if (token.type == TokenType::SHORT_FLAG) {
            const auto found = current->flagLookup(token.content);
            if (!found) {
                return ParseError{ParseErrorType::UNKNOWN_FLAG,
                    "unknown flag: -" + token.content};
            }
            if (found->takesValue) {
                if (i + 1 >= tokenized.size()) {
                    return ParseError{ParseErrorType::MISSING_FLAG_VALUE,
                        "flag -" + token.content + " requires a value"};
                }
                if (tokenized[i + 1].type != TokenType::POSITIONAL) {
                    return ParseError{ParseErrorType::MISSING_FLAG_VALUE,
                        "flag -" + token.content + " requires a positional value, not another flag"};
                }

                auto& value = tokenized[++i].content;
                if (found->validator) {
                    if (const auto err = found->validator(value)) {
                        return ParseError{ParseErrorType::MISSING_FLAG_VALUE,
                        "flag --" + token.content + ": " + *err};
                    }
                }

                std::get<std::reference_wrapper<std::string>>(found->value).get() = value;
                found->wasSet = true;
            }
            else {
                std::get<std::reference_wrapper<bool>>(found->value).get() = true;
                found->wasSet = true;
            }
        }
        else if (token.type == TokenType::TERMINATOR) {
            terminated = true;
        }
        i++;
    }
    return ParseError{};
}

inline std::optional<std::string> checkRequiredFlags(Command*& cmd) {
    for (auto& f : cmd->flags) {
        if (f->required && !f->wasSet) {
            return "missing required flag: --" + f->longName;
        }
    }
    // check for persistent flags which were not set
    for (auto c = cmd->parent; c; c = c->parent) {
        for (auto& f : c->flags) {
            if (f->persistent && f->required && !f->wasSet) {
                return "missing required flag: --" + f->longName;
            }
        }
    }
    return std::nullopt;
}

/*===============================================*/
/*++++++++++++++++ DISPATCHING ++++++++++++++++++*/
/*===============================================*/
using ErrorHandler = std::function<void(const Command& cmd, const std::string& message)>;
inline void defaultErrorHandler(const Command& cmd, const std::string& message) {
    std::cerr << "Error: " << message << '\n';
    std::cerr << "Usage: " << cmd.fullName() << cmd.use << '\n';
}

inline int execute(Command* root, int argc, char** argv, const ErrorHandler& errorHandler = defaultErrorHandler) {
    bool shouldReturnEarly = false;
    for (const auto& err : g_ConfigErrors) {
        if (err) {
            std::cerr << "Config error: " << err.msg << '\n';
            if (!shouldReturnEarly) {
                shouldReturnEarly = true;
            }
        }
    }
    if (shouldReturnEarly) {
        return 1;
    }
    g_ConfigErrors.clear();

    addDefaults(root);

    const auto& tokenized = tokenize(argc, argv);
    auto current = root;
    std::vector<std::string> args;

    if (const auto err = parse(current, tokenized, args)) {
        errorHandler(*current, err.msg);
        return 1;
    }

    for (auto c = current; c; c = c->parent) {
        if (c->helpRequested) {
            std::cout << helpMsg(c);
            return 0;
        }
    }

    if (const auto flagErr = checkRequiredFlags(current)) {
        errorHandler(*current, *flagErr);
        return 1;
    }

    if (!current->run) {
        errorHandler(*current, "Invalid action");
        return 1;
    }

    if (const auto argErr = current->args(args.size())) {
        errorHandler(*current, *argErr);
        return 1;
    }

    return current->run(args);
}

/*===============================================*/
/*++++++++++++++++++++ MISC +++++++++++++++++++++*/
/*===============================================*/
inline std::string helpMsg(Command* cmd) {
    // compute the length of the longest command
    std::size_t cmdWidth = 0;
    for (const auto& c : cmd->children) {
        if (c->name.size() > cmdWidth) cmdWidth = c->name.size();
    }
    // computer the length of the longest flags
    // -f, --flag
    // final would be 1(-) + 1(,) + 1(" ") + 2(--) + 1(flagShortName) + strlen(flagLongName) = 6 + strlen(flagLongName)
    std::size_t flagWidth = 0;
    for (const auto& f : cmd->flags) {
        if (f->longName.size() > flagWidth) flagWidth = f->longName.size();
    }
    flagWidth += 6;
    constexpr int spaceWidth = 2;

    std::ostringstream oss;
    oss << cmd->name << " - " << cmd->desc << "\n\n";
    oss << "Usage:" << '\n';
    oss << std::setw(spaceWidth) << ""
    << std::right << cmd->fullName() << cmd->use << "\n\n";
    if (!cmd->children.empty())
        oss << "Available Commands:" << '\n';
    for (const auto& c : cmd->children) {
        oss << std::right << std::setw(spaceWidth) << ""
        << std::left << std::setw(static_cast<int>(cmdWidth)) << c->name
        << std::setw(spaceWidth + 3) << ""
        << c->desc << '\n';
    }
    if (!cmd->children.empty())
        oss << '\n';
    if (!cmd->flags.empty())
        oss << "Flags:" << '\n';
    for (const auto& f : cmd->flags) {
        std::string flag = "-" + std::string(1, f->shortName) + ", --" + f->longName;
        oss << std::right << std::setw(spaceWidth) << ""
        << std::left << std::setw(static_cast<int>(flagWidth)) << flag
        << std::setw(spaceWidth + 3) << ""
        << f->desc << '\n';
    }
    // for persistent flags
    for (auto c = cmd->parent; c; c = c->parent) {
        for (const auto& f : c->flags) {
            if (!f->persistent) continue;
            std::string flag = "-" + std::string(1, f->shortName) + ", --" + f->longName;
            oss << std::right << std::setw(spaceWidth) << ""
            << std::left << std::setw(static_cast<int>(flagWidth)) << flag
            << std::setw(spaceWidth + 3) << ""
            << f->desc << '\n';
        }
    }
    if (!cmd->flags.empty())
        oss << '\n';
    oss << "use \"" << cmd->fullName() << "[command] --help\" for more information about a command" << '\n';
    return oss.str();
}

inline void addDefaults(Command* cmd) {
    if (!cmd->flagDuplicate("help")) {
        cmd->addFlag("help", 'h', cmd->helpRequested, "show this help message");
    }
    for (auto& c : cmd->children) {
        addDefaults(c.get());
    }
}

} /* namespace viper */
