#include <memory>
#include <string_view>
#include "custom_span.h"// 
#include <iostream>

int main(void) {
   // std::unique_ptr<uint8_t[]> ptr = std::make_unique<uint8_t[]>(1024);
   //  auto buf = Span(ptr.get(), 1024);
    // buf.begin();
    std::string di("dimitra");
    std::string_view v(di);
    std::cout << v;
    return 0;
}