#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <charconv>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <limits>
#include <sstream>
#include <stdexcept>

struct Expense {
    int id;
    float amount;
    std::string description;
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
    std::cout << "Usage: expense-tracker <command> [arguments]\n\n"
              << "Commands:\n"
              << "  add --description <description> --amount <amount>         Add a task\n"
              << "  list                               List all tasks\n"
              << "  summary                            Show summary\n"
              << "  summary --month <month>                    Show summary for specific month\n"
              << "  delete --id <id>                        Delete a task\n"
              << "  help                       Show this help\n\n"
              << "Months: 1 - 12\n"
              << "Quote descriptions containing spaces. Expenses are saved in expenses.db\n"
              << "in the current directory.\n";
}

bool validMonth(const std::string& month) {
    return std::stoi(month) >= 1 && std::stoi(month) <= 12;
}

int main(int argc, char* argv[]) {
    try {
        const std::string command = argc > 1 ? argv[1] : "help";
        if (command == "help" || command == "--help" || command == "-h") {
            printHelp();
            return 0;
        }
        const bool add = command == "add";
        const bool list = command == "list";
        const bool summary = command == "summary";
        const bool change = command == "delete";
        if (!add && !list && !summary && !change)
            throw std::runtime_error("Unknown command. Run expense-tracker help.");
        if ((list && argc != 2) || (summary && argc != 2 && argc != 4) ||
            (add && argc != 6) || (change && argc != 4))
            throw std::runtime_error("Invalid arguments. Run expense-tracker help.");

        // argv[0] is the program, argv[1] is the command.
        std::string description;
        float amount = 0;
        int month = 0; // 0 means summary for all months.
        int id = 0;
        auto parsePositiveInt = [](const std::string& value) {
            int number = 0;
            const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || number <= 0)
                throw std::runtime_error("Expected a positive integer: " + value);
            return number;
        };
        if (add) {
            if (std::string(argv[2]) != "--description" || std::string(argv[4]) != "--amount")
                throw std::runtime_error("Usage: expense-tracker add --description <description> --amount <amount>");
            description = argv[3];
            const std::string value = argv[5];
            std::size_t consumed = 0;
            try {
                amount = std::stof(value, &consumed);
            } catch (const std::exception&) {
                throw std::runtime_error("Amount must be a finite positive number.");
            }
            if (consumed != value.size() || !std::isfinite(amount) || amount <= 0)
                throw std::runtime_error("Amount must be a finite positive number.");
        }
        if (summary && argc == 4) {
            if (std::string(argv[2]) != "--month")
                throw std::runtime_error("Usage: expense-tracker summary --month <month>");
            month = parsePositiveInt(argv[3]);
            if (month > 12) throw std::runtime_error("Month must be within 1 to 12.");
        }

        if (add && (description.find_first_not_of(" \t\r\n") == std::string::npos || description.find_first_of("\r\n") != std::string::npos))
            throw std::runtime_error("Description must be nonempty and on one line.");

        if (change) {
            if (std::string(argv[2]) != "--id")
                throw std::runtime_error("Usage: expense-tracker delete --id <id>");
            id = parsePositiveInt(argv[3]);
        }

        std::vector<Expense> expenses;
        int nextId = 1;
        if (std::filesystem::exists("expenses.db")) {
            std::ifstream input("expenses.db");
            std::string header;

            if (!input.is_open()) {
                throw std::runtime_error("Cannot open expenses.db");
            }

            if (!(input >> header >> nextId)) {
                throw std::runtime_error("Cannot read header and nextId.");
            }

            if (header != "EXPENSE1" && header != "EXPENSE2") {
                throw std::runtime_error("Unrecognized header: " + header);
            }

            if (nextId < 1) {
                throw std::runtime_error("nextId must be at least 1.");
            }

            while (input >> std::ws && input.peek() != EOF) {
                Expense expense;
                if (!(input >> expense.id
                            >> std::quoted(expense.description)
                            >> expense.amount
                            >> std::quoted(expense.createdAt)
                            >> std::quoted(expense.updatedAt)) ||
                    expense.id <= 0 ||
                    expense.id >= nextId ||
                    !std::isfinite(expense.amount) ||
                    expense.amount <= 0 ||
                    expense.createdAt.empty() ||
                    expense.updatedAt.empty() ||
                    std::any_of(expenses.begin(), expenses.end(),
                        [&](const Expense& e) {
                            return e.id == expense.id;
                        })) {
                    throw std::runtime_error("Cannot read expenses.db: invalid expense.");
            }
                if (header == "EXPENSE1") {
                    if (!(input >> std::quoted(expense.createdAt) >> std::quoted(expense.updatedAt)) ||
                        expense.createdAt.empty() || expense.updatedAt.empty())
                        throw std::runtime_error("Cannot read tasks.db: invalid timestamps.");
                } else {
                    expense.createdAt = expense.updatedAt = "unknown";
                }
                expenses.push_back(expense);
            }
            if (input.bad()) throw std::runtime_error("Cannot read expense.db."); 
        }

        if (list) {
            int count = 0;
            for (const auto& expense : expenses) {
                if (argc == 2 && summary) { // summary <month> OR delete <id>
                    std::cout << expense.id << expense.description << '\n'
                        << " createdAt: " << expense.createdAt << '\n'
                        << " updatedAt: " << expense.updatedAt << '\n';
                    ++count;
                }
            }
            if (!count) std::cout << "No expense found.\n";
            return 0;
        }

        std::string message;
        if (add) {
            if (nextId == std::numeric_limits<int>::max()) {
                throw std::runtime_error("Expense ID limit reached.");
            }
            id = nextId++;
            const auto timestamp = timestampNow();
            expenses.push_back({id, amount, description, timestamp, timestamp});
            message = "Added task ";
        } else {
            auto expense = std::find_if(expenses.begin(), expenses.end(), [&](const Expense& e) {return e.id == id; });
            if (expense == expenses.end()) throw std::runtime_error("Expense ID not found");
            if (command == "delete") expenses.erase(expense);
            message = "delete";
        }

        // No report success message until data has been flushed to disk
        std::ofstream output("expenses.db", std::ios::trunc);
        output << "EXPENSE2 " << nextId << '\n';
        for (const auto& expense : expenses)
            output << expense.id << ' ' << std::quoted(expense.description) 
                << ' ' << expense.amount << ' ' << std::quoted(expense.createdAt) << std::quoted(expense.updatedAt) << '\n';
                
        // Flush buffered data and close the file.
        output.close();

        if (!output) {
            throw std::runtime_error("Cannot save expenses.db.");
        }

        std::cout << message << id << ".\n";
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
};
