// build CLI
// track what you need to do, what you have done, what you are currently working on.

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <limits>

struct Task {
    int id;
    std::string status, description;
    std::string createdAt, updatedAt;
};

std::string timestampNow() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds> (now.time_since_epoch()).count() % 1000;
    const auto* utc = std::gmtime(&time);
    if (!utc) throw std::runtime_error("Cannot determine current time.");
    std::ostringstream timestamp;
    timestamp << std::put_time(utc, "%Y-%m-%dT%H:%M:%S")
              << '.' << std::setfill('0') << std::setw(3) << millis << 'Z';
    return timestamp.str();
}

void printHelp() {
    std::cout << "Usage: task-cli <command> [arguments]\n\n"
              << "Commands:\n"
              << "  add <description>          Add a task\n"
              << "  update <id> <description>  Update a task\n"
              << "  delete <id>                Delete a task\n"
              << "  mark-in-progress <id>      Mark a task as in progress\n"
              << "  mark-done <id>             Mark a task as done\n"
              << "  list [status]              List all tasks or filter by status\n"
              << "  help                       Show this help\n\n"
              << "Statuses: todo, in-progress, done\n"
              << "Quote descriptions containing spaces. Tasks are saved in tasks.db\n"
              << "in the current directory.\n";
}

bool validStatus(const std::string& status) {
    return status == "todo" || status == "in-progress" || status == "done";
}

// argc: number of arguments, including the program name.
// argv: array of argument strings.
// e.g. task-cli add "Buy groceries"
// argc    // 3
// argv[0] // "task-cli" (program name or path)
// argv[1] // "add" (command)
// argv[2] // "Buy groceries" (command argument)
int main(int argc, char* argv[]) {
    try {
        const std::string command = argc > 1 ? argv[1] : "help"; // more than 1 argument, extract command, else "help".
        if (command == "help" || command == "--help" || command == "-h") {
            printHelp();
            return 0;
        }
        const bool add = command == "add", list = command == "list";
        const bool update = command == "update";
        const bool change = command == "delete" || command == "mark-in-progress" || command == "mark-done";
        if (!add && !list && !update && !change)
            throw std::runtime_error("Unknown command. Run task-cli help.");
        if ((list && argc != 2 && argc != 3) || ((add || change) && argc != 3) || (update && argc != 4))
            throw std::runtime_error("Invalid arguments. Run task-cli help.");
        if (list && argc == 3 && !validStatus(argv[2]))
            throw std::runtime_error("Status must be todo, in-progress, or done.");
        
        std::string description = add ? argv[2] : update ? argv[3] : "";

        if ((add || update) && (description.find_first_not_of(" \t\r\n") == std::string::npos || description.find_first_of("\r\n") != std::string::npos))
            throw std::runtime_error("Description must be nonempty and on one line.");

        int id = 0;
        if (change || update) {
            const std::string value = argv[2];
            auto result = std::from_chars(value.data(), value.data() + value.size(), id);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || id <= 0)
                throw std::runtime_error("Task ID must be a positive integer.");
        }

        std::vector<Task> tasks;
        int nextId = 1;
        if (std::filesystem::exists("tasks.db")) {
            std::ifstream input("tasks.db");
            std::string header;

            if (!input.is_open()) {
                throw std::runtime_error("Cannot open tasks.db.");
            }

            if (!(input >> header >> nextId)) {
                throw std::runtime_error("Cannot read header and nextId.");
            }

            if (header != "TASKCLI1" && header != "TASKCLI2" ) {
                throw std::runtime_error("Unrecognized header: " + header);
            }

            if (nextId < 1) {
                throw std::runtime_error("nextId must be at least 1.");
            }

            while (input >> std::ws && input.peek() != EOF) {
                Task task;
                if (!(input >> task.id >> task.status >> std::quoted(task.description)) || 
                    task.id <= 0 || task.id >= nextId || !validStatus(task.status) ||
                    std::any_of(tasks.begin(), tasks.end(), [&](const Task& t) { return t.id == task.id; }))
                    throw std::runtime_error("Cannot read tasks.db: invalid task data.");
                if (header == "TASKCLI2") {
                    if (!(input >> std::quoted(task.createdAt) >> std::quoted(task.updatedAt)) ||
                        task.createdAt.empty() || task.updatedAt.empty())
                        throw std::runtime_error("Cannot read tasks.db: invalid timestamps.");
                } else {
                    // Historical timestamps were not recorded by the original format.
                    task.createdAt = task.updatedAt = "unknown";
                }
                tasks.push_back(task);
            }
            if (input.bad()) throw std::runtime_error("Cannot read tasks.db.");
        }
        if (list) {
            int count = 0;
            for (const auto& task : tasks) {
                if (argc == 2 || task.status == argv[2]) {
                    std::cout << task.id << " [" << task.status << "] " << task.description << '\n'
                        << " createdAt: " << task.createdAt << '\n'
                        << " updatedAt: " << task.updatedAt << '\n';
                    ++count;
                }
            }
            if (!count) std::cout << "No tasks found.\n";
            return 0;
        }
        std::string message;
        if (add) {
            if (nextId == std::numeric_limits<int>::max()) {
                throw std::runtime_error("Task ID limit reached.");
            }
            id = nextId++;
            const auto timestamp = timestampNow();
            tasks.push_back({id, "todo", description, timestamp, timestamp});
            message = "Added task ";
        } else {
            auto task = std::find_if(tasks.begin(), tasks.end(), [&](const Task& t) { return t.id == id; });
            if (task == tasks.end()) throw std::runtime_error("Task ID not found.");
            if (command == "delete") tasks.erase(task);
            else {
                if (update) task->description = description;
                else task->status = command == "mark-done" ? "done" : "in-progress";
                task->updatedAt = timestampNow();
            }
            message = command == "delete"? "Deleted task " : "Updated task ";
        }
        // Never report success until the data has been flushed to disk.
        std::ofstream output("tasks.db", std::ios::trunc);
        output << "TASKCLI2 " << nextId << '\n';
        for (const auto& task : tasks) 
            output << task.id << ' ' << task.status << ' ' << std::quoted(task.description)
                << ' ' << std::quoted(task.createdAt) << ' ' << std::quoted(task.updatedAt) << '\n';
        
        // Flush buffered data and close the file
        output.close();
        
        if (!output) {
            throw std::runtime_error("Cannot save tasks.db.");
        }

        // Runs only if writing and closing reported no errors
        std::cout << message << id << ".\n";
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
};