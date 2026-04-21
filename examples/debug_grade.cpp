#include <iostream>
#include <string>

template<typename... Args>
void print(Args&&... args) {
    bool __first = true;
    ((std::cout << (__first ? "" : " ") << args, __first = false), ...);
    std::cout << std::endl;
}

auto grade(auto score) {
    if ((score >= 90)) return "A";
    if ((score >= 80)) return "B";
    if ((score >= 70)) return "C";
    return "D";
}

int main() {
    print("grade(86):", grade(86));
    print("grade(95):", grade(95));
    print("done");
    return 0;
}
