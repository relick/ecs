#ifndef ECS_ENTITY_ID
#define ECS_ENTITY_ID

#include <cstddef>

namespace ecs {
    namespace detail {
        using entity_type = std::ptrdiff_t;
        using entity_offset = std::size_t; // must cover the entire entity_type domain
    } // namespace detail

    // A simple struct that is an entity identifier.
    struct entity_id {
        using offset = detail::entity_offset;
        using value = detail::entity_type;

        // Uninitialized entity ids are not allowed, because they make no sense
        entity_id() = delete;

        constexpr entity_id(value _id) noexcept
            : id(_id) {
        }

        constexpr operator value& () noexcept {
            return id;
        }
        constexpr operator value const& () const noexcept {
            return id;
        }

    private:
        value id;
    };
} // namespace ecs

#endif // !ECS_ENTITY_ID
