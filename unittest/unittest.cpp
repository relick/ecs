#define CATCH_CONFIG_MAIN
#include "catch.hpp"

namespace std {
    bool __cdecl std::operator==(class std::exception_ptr const&, std::nullptr_t) noexcept {
        return false;
    }

}