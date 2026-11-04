#include <iostream>

int main(void) {
    int choice, guess, chance;
    int count = 0;
    int answer = rand() % 101;
    std::cout << "Welcome to the Number Guessing Game!" << std::endl;
    std::cout << "I'm thinking of a number between 1 and 100." << std::endl;
    std::cout << "You have 5 chances to guess the correct number." << std::endl;
    std::cout << std::endl;

    std::cout << "Please select the difficulty level:" << std::endl;
    std::cout << "1. Easy (10 chances)" << std::endl;
    std::cout << "2. Medium (5 chances)" << std::endl;
    std::cout << "3. Hard (3 chances)" << std::endl;
    std::cout << std::endl;

    std::cout << "Enter your choice: " << std::endl;
    std::cin >> choice;

    std::cout << "Great! You have selected the ";
    if (choice == 1) {
        std::cout << "Easy ";
        chance = 10;
    }
    else if (choice == 2) {
        std::cout << "Medium ";
        chance = 5;
    }
    else if (choice == 3) {
        std::cout << "Hard ";
        chance = 3;
    }
    std::cout << "difficulty level." << std::endl;
    std::cout << "Let's start the game!" << std::endl << std::endl;

    do {
        std::cout << "Enter your guess: ";
        std::cin >> guess;
        count++;
        if (guess != answer) {
            std::cout << "Incorrect! ";
            if (answer > guess) std::cout << "The number is greater than " << guess << "." << std::endl;
            else if (answer < guess) std::cout << "The number is less than " << guess << "." << std::endl;
        } else {
            std::cout << "Congratulations! You guessed the correct number in " << count << " attempts." << std::endl;
        }
        
    } while (guess != answer && count < chance);

    if (guess != answer && count == chance) 
        std::cout << "Thanks for joining the game. Have a try again!" << std::endl;
    return 0;
}