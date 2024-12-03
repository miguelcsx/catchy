#include <iostream>
#include <string>
#include <vector>
#include <cctype>

std::string countVowels(const std::string& word) {
    int count = 0;
    std::vector<char> vowels = {'a', 'e', 'i', 'o', 'u'}; // List of vowels

    // Iterate through each character in the word
    for (char c : word) { // +1
        for (char v : vowels) { // +2 (nesting level = 1)
            if (std::tolower(c) == v) { // +3 (nesting level = 2)
                count++;
            }
        }
    }
    
    if (count == 0) { // +1
        return "does not contain vowels";
    }
    return "contains " + std::to_string(count) + " vowels";
} // cognitive complexity = 7


int main() {
    std::string word;
    std::cout << "Enter a word: ";
    std::cin >> word;

    std::cout << word << " " << countVowels(word) << std::endl;

   return 0;
} // cognitive complexity = 0
