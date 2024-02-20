# PolicyCB

PolicyCB is a C++20 library that provides a flexbible Callback implementation. This is a highly experimental implementation and shouldn't be use in any production environment. View the associated [Blog post](https://intmainreturn0.com/notes/callback-design.html) for more details. Users may configure the behavior of the class via the below template parameters:

```cpp
// The policy on allowed callable and Callback itself
// Note that when none of them is DYNAMIC, Callback<> could
// utilize flattened function pointer to save a virtual call
// @{
enum class MovePolicy
{
    // Allows non-trivially movable object
    DYNAMIC = 0,
    // Only allows trivially movable object
    TRIVIAL_ONLY = 1,
    // Forbids any move on Callback
    NOMOVE = 2,
};

enum class CopyPolicy
{
    // Allows non-trivially copyable object
    DYNAMIC = 0,
    // Only allows trivially copyable object
    TRIVIAL_ONLY = 1,
    // Forbids any copy on Callback
    NOCOPY = 2,
};

enum class DestroyPolicy
{
    // Allows non-trivially destructable object
    DYNAMIC = 0,
    // Only allows trivially destructable object
    TRIVIAL_ONLY = 1,
};

// @}

// Policy on the small-buffer-optimization storage
enum class SBOPolicy
{
    // Allows the Callback to store arbitrary-sized object
    // The storage takes InitialBufferSize + 8 bytes
    DYNAMIC_GROWTH = 0,
    // Only allows the Callback to store a object with specified
    // maximum size. Causes an compilation error if the object is
    // too large.
    FIXED_SIZE = 1,
    // This disables storage of the original function,
    // essentially makes the Callback a function pointer
    NO_STORAGE = 2,
};


template<typename FT, // function signature
         MovePolicy MP,
         CopyPolicy CP,
         DestroyPolicy DP,
         SBOPolicy SBOP,
         std::size_t InitialBufferSize = 16>
class Callback;
```

This is a header-only library. Drop in `include/PolicyCB.hpp` into your project to use it.

## License

Apache 2.0
