#pragma once

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>

namespace PolicyCB {

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
} sboPolicy;

namespace internal {
struct Empty
{
};

template<typename T>
struct CallableTypeHelper;
template<typename Ret, typename... Args>
struct CallableTypeHelper<Ret(Args...)>
{
    using ReturnType = Ret;
    using ArgsTuple = std::tuple<Args...>;
    using TrampolineType = Ret(void*, Args&&...);
    using TrampolinePtrType = TrampolineType*;
    template<typename ObjT>
    using satisfiedBy = std::is_invocable_r<Ret, ObjT, Args&&...>;
};

template<typename FT, MovePolicy movePolicy, CopyPolicy copyPolicy, DestroyPolicy destroyPolicy>
struct WrapperBase;

template<typename RetT, MovePolicy movePolicy, CopyPolicy copyPolicy, DestroyPolicy destroyPolicy, typename... Args>
struct WrapperBase<RetT(Args...), movePolicy, copyPolicy, destroyPolicy>
{
    virtual ~WrapperBase() {}
    virtual void copyTo(void* dest) const = 0;
    virtual void moveTo(void* other) && = 0;
    virtual RetT invoke(Args&&... args) = 0;
};

template<typename FT, typename ObjT, MovePolicy movePolicy, CopyPolicy copyPolicy, DestroyPolicy destroyPolicy>
struct WrapperImpl;

template<typename RetT,
         typename ObjT,
         MovePolicy movePolicy,
         CopyPolicy copyPolicy,
         DestroyPolicy destroyPolicy,
         typename... Args>
struct WrapperImpl<RetT(Args...), ObjT, movePolicy, copyPolicy, destroyPolicy>
  : public WrapperBase<RetT(Args...), movePolicy, copyPolicy, destroyPolicy>
{
    ~WrapperImpl() {}
    [[no_unique_address]] ObjT obj;
    explicit WrapperImpl(ObjT&& obj)
      : obj(std::move(obj))
    {
    }
    void copyTo(void* other) const
    {
        if constexpr (copyPolicy == CopyPolicy::DYNAMIC) {
            new (static_cast<ObjT*>(other)) ObjT(obj);
        }
    }
    void moveTo(void* other) &&
    {
        if constexpr (movePolicy == MovePolicy::DYNAMIC) {
            new (static_cast<ObjT*>(other)) ObjT(std::move(obj));
        }
    }

    RetT invoke(Args&&... args) final
    {
        return obj(std::forward<Args>(args)...);
    }
};

template<SBOPolicy sboPolicy, std::size_t InitialBufferSize>
class SBOImpl
{
  public:
    static_assert(sboPolicy == SBOPolicy::NO_STORAGE || InitialBufferSize >= sizeof(std::size_t));
    using HeapStorageT = std::conditional_t<sboPolicy == SBOPolicy::NO_STORAGE || sboPolicy == SBOPolicy::FIXED_SIZE,
                                            Empty,
                                            std::unique_ptr<unsigned char[]>>;

    union PolyStackStorage {
        std::size_t heapBufferSize = 0;
        unsigned char stackBuffer[InitialBufferSize];
    };
    using StackStorageT = std::conditional_t<sboPolicy == SBOPolicy::NO_STORAGE, Empty, PolyStackStorage>;

  private:
    [[no_unique_address]] StackStorageT stackStorage;
    [[no_unique_address]] HeapStorageT heapStorage;

    void switchToHeap(std::size_t newSize)
    {
        if constexpr (sboPolicy != SBOPolicy::NO_STORAGE && sboPolicy != SBOPolicy::FIXED_SIZE) {
            assert(!heapStorage);
            heapStorage = std::make_unique<unsigned char[]>(newSize);
            stackStorage.heapBufferSize = newSize;
        }
    }

    void switchToStack()
    {
        if constexpr (sboPolicy != SBOPolicy::NO_STORAGE && sboPolicy != SBOPolicy::FIXED_SIZE) {
            assert(heapStorage);
            stackStorage.heapBufferSize = 0;
            heapStorage.reset();
        }
    }

