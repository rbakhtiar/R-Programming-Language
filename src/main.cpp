
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <regex>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace rplus {

using Value = std::variant<
    long long,
    double,
    bool,
    std::string
>;

struct Instruction {
    int id;
    std::string source;
    std::string command;
    bool saved = false;
};

struct Variable {
    std::string type;
    Value value;
    bool mutable_value;
};

class Runtime {
private:
    std::vector<Instruction> instructions;

    std::unordered_map<int, size_t> line_index;

    std::unordered_map<std::string, Variable> variables;

    static std::string trim(std::string s) {
        auto start = s.find_first_not_of(" \t\r\n");

        if (start == std::string::npos)
            return "";

        auto end = s.find_last_not_of(" \t\r\n");

        return s.substr(start, end - start + 1);
    }

    static std::vector<std::string> split(
        const std::string& input,
        char delimiter
    ) {
        std::vector<std::string> result;
        std::stringstream stream(input);
        std::string part;

        while (std::getline(stream, part, delimiter))
            result.push_back(trim(part));

        return result;
    }

    static std::string unquote(std::string value) {
        if (value.size() >= 2 &&
            value.front() == '"' &&
            value.back() == '"') {
            return value.substr(1, value.size() - 2);
        }

        return value;
    }

    Value parseValue(
        const std::string& type,
        const std::string& raw
    ) {
        try {
            if (type == "Int")
                return std::stoll(raw);

            if (type == "Float")
                return std::stod(raw);

            if (type == "Boolean") {
                if (raw == "true") return true;
                if (raw == "false") return false;

                throw std::runtime_error(
                    "Boolean must be true or false"
                );
            }

            if (type == "String")
                return unquote(raw);

            throw std::runtime_error(
                "Unsupported type: " + type
            );
        }
        catch (const std::invalid_argument&) {
            throw std::runtime_error(
                "Invalid value: " + raw
            );
        }
        catch (const std::out_of_range&) {
            throw std::runtime_error(
                "Value out of range: " + raw
            );
        }
    }

    static std::string valueToString(const Value& value) {
        return std::visit(
            [](const auto& v) -> std::string {
                using T = std::decay_t<decltype(v)>;

                if constexpr (std::is_same_v<T, std::string>)
                    return v;

                else if constexpr (std::is_same_v<T, bool>)
                    return v ? "true" : "false";

                else
                    return std::to_string(v);
            },
            value
        );
    }

    void execute(int id, int depth = 0) {
        if (depth > 32)
            throw std::runtime_error(
                "Maximum nested execution depth exceeded"
            );

        if (!line_index.count(id))
            throw std::runtime_error(
                "Unknown line l" + std::to_string(id)
            );

        Instruction& instruction =
            instructions[line_index.at(id)];

        if (!instruction.saved)
            throw std::runtime_error(
                "Line l" + std::to_string(id) +
                " has not been saved"
            );

        const std::string& command = instruction.command;

        if (command.rfind("text :", 0) == 0) {
            std::string content = trim(command.substr(6));

            auto style = split(content, ':');

            if (style.size() == 2) {
                std::cout << "[TEXT STYLE] "
                          << style[0] << " = "
                          << style[1] << '\n';
            } else {
                std::cout << unquote(content) << '\n';
            }
        }

        else if (command.rfind("number :", 0) == 0) {
            std::string raw = trim(command.substr(8));

            std::cout << raw << '\n';
        }

        else if (
            command.rfind("variable :", 0) == 0 ||
            command.rfind("mutable :", 0) == 0
        ) {
            bool isMutable =
                command.rfind("mutable :", 0) == 0;

            auto parts = split(
                command,
                ':'
            );

            if (parts.size() != 4)
                throw std::runtime_error(
                    "Expected: variable : name : Type : value"
                );

            std::string name = parts[1];
            std::string type = parts[2];

            if (variables.count(name))
                throw std::runtime_error(
                    "Variable already declared: " + name
                );

            variables[name] = {
                type,
                parseValue(type, parts[3]),
                isMutable
            };
        }

        else if (command.rfind("set :", 0) == 0) {
            auto parts = split(command, ':');

            if (parts.size() != 3)
                throw std::runtime_error(
                    "Expected: set : name : value"
                );

            std::string name = parts[1];

            if (!variables.count(name))
                throw std::runtime_error(
                    "Undefined variable: " + name
                );

            Variable& variable = variables.at(name);

            if (!variable.mutable_value)
                throw std::runtime_error(
                    "Cannot modify immutable variable: " + name
                );

            variable.value = parseValue(
                variable.type,
                parts[2]
            );
        }

        else if (command.rfind("print :", 0) == 0) {
            std::string name = trim(command.substr(7));

            if (variables.count(name)) {
                std::cout << valueToString(
                    variables.at(name).value
                ) << '\n';
            } else {
                std::cout << unquote(name) << '\n';
            }
        }

        else if (command.rfind("enter ", 0) == 0) {
            int target = std::stoi(command.substr(6));

            execute(target, depth + 1);
        }

        else if (
            command.rfind("save ", 0) == 0
        ) {
            throw std::runtime_error(
                "Save directives are handled by the loader"
            );
        }

        else {
            throw std::runtime_error(
                "Unknown instruction: " + command
            );
        }
    }

public:
    void load(const std::string& filename) {
        std::ifstream file(filename);

        if (!file)
            throw std::runtime_error(
                "Cannot open file: " + filename
            );

        std::string line;
        std::regex pattern(
            R"(^\s*\(l([0-9]+)\)\s*(.*?)\s*$)"
        );

        int lineNumber = 0;

        while (std::getline(file, line)) {
            ++lineNumber;

            if (trim(line).empty())
                continue;

            std::smatch match;

            if (!std::regex_match(line, match, pattern))
                throw std::runtime_error(
                    "Invalid numbered line at source line " +
                    std::to_string(lineNumber)
                );

            int id = std::stoi(match[1]);
            std::string command = trim(match[2]);

            if (line_index.count(id))
                throw std::runtime_error(
                    "Duplicate instruction ID: l" +
                    std::to_string(id)
                );

            line_index[id] = instructions.size();

            instructions.push_back({
                id,
                line,
                command,
                false
            });
        }

        if (instructions.empty())
            throw std::runtime_error(
                "Source file is empty"
            );
    }

    void run() {
        // Phase 1: register saved instructions.

        for (auto& instruction : instructions) {
            const std::string& cmd = instruction.command;

            if (cmd.rfind("save ", 0) == 0) {
                std::string target = trim(cmd.substr(5));

                if (target.size() < 2 || target[0] != 'l')
                    throw std::runtime_error(
                        "Invalid save target: " + target
                    );

                int id = std::stoi(target.substr(1));

                if (!line_index.count(id))
                    throw std::runtime_error(
                        "Cannot save unknown line: " + target
                    );

                instructions[line_index.at(id)].saved = true;
            }
        }

        // Phase 2: execute explicit entry points.

        for (const auto& instruction : instructions) {
            const std::string& cmd = instruction.command;

            if (cmd.rfind("enter ", 0) == 0) {
                std::string target = trim(cmd.substr(6));

                if (target.size() < 2 || target[0] != 'l')
                    throw std::runtime_error(
                        "Invalid entry target: " + target
                    );

                int id = std::stoi(target.substr(1));

                execute(id);
            }
        }
    }
};

}

int main(int argc, char* argv[]) {
    std::cout << "R+ Compiler 0.1.0\n";
    std::cout << "C++ Runtime Prototype\n";
    std::cout << "---------------------\n";

    if (argc != 2) {
        std::cerr << "Usage: rp <source.rp>\n";
        return 1;
    }

    try {
        rplus::Runtime runtime;

        runtime.load(argv[1]);
        runtime.run();

        std::cout << "\nR+ execution completed.\n";

        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "\nR+ ERROR: "
                  << error.what() << '\n';

        return 1;
    }
}
