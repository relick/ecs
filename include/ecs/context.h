#ifndef __CONTEXT
#define __CONTEXT

/*#include <memory>
#include <vector>
#include <map>
#include <shared_mutex>*/
import std.core;
import std.threading;
#include <tls/cache.h>

#include "component_pool.h"
#include "system.h"
#include "type_hash.h"
#include "scheduler.h"

namespace ecs::detail {
	// The central class of the ecs implementation. Maintains the state of the system.
	class context final {
		// The values that make up the ecs core.
		std::vector<std::unique_ptr<system_base>> systems;
		std::vector<std::unique_ptr<component_pool_base>> component_pools;
		std::map<type_hash, component_pool_base*> type_pool_lookup;
		scheduler sched;

		mutable std::shared_mutex mutex;

	public:
		// Commits the changes to the entities.
		void commit_changes() {
			// Prevent other threads from
			//  adding components
			//  registering new component types
			//  adding new systems
			std::shared_lock lock(mutex);

			// Let the component pools handle pending add/remove requests for components
			for (auto const& pool : component_pools) {
				pool->process_changes();
			}

			// Let the systems respond to any changes in the component pools
			for (auto const& sys : systems) {
				sys->process_changes();
			}

			// Reset any dirty flags on pools
			for (auto const& pool : component_pools) {
				pool->clear_flags();
			}
		}

		// Calls the 'update' function on all the systems in the order they were added.
		void run_systems() {
			// Prevent other threads from adding new systems during the run
			std::shared_lock lock(mutex);

			// Run all the systems
			sched.run();
		}

		// Returns true if a pool for the type exists
		template<typename T>
		bool has_component_pool() const {
			// Prevent other threads from registering new component types
			std::shared_lock lock(mutex);

			constexpr auto hash = get_type_hash<T>();
			return type_pool_lookup.contains(hash);
		}

		// Resets the runtime state. Removes all systems, empties component pools
		void reset() {
			std::unique_lock<std::shared_mutex> lock(mutex);

			systems.clear();
			sched = scheduler();
			// context::component_pools.clear(); // this will cause an exception in get_component_pool() due to the cache
			for (auto& pool : component_pools) {
				pool->clear();
			}
		}

		// Returns a reference to a components pool.
		// If a pool doesn't exist, one will be created.
		template <typename T>
		component_pool<T>& get_component_pool() {
			thread_local tls::cache<type_hash, component_pool_base*, get_type_hash<void>()> cache;

			constexpr auto hash = get_type_hash<T>();
			auto pool = cache.get_or(hash, [this](type_hash hash) {
				std::shared_lock lock(mutex);
				
				// Look in the pool for the type
				auto const it = type_pool_lookup.find(hash);
				[[unlikely]] if (it == type_pool_lookup.end()) {
					// The pool wasn't found so create it.
					// create_component_pool takes a unique lock, so unlock the
					// shared lock during its call
					lock.unlock();
					return create_component_pool<T>();
				}
				else {
					return it->second;
				}
			});

			return *static_cast<component_pool<T>*>(pool);
		}

		// Const lambdas
		template <int Group, typename ExecutionPolicy, typename UserUpdateFunc, typename R, typename C, typename ...Args>
		auto& create_system(UserUpdateFunc update_func, R(C::*)(Args...) const) {
			return create_system<Group, ExecutionPolicy, UserUpdateFunc, Args...>(update_func);
		}

		// Mutable lambdas
		template <int Group, typename ExecutionPolicy, typename UserUpdateFunc, typename R, typename C, typename ...Args>
		auto& create_system(UserUpdateFunc update_func, R(C::*)(Args...)) {
			return create_system<Group, ExecutionPolicy, UserUpdateFunc, Args...>(update_func);
		}

	private:
		template <int Group, typename ExecutionPolicy, typename UserUpdateFunc, typename FirstArg, typename ...Args>
		auto& create_system(UserUpdateFunc update_func) {
			// Set up the implementation
			using typed_system = system<Group, ExecutionPolicy, UserUpdateFunc, FirstArg, Args...>;

			// Is the first argument an entity of sorts?
			bool constexpr has_entity = std::is_same_v<FirstArg, entity_id> || std::is_same_v<FirstArg, entity>;

			// Create the system instance
			std::unique_ptr<system_base> sys;
			if constexpr (has_entity) {
				sys = std::make_unique<typed_system>(
					update_func,
					/* dont add the entity as a component pool */
					&get_component_pool<std::remove_cv_t<std::remove_reference_t<Args>>>()...);
			}
			else {
				sys = std::make_unique<typed_system>(
					update_func,
					&get_component_pool<std::remove_cv_t<std::remove_reference_t<FirstArg>>>(),
					&get_component_pool<std::remove_cv_t<std::remove_reference_t<Args>>>()...);
			}

			std::unique_lock lock(mutex);
			systems.push_back(std::move(sys));
			system_base* ptr_system = systems.back().get();
			Ensures(ptr_system != nullptr);

			sort_systems_by_group();

			sched.insert(ptr_system);

			return *ptr_system;
		}

		// Sorts the systems based on their group number.
		// The sort maintains ordering in the individual groups.
		void sort_systems_by_group() {
			std::stable_sort(systems.begin(), systems.end(), [](auto const& l, auto const& r) {
				return l->get_group() < r->get_group();
			});
		}

		// Create a component pool for a new type
		template <typename T>
		component_pool_base* create_component_pool() {
			// Create a new pool if one does not already exist
			if (!has_component_pool<T>()) {
				std::unique_lock lock(mutex);

				auto pool = std::make_unique<component_pool<T>>();
				constexpr auto hash = get_type_hash<T>();
				type_pool_lookup.emplace(hash, pool.get());
				component_pools.push_back(std::move(pool));
				return component_pools.back().get();
			}
			else
				return &get_component_pool<T>();
		}
	};

	inline context& get_context() {
		static context ctx;
		return ctx;
	}

	// The global reference to the context
	static inline context& _context = get_context();
}

#endif // !__CONTEXT