  public:
    unsigned char* getStorage() noexcept
    {
        if constexpr (sboPolicy == SBOPolicy::NO_STORAGE) {
            return nullptr;
        } else if constexpr (sboPolicy == SBOPolicy::FIXED_SIZE) {
            return stackStorage.stackBuffer;
        } else {
            if (!heapStorage) {
                return stackStorage.stackBuffer;
            } else {
                return heapStorage.get();
            }
        }
    }

    const unsigned char* getStorage() const noexcept
    {
        if constexpr (sboPolicy == SBOPolicy::NO_STORAGE) {
            return nullptr;
        } else if constexpr (sboPolicy == SBOPolicy::FIXED_SIZE) {
            return stackStorage.stackBuffer;
        } else {
            if (!heapStorage) {
                return stackStorage.stackBuffer;
            } else {
                return heapStorage.get();
            }
        }
    }
    SBOImpl() = default;
    SBOImpl(SBOImpl&) = delete;
    SBOImpl(SBOImpl&&) = default;
    SBOImpl& operator=(SBOImpl&&) = default;
    SBOImpl& operator=(SBOImpl&) = delete;
    SBOImpl cloneStorage() const
    {
        SBOImpl result;
        if constexpr (sboPolicy == SBOPolicy::NO_STORAGE || sboPolicy == SBOPolicy::FIXED_SIZE) {
            result.stackStorage = stackStorage;
        } else {
            if (heapStorage) {
                result.heapStorage = std::make_unique<unsigned char[]>(effectiveBufferSize());
                result.stackStorage.heapBufferSize = stackStorage.heapBufferSize;
            }
        }
        return result;
    }

    void resizeTo(std::size_t newSize)
    {
        if constexpr (sboPolicy == SBOPolicy::NO_STORAGE || sboPolicy == SBOPolicy::FIXED_SIZE) {
            return;
        } else {
            if (newSize <= InitialBufferSize) {
                heapStorage.reset();
            } else if (!heapStorage || stackStorage.heapBufferSize != newSize) {
                stackStorage.heapBufferSize = newSize;
                heapStorage = std::make_unique<unsigned char[]>(newSize);
            }
        }
    }
    bool onHeap() const noexcept
    {
        if constexpr (sboPolicy == SBOPolicy::NO_STORAGE || sboPolicy == SBOPolicy::FIXED_SIZE) {
            return false;
        } else {
            return heapStorage != nullptr;
        }
    }

    size_t effectiveBufferSize() const noexcept
    {
        if constexpr (sboPolicy == SBOPolicy::NO_STORAGE || sboPolicy == SBOPolicy::FIXED_SIZE) {
            return InitialBufferSize;
        } else {
            return heapStorage ? stackStorage.heapBufferSize : InitialBufferSize;
        }
    }
};

template<typename FT, typename ObjT>
struct TrampolineImpl;

template<typename RetT, typename ObjT, typename... Args>
struct TrampolineImpl<RetT(Args...), ObjT>
{
    static_assert(std::is_invocable_r_v<RetT, ObjT, Args...>);
    static RetT call(void* obj, Args&&... args)
    {
        return std::invoke(*static_cast<ObjT*>(obj), std::forward<Args>(args)...);
    }
    static_assert(
      std::is_same_v<decltype(&TrampolineImpl::call), typename CallableTypeHelper<RetT(Args&&...)>::TrampolinePtrType>);
};

template<typename FT, typename ObjT>
using Trampoline = TrampolineImpl<FT, ObjT>;

// Base classes for potentially empty fields in Callback
// Making them into separate classes allows Null base optimization to kick in
// @{
template<typename T>
struct StorageBase
{
    [[no_unique_address]] T storage;
};

template<>
struct StorageBase<Empty>
{
};

template<typename TrampolineType>
struct TrampolineBase
{
    [[no_unique_address]] TrampolineType trampolinePtr;
};

template<>
struct TrampolineBase<Empty>
{
};

template<typename FuncPtrType>
struct FuncPtrBase
{
    [[no_unique_address]] FuncPtrType funcPtr;
};

template<>
struct FuncPtrBase<Empty>
{
};

template<typename FT,
         MovePolicy MP,
         CopyPolicy CP,
         DestroyPolicy DP,
         SBOPolicy SBOP,
         // Setting this to zero disables SBO
         // by allocating everything on heap
         std::size_t InitialBufferSize = 16>
struct CallbackTraits
{
    using StorageT = internal::SBOImpl<SBOP, InitialBufferSize>;
    enum class DynamicDispatchMethod
    {
        NO_DISPATCH = 0, // when SBOP is NO_STORAGE, equivalent to a function pointer
        FUNC_PTR = 1,    // Only when both MP/CP/DP are all TRIVIAL_ONLY or NO
        VIRTCALL = 2,    // when there're more than 1 dynamic method
    };

