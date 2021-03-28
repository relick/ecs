#ifndef __CONTRACT
#define __CONTRACT

// Contracts. If they are violated, the program is an invalid state, so nuke it from orbit
#ifndef Expects
#define Expects(cond) do { ((cond) ? static_cast<void>(0) : std::terminate()); } while(false)
#endif
#ifndef Ensures
#define Ensures(cond) do { ((cond) ? static_cast<void>(0) : std::terminate()); } while(false)
#endif

#endif // !__CONTRACT
