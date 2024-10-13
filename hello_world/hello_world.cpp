#include <iostream>  // For input-output stream
#include <cstdlib>   // For system("pause") or cin.get()

int main() {
    std::cout << "Hello, World!" << std::endl;  // Print Hello, World! to the console

    // Wait for user to press Enter
    std::cout << "Press Enter to exit...";
    std::cin.get();  // Wait for input from user
    
    return 0;  // Return 0 to indicate successful execution
}