    static constexpr DynamicDispatchMethod dynamicDispatchMethod =
      SBOP == SBOPolicy::NO_STORAGE
        ? DynamicDispatchMethod::NO_DISPATCH
        : ((MP != MovePolicy::DYNAMIC) && (CP != CopyPolicy::DYNAMIC) && (DP != DestroyPolicy::DYNAMIC)
             ? DynamicDispatchMethod::FUNC_PTR
             : DynamicDispatchMethod::VIRTCALL);

    using type = typename internal::CallableTypeHelper<FT>::ReturnType;
    using ReturnType = typename internal::CallableTypeHelper<FT>::ReturnType;
    using ArgsTuple = typename internal::CallableTypeHelper<FT>::ArgsTuple;
    using FuncPtrType =
      std::conditional_t<dynamicDispatchMethod == DynamicDispatchMethod::NO_DISPATCH, FT*, internal::Empty>;

    template<typename ObjT>
    using StoredObjT = std::conditional_t<dynamicDispatchMethod == DynamicDispatchMethod::FUNC_PTR,
                                          ObjT,
                                          std::conditional_t<dynamicDispatchMethod == DynamicDispatchMethod::VIRTCALL,
                                                             internal::WrapperImpl<FT, ObjT, MP, CP, DP>,
                                                             internal::Empty>>;

    using WrapperBaseType = internal::WrapperBase<FT, MP, CP, DP>;

    using TrampolinePtrType = std::conditional_t<dynamicDispatchMethod == DynamicDispatchMethod::FUNC_PTR,
                                                 typename internal::CallableTypeHelper<FT>::TrampolinePtrType,
                                                 internal::Empty>;
};

} // namespace internal

template<typename FT,
         MovePolicy MP,
         CopyPolicy CP,
         DestroyPolicy DP,
         SBOPolicy SBOP,
         std::size_t InitialBufferSize = 16>
class Callback
  : private internal::CallbackTraits<FT, MP, CP, DP, SBOP, InitialBufferSize>
  , private internal::StorageBase<typename internal::CallbackTraits<FT, MP, CP, DP, SBOP, InitialBufferSize>::StorageT>
  , private internal::TrampolineBase<
      typename internal::CallbackTraits<FT, MP, CP, DP, SBOP, InitialBufferSize>::TrampolinePtrType>
  , private internal::FuncPtrBase<
      typename internal::CallbackTraits<FT, MP, CP, DP, SBOP, InitialBufferSize>::FuncPtrType>
{
  private:
    using Traits = internal::CallbackTraits<FT, MP, CP, DP, SBOP, InitialBufferSize>;
    using Traits::dynamicDispatchMethod;
    using typename Traits::DynamicDispatchMethod;

  public:
    using type = Traits::type;
    using ReturnType = Traits::ReturnType;
    using ArgsTuple = Traits::ArgsTuple;
    using FuncPtrType = Traits::FuncPtrType;

  private:
    auto getStoredObj() noexcept
    {
        if constexpr (dynamicDispatchMethod == DynamicDispatchMethod::FUNC_PTR) {
            return static_cast<void*>(this->storage.getStorage());
        } else if constexpr (dynamicDispatchMethod == DynamicDispatchMethod::VIRTCALL) {
            return std::launder(reinterpret_cast<Traits::WrapperBaseType*>(this->storage.getStorage()));
        } else {
            return nullptr;
        }
    }

  public:
    template<typename ObjT>
    explicit Callback(ObjT obj)
    {
        static_assert(internal::CallableTypeHelper<FT>::template satisfiedBy<ObjT&>::value);
        if constexpr (dynamicDispatchMethod == DynamicDispatchMethod::NO_DISPATCH) {
            static_assert(std::is_convertible_v<ObjT, FuncPtrType>);
            this->funcPtr = static_cast<FuncPtrType>(obj);
        } else if constexpr (dynamicDispatchMethod == DynamicDispatchMethod::FUNC_PTR) {
            this->trampolinePtr = &internal::Trampoline<FT, ObjT>::call;
            static_assert(CP != CopyPolicy::TRIVIAL_ONLY || std::is_trivially_copyable_v<ObjT>);
            static_assert(MP != MovePolicy::TRIVIAL_ONLY || std::is_trivially_move_constructible_v<ObjT>);
            static_assert(DP != DestroyPolicy::TRIVIAL_ONLY || std::is_trivially_destructible_v<ObjT>);
            this->storage.resizeTo(sizeof(ObjT));
            if constexpr (SBOP == SBOPolicy::FIXED_SIZE) {
                static_assert(sizeof(ObjT) <= InitialBufferSize);
            }
            new (this->storage.getStorage()) ObjT(std::move(obj));
        } else {
            this->storage.resizeTo(sizeof(typename Traits::template StoredObjT<ObjT>));
            if constexpr (SBOP == SBOPolicy::FIXED_SIZE) {
                static_assert(sizeof(typename Traits::template StoredObjT<ObjT>) <= InitialBufferSize);
            }
            new (this->storage.getStorage()) Traits::template StoredObjT<ObjT>(std::move(obj));
        }
    }

    template<typename... ArgsT>
    ReturnType operator()(ArgsT&&... args)
    {
        static_assert(std::is_invocable_r_v<ReturnType, FT, ArgsT...>);
        if constexpr (dynamicDispatchMethod == DynamicDispatchMethod::NO_DISPATCH) {
            return (*this->funcPtr)(std::forward<ArgsT>(args)...);
        } else if constexpr (dynamicDispatchMethod == DynamicDispatchMethod::FUNC_PTR) {
            return (*this->trampolinePtr)(getStoredObj(), std::forward<ArgsT>(args)...);
        } else if constexpr (dynamicDispatchMethod == DynamicDispatchMethod::VIRTCALL) {
            return (getStoredObj())->invoke(std::forward<ArgsT>(args)...);
        }
    }

    ~Callback()
    {
        if constexpr (dynamicDispatchMethod == DynamicDispatchMethod::NO_DISPATCH ||
                      dynamicDispatchMethod == DynamicDispatchMethod::FUNC_PTR) {
            return;
        } else if constexpr (dynamicDispatchMethod == DynamicDispatchMethod::VIRTCALL) {
            using WrapperBaseType = Traits::WrapperBaseType;
            (getStoredObj())->~WrapperBaseType();
        }
    }

    Callback(const std::enable_if_t<CP != CopyPolicy::NOCOPY, Callback>& other)
    {
        if constexpr (dynamicDispatchMethod == DynamicDispatchMethod::VIRTCALL) {
            this->storage = other.storage.cloneStorage();
            getStoredObj()->copyTo(this->storage.getStorage());

        } else if constexpr (dynamicDispatchMethod == DynamicDispatchMethod::FUNC_PTR) {
            this->storage = other.storage.cloneStorage();
            memcpy(this->storage.getStorage(), other.storage.getStorage(), this->storage.effectiveBufferSize());
            this->trampolinePtr = other.trampolinePtr;
        } else {
            this->funcPtr = other.funcPtr;
        }
    }

    Callback(std::enable_if_t<MP != MovePolicy::NOMOVE, Callback>&& other)
    {
        if constexpr (dynamicDispatchMethod == DynamicDispatchMethod::VIRTCALL) {
            if (other.storage.onHeap()) {
                this->storage = std::move(other.storage);
            } else {
                other.getStoredObj()->moveTo(this->storage.getStorage());
            }
        } else if constexpr (dynamicDispatchMethod == DynamicDispatchMethod::FUNC_PTR) {
            if (other.storage.onHeap()) {
                this->storage = std::move(other.storage);
            } else {
                other.getStoredObj()->moveTo(this->storage.getStorage());
            }
            this->trampolinePtr = other.trampolinePtr;
        } else {
            this->funcPtr = other.funcPtr;
        }
    };
};
}