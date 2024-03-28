#ifndef FAITHFUL_FUNCTION_H
#define FAITHFUL_FUNCTION_H

#include <functional>
#include <type_traits>

namespace faithful {}  // namespace faithful

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
///////// THIS IS NOT MY CODE (!!!) Thanks to -->  (Facebook) Folly

#include <cstring>
#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace folly {

template <typename FunctionType>
class Function;

template <typename ReturnType, typename... Args>
Function<ReturnType(Args...) const> constCastFunction(
    Function<ReturnType(Args...)>&&) noexcept;

template <typename ReturnType, typename... Args>
Function<ReturnType(Args...) const noexcept> constCastFunction(
    Function<ReturnType(Args...) noexcept>&&) noexcept;

namespace detail {
namespace function {

enum class Op {
  MOVE,
  NUKE,
  HEAP
};

union Data {
  void* big;
  std::aligned_storage<6 * sizeof(void*)>::type tiny;
};

struct CoerceTag {};

// TODO 1: _____________________________________________________
template <typename T>
struct IsNullptrCompatible {
  template <typename U>
  static auto test(U* ptr)
      -> decltype(static_cast<bool>(static_cast<U const&>(*ptr) == nullptr),
                  std::true_type{});

  static std::false_type test(...);

  static constexpr bool value = decltype(test(static_cast<T*>(nullptr)))::value;
};

template <typename T, std::enable_if_t<!IsNullptrCompatible<T>::value, int> = 0>
constexpr bool isEmptyFunction(T const&) {
  return false;
}
template <typename T, std::enable_if_t<IsNullptrCompatible<T>::value, int> = 0>
constexpr bool isEmptyFunction(T const& t) {
  return static_cast<bool>(t == nullptr);
}

template <typename F, typename... Args>
using CallableResult = decltype(std::declval<F&&>()(std::declval<Args&&>()...));

template <typename F, typename... Args>
constexpr bool CallableNoexcept =
    noexcept(std::declval<F&&>()(std::declval<Args&&>()...));

template <typename From, typename To,
          typename =
              typename std::enable_if<!std::is_reference<To>::value ||
                                      std::is_reference<From>::value>::type>
using IfSafeResultImpl = decltype(void(static_cast<To>(std::declval<From>())));

constexpr std::size_t register_pass_max_size = 8u;  // kMscVer ? 8u : 16u;

template <typename T>
constexpr bool is_register_pass_v =
    (sizeof(T) <= register_pass_max_size) && std::is_trivially_copyable_v<T>;
template <typename T>
constexpr bool is_register_pass_v<T&> = true;
template <typename T>
constexpr bool is_register_pass_v<T&&> = true;

template <typename, typename>
inline constexpr bool is_similar_instantiation_v = false;
template <template <typename...> class C, typename... A, typename... B>
inline constexpr bool is_similar_instantiation_v<C<A...>, C<B...>> = true;
template <typename A, typename B>
struct is_similar_instantiation
    : std::bool_constant<is_similar_instantiation_v<A, B>> {};

template <typename T>
using CallArg = std::conditional_t<is_register_pass_v<T>, T, T&&>;

template <typename F, bool Nx, typename R, typename... A>
class FunctionTraitsSharedProxy {
  std::shared_ptr<Function<F>> sp_;

 public:
  explicit FunctionTraitsSharedProxy(std::nullptr_t) noexcept {
  }
  explicit FunctionTraitsSharedProxy(Function<F>&& func)
      : sp_(func ? std::make_shared<Function<F>>(std::move(func))
                 : std::shared_ptr<Function<F>>()) {
  }
  R operator()(A... args) const noexcept(Nx) {
    if (!sp_) {
      throw std::bad_function_call();  // ---------------------------- r1
      // throw_exception<std::bad_function_call>();
    }
    return (*sp_)(static_cast<A&&>(args)...);
  }

  explicit operator bool() const noexcept {
    return sp_ != nullptr;
  }

  friend bool operator==(FunctionTraitsSharedProxy const& proxy,
                         std::nullptr_t) noexcept {
    return proxy.sp_ == nullptr;
  }
  friend bool operator!=(FunctionTraitsSharedProxy const& proxy,
                         std::nullptr_t) noexcept {
    return proxy.sp_ != nullptr;
  }

  friend bool operator==(std::nullptr_t,
                         FunctionTraitsSharedProxy const& proxy) noexcept {
    return proxy.sp_ == nullptr;
  }
  friend bool operator!=(std::nullptr_t,
                         FunctionTraitsSharedProxy const& proxy) noexcept {
    return proxy.sp_ != nullptr;
  }
};

template <typename FunctionType>
struct FunctionTraits;

template <typename ReturnType, typename... Args>
struct FunctionTraits<ReturnType(Args...)> {
  using Call = ReturnType (*)(CallArg<Args>..., Data&);
  using ConstSignature = ReturnType(Args...) const;
  using NonConstSignature = ReturnType(Args...);
  using OtherSignature = ConstSignature;

  template <typename F, typename R = CallableResult<F&, Args...>>
  using IfSafeResult = IfSafeResultImpl<R, ReturnType>;

  template <typename Fun>
  static ReturnType callSmall(CallArg<Args>... args, Data& p) {
    auto& fn = *static_cast<Fun*>(static_cast<void*>(&p.tiny));
    if constexpr (std::is_void<ReturnType>::value) {
      fn(static_cast<Args&&>(args)...);
    } else {
      return fn(static_cast<Args&&>(args)...);
    }
  }

  template <typename Fun>
  static ReturnType callBig(CallArg<Args>... args, Data& p) {
    auto& fn = *static_cast<Fun*>(p.big);
    if constexpr (std::is_void<ReturnType>::value) {
      fn(static_cast<Args&&>(args)...);
    } else {
      return fn(static_cast<Args&&>(args)...);
    }
  }

  static ReturnType uninitCall(CallArg<Args>..., Data&) {
    throw std::bad_function_call();  // ---------------------------- r1
    // throw_exception<std::bad_function_call>();
  }

  ReturnType operator()(Args... args) {
    auto& fn = *static_cast<Function<NonConstSignature>*>(this);
    return fn.call_(static_cast<Args&&>(args)..., fn.data_);
  }

  using SharedProxy =
      FunctionTraitsSharedProxy<NonConstSignature, false, ReturnType, Args...>;
};

template <typename ReturnType, typename... Args>
struct FunctionTraits<ReturnType(Args...) const> {
  using Call = ReturnType (*)(CallArg<Args>..., Data&);
  using ConstSignature = ReturnType(Args...) const;
  using NonConstSignature = ReturnType(Args...);
  using OtherSignature = NonConstSignature;

  template <typename F, typename R = CallableResult<const F&, Args...>>
  using IfSafeResult = IfSafeResultImpl<R, ReturnType>;

  template <typename Fun>
  static ReturnType callSmall(CallArg<Args>... args, Data& p) {
    auto& fn = *static_cast<const Fun*>(static_cast<void*>(&p.tiny));
    if constexpr (std::is_void<ReturnType>::value) {
      fn(static_cast<Args&&>(args)...);
    } else {
      return fn(static_cast<Args&&>(args)...);
    }
  }

  template <typename Fun>
  static ReturnType callBig(CallArg<Args>... args, Data& p) {
    auto& fn = *static_cast<const Fun*>(p.big);
    if constexpr (std::is_void<ReturnType>::value) {
      fn(static_cast<Args&&>(args)...);
    } else {
      return fn(static_cast<Args&&>(args)...);
    }
  }

  static ReturnType uninitCall(CallArg<Args>..., Data&) {
    throw std::bad_function_call();  // ---------------------------- r1
    // throw_exception<std::bad_function_call>();
  }

  ReturnType operator()(Args... args) const {
    auto& fn = *static_cast<const Function<ConstSignature>*>(this);
    return fn.call_(static_cast<Args&&>(args)..., fn.data_);
  }

  using SharedProxy =
      FunctionTraitsSharedProxy<ConstSignature, false, ReturnType, Args...>;
};

template <typename ReturnType, typename... Args>
struct FunctionTraits<ReturnType(Args...) noexcept> {
  using Call = ReturnType (*)(CallArg<Args>..., Data&) noexcept;
  using ConstSignature = ReturnType(Args...) const noexcept;
  using NonConstSignature = ReturnType(Args...) noexcept;
  using OtherSignature = ConstSignature;

  template <typename F, typename R = CallableResult<F&, Args...>,
            std::enable_if_t<CallableNoexcept<F&, Args...>, int> = 0>
  using IfSafeResult = IfSafeResultImpl<R, ReturnType>;

  template <typename Fun>
  static ReturnType callSmall(CallArg<Args>... args, Data& p) noexcept {
    auto& fn = *static_cast<Fun*>(static_cast<void*>(&p.tiny));
    if constexpr (std::is_void<ReturnType>::value) {
      fn(static_cast<Args&&>(args)...);
    } else {
      return fn(static_cast<Args&&>(args)...);
    }
  }

  template <typename Fun>
  static ReturnType callBig(CallArg<Args>... args, Data& p) noexcept {
    auto& fn = *static_cast<Fun*>(p.big);
    if constexpr (std::is_void<ReturnType>::value) {
      fn(static_cast<Args&&>(args)...);
    } else {
      return fn(static_cast<Args&&>(args)...);
    }
  }

  static ReturnType uninitCall(CallArg<Args>..., Data&) noexcept {
    std::terminate();  // ---------------------------- r2
    // terminate_with<std::bad_function_call>();
  }

  ReturnType operator()(Args... args) noexcept {
    auto& fn = *static_cast<Function<NonConstSignature>*>(this);
    return fn.call_(static_cast<Args&&>(args)..., fn.data_);
  }

  using SharedProxy =
      FunctionTraitsSharedProxy<NonConstSignature, true, ReturnType, Args...>;
};

template <typename ReturnType, typename... Args>
struct FunctionTraits<ReturnType(Args...) const noexcept> {
  using Call = ReturnType (*)(CallArg<Args>..., Data&) noexcept;
  using ConstSignature = ReturnType(Args...) const noexcept;
  using NonConstSignature = ReturnType(Args...) noexcept;
  using OtherSignature = NonConstSignature;

  template <typename F, typename R = CallableResult<const F&, Args...>,
            std::enable_if_t<CallableNoexcept<const F&, Args...>, int> = 0>
  using IfSafeResult = IfSafeResultImpl<R, ReturnType>;

  template <typename Fun>
  static ReturnType callSmall(CallArg<Args>... args, Data& p) noexcept {
    auto& fn = *static_cast<const Fun*>(static_cast<void*>(&p.tiny));
    if constexpr (std::is_void<ReturnType>::value) {
      fn(static_cast<Args&&>(args)...);
    } else {
      return fn(static_cast<Args&&>(args)...);
    }
  }

  template <typename Fun>
  static ReturnType callBig(CallArg<Args>... args, Data& p) noexcept {
    auto& fn = *static_cast<const Fun*>(p.big);
    if constexpr (std::is_void<ReturnType>::value) {
      fn(static_cast<Args&&>(args)...);
    } else {
      return fn(static_cast<Args&&>(args)...);
    }
  }

  static ReturnType uninitCall(CallArg<Args>..., Data&) noexcept {
    std::terminate();  // ---------------------------- r2
    // terminate_with<std::bad_function_call>();
  }

  ReturnType operator()(Args... args) const noexcept {
    auto& fn = *static_cast<const Function<ConstSignature>*>(this);
    return fn.call_(static_cast<Args&&>(args)..., fn.data_);
  }

  using SharedProxy =
      FunctionTraitsSharedProxy<ConstSignature, true, ReturnType, Args...>;
};

std::size_t exec_(Op, Data*, Data*) noexcept;
using Exec = decltype(&exec_);
static_assert(noexcept(Exec(nullptr)(Op{}, nullptr, nullptr)), "");
struct DispatchSmallTrivial {
  template <typename Fun, typename Base>
  static constexpr auto call = Base::template callSmall<Fun>;

  template <std::size_t Size>
  static std::size_t exec_(Op o, Data* src, Data* dst) noexcept {
    switch (o) {
      case Op::MOVE:
        std::memcpy(static_cast<void*>(dst), static_cast<void*>(src), Size);
        break;
      case Op::NUKE:
        break;
      case Op::HEAP:
        break;
    }
    return 0U;
  }
  template <std::size_t size, std::size_t adjust = alignof(Data) - 1>
  static constexpr std::size_t size_ = (size + adjust) & ~adjust;
  template <typename Fun>
  static constexpr auto exec = exec_<size_<sizeof(Fun)>>;
};

struct DispatchSmall {
  template <typename Fun, typename Base>
  static constexpr auto call = Base::template callSmall<Fun>;

  template <typename Fun>
  static std::size_t exec(Op o, Data* src, Data* dst) noexcept {
    switch (o) {
      case Op::MOVE:
        ::new (static_cast<void*>(&dst->tiny)) Fun(static_cast<Fun&&>(
            *static_cast<Fun*>(static_cast<void*>(&src->tiny))));
        // TODO: __attribute__((fallthrough));
      case Op::NUKE:
        static_cast<Fun*>(static_cast<void*>(&src->tiny))->~Fun();
        break;
      case Op::HEAP:
        break;
    }
    return 0U;
  }
};

struct DispatchBig {
  template <typename Fun, typename Base>
  static constexpr auto call = Base::template callBig<Fun>;

  template <typename Fun>
  static std::size_t exec(Op o, Data* src, Data* dst) noexcept {
    switch (o) {
      case Op::MOVE:
        dst->big = src->big;
        src->big = nullptr;
        break;
      case Op::NUKE:
        delete static_cast<Fun*>(src->big);
        break;
      case Op::HEAP:
        break;
    }
    return sizeof(Fun);
  }
};

}  // namespace function
}  // namespace detail

template <typename FunctionType>
class Function final : private detail::function::FunctionTraits<FunctionType> {
  using Data = detail::function::Data;
  using Op = detail::function::Op;
  using CoerceTag = detail::function::CoerceTag;

  using Traits = detail::function::FunctionTraits<FunctionType>;
  using Call = typename Traits::Call;
  using Exec = detail::function::Exec;

  // mutable Data data_{unsafe_default_initialized};
  // ---------------------------------- r3
  Data data_{};

  Call call_{&Traits::uninitCall};
  Exec exec_{nullptr};

  std::size_t exec(Op o, Data* src, Data* dst) const {
    if (!exec_) {
      return 0U;
    }
    return exec_(o, src, dst);
  }

  friend Traits;
  friend Function<typename Traits::ConstSignature> folly::constCastFunction<>(
      Function<typename Traits::NonConstSignature>&&) noexcept;
  friend class Function<typename Traits::OtherSignature>;

  template <typename Signature>
  Function(Function<Signature>&& fun, CoerceTag) {
    using Fun = Function<Signature>;
    if (fun) {
      data_.big = new Fun(static_cast<Fun&&>(fun));
      call_ = Traits::template callBig<Fun>;
      exec_ = Exec(detail::function::DispatchBig::exec<Fun>);
    }
  }

  Function(Function<typename Traits::OtherSignature>&& that, CoerceTag) noexcept
      : call_(that.call_),
        exec_(that.exec_) {
    that.call_ = &Traits::uninitCall;
    that.exec_ = nullptr;
    exec(Op::MOVE, &that.data_, &data_);
  }

 public:
  constexpr Function() = default;

  // not copyable
  Function(const Function&) = delete;

  Function(Function&& that) noexcept
      : call_(that.call_),
        exec_(that.exec_) {
    // that must be uninitialized before exec() call in the case of self move
    that.call_ = &Traits::uninitCall;
    that.exec_ = nullptr;
    exec(Op::MOVE, &that.data_, &data_);
  }

  /* implicit */ constexpr Function(std::nullptr_t) noexcept {
  }

  template <typename Fun,
            typename = std::enable_if_t<
                !detail::function::is_similar_instantiation_v<Function, Fun>>,
            typename = typename Traits::template IfSafeResult<Fun>,
            bool IsSmall = (                      //
                sizeof(Fun) <= sizeof(Data) &&    //
                alignof(Fun) <= alignof(Data) &&  //
                noexcept(Fun(std::declval<Fun>())))>
  /* implicit */ constexpr Function(Fun fun) noexcept(IsSmall) {
    using Dispatch = std::conditional_t<
        IsSmall && std::is_trivially_copyable_v<Fun>,
        detail::function::DispatchSmallTrivial,
        std::conditional_t<IsSmall, detail::function::DispatchSmall,
                           detail::function::DispatchBig>>;
    if constexpr (detail::function::IsNullptrCompatible<Fun>::value) {
      if (detail::function::isEmptyFunction(fun)) {
        return;
      }
    }
    if constexpr (IsSmall) {
      if constexpr (!std::is_empty<Fun>::value ||
                    !std::is_trivially_copyable_v<Fun>) {
        ::new (&data_.tiny) Fun(static_cast<Fun&&>(fun));
      }
    } else {
      data_.big = new Fun(static_cast<Fun&&>(fun));
    }
    call_ = Dispatch::template call<Fun, Traits>;
    exec_ = Exec(Dispatch::template exec<Fun>);
  }

  template <typename Signature, typename Fun = Function<Signature>,
            // prevent gcc from making this a better match than move-ctor
            typename = std::enable_if_t<!std::is_same<Function, Fun>::value>,
            typename = typename Traits::template IfSafeResult<Fun>>
  Function(Function<Signature>&& that) noexcept(
      noexcept(Function(std::move(that), CoerceTag{})))
      : Function(std::move(that), CoerceTag{}) {
  }

  template <typename Member, typename Class,
            // Prevent this overload from being selected when `ptr` is not a
            // compatible member function pointer.
            typename = decltype(Function(std::mem_fn((Member Class::*)0)))>
  /* implicit */ Function(Member Class::*ptr) noexcept {
    if (ptr) {
      *this = std::mem_fn(ptr);
    }
  }

  ~Function() {
    exec(Op::NUKE, &data_, nullptr);
  }

  Function& operator=(const Function&) = delete;

  Function& operator=(Function&& that) noexcept {
    exec(Op::NUKE, &data_, nullptr);
    if (this != &that) {
      [[likely]] that.exec(Op::MOVE, &that.data_, &data_);
      exec_ = that.exec_;
      call_ = that.call_;
    }
    that.exec_ = nullptr;
    that.call_ = &Traits::uninitCall;
    return *this;
  }

  template <typename Fun, typename...,
            bool Nx = noexcept(Function(std::declval<Fun&&>()))>
  Function& operator=(Fun fun) noexcept(Nx) {
    // Doing this in place is more efficient when we can do so safely.
    if (Nx) {
      // Q: Why is is safe to destroy and reconstruct this object in place?
      // A: See the explanation in the move assignment operator.
      this->~Function();
      ::new (this) Function(static_cast<Fun&&>(fun));
    } else {
      // Construct a temporary and (nothrow) swap.
      Function(static_cast<Fun&&>(fun)).swap(*this);
    }
    return *this;
  }

  template <
      typename Signature, typename...,
      typename = typename Traits::template IfSafeResult<Function<Signature>>>
  Function& operator=(Function<Signature>&& that) noexcept(
      noexcept(Function(std::move(that)))) {
    return (*this = Function(std::move(that)));
  }

  Function& operator=(std::nullptr_t) noexcept {
    return (*this = Function());
  }

  template <typename Member, typename Class>
  auto operator=(Member Class::*ptr) noexcept
      // Prevent this overload from being selected when `ptr` is not a
      // compatible member function pointer.
      -> decltype(operator=(std::mem_fn(ptr))) {
    return ptr ? (*this = std::mem_fn(ptr)) : (*this = Function());
  }

  using Traits::operator();

  void swap(Function& that) noexcept {
    std::swap(*this, that);
  }

  explicit operator bool() const noexcept {
    return exec_ != nullptr;
  }

  std::size_t heapAllocatedMemory() const noexcept {
    return exec(Op::HEAP, nullptr, nullptr);
  }

  using typename Traits::SharedProxy;

  SharedProxy asSharedProxy() && {
    return SharedProxy{std::move(*this)};
  }

  std::function<typename Traits::NonConstSignature> asStdFunction() && {
    return std::move(*this).asSharedProxy();
  }
};

template <typename FunctionType>
void swap(Function<FunctionType>& lhs, Function<FunctionType>& rhs) noexcept {
  lhs.swap(rhs);
}

template <typename FunctionType>
bool operator==(const Function<FunctionType>& fn, std::nullptr_t) {
  return !fn;
}

template <typename FunctionType>
bool operator==(std::nullptr_t, const Function<FunctionType>& fn) {
  return !fn;
}

template <typename FunctionType>
bool operator!=(const Function<FunctionType>& fn, std::nullptr_t) {
  return !(fn == nullptr);
}

template <typename FunctionType>
bool operator!=(std::nullptr_t, const Function<FunctionType>& fn) {
  return !(nullptr == fn);
}
template <typename ReturnType, typename... Args>
Function<ReturnType(Args...) const> constCastFunction(
    Function<ReturnType(Args...)>&& that) noexcept {
  return Function<ReturnType(Args...) const>{std::move(that),
                                             detail::function::CoerceTag{}};
}

template <typename ReturnType, typename... Args>
Function<ReturnType(Args...) const> constCastFunction(
    Function<ReturnType(Args...) const>&& that) noexcept {
  return std::move(that);
}

template <typename ReturnType, typename... Args>
Function<ReturnType(Args...) const noexcept> constCastFunction(
    Function<ReturnType(Args...) noexcept>&& that) noexcept {
  return Function<ReturnType(Args...) const noexcept>{
      std::move(that), detail::function::CoerceTag{}};
}

template <typename ReturnType, typename... Args>
Function<ReturnType(Args...) const noexcept> constCastFunction(
    Function<ReturnType(Args...) const noexcept>&& that) noexcept {
  return std::move(that);
}

template <typename FunctionType>
class FunctionRef;

template <typename ReturnType, typename... Args>
class FunctionRef<ReturnType(Args...)> final {
  template <typename Arg>
  using CallArg = detail::function::CallArg<Arg>;

  using Call = ReturnType (*)(CallArg<Args>..., void*);

  static ReturnType uninitCall(CallArg<Args>..., void*) {
    throw std::bad_function_call();  // ---------------------------- r1
    // throw_exception<std::bad_function_call>();
  }

  template <typename Fun,
            std::enable_if_t<!std::is_pointer<Fun>::value, int> = 0>
  static ReturnType call(CallArg<Args>... args, void* object) {
    using Pointer = std::add_pointer_t<Fun>;
    return static_cast<ReturnType>(
        invoke(static_cast<Fun&&>(*static_cast<Pointer>(object)),
               static_cast<Args&&>(args)...));
  }
  template <typename Fun,
            std::enable_if_t<std::is_pointer<Fun>::value, int> = 0>
  static ReturnType call(CallArg<Args>... args, void* object) {
    return static_cast<ReturnType>(
        invoke(reinterpret_cast<Fun>(object), static_cast<Args&&>(args)...));
  }

  void* object_{nullptr};
  Call call_{&FunctionRef::uninitCall};

 public:
  constexpr FunctionRef() = default;

  constexpr explicit FunctionRef(std::nullptr_t) noexcept {
  }

  template <typename Fun,
            std::enable_if_t<
                std::conjunction<
                    std::negation<std::is_same<FunctionRef, std::decay_t<Fun>>>,
                    std::is_invocable_r<ReturnType, Fun&&, Args&&...>>::value,
                int> = 0>
  constexpr /* implicit */ FunctionRef(Fun&& fun) noexcept {
    auto& ref = fun;
    if constexpr (detail::function::IsNullptrCompatible<
                      std::decay_t<Fun>>::value) {
      if (detail::function::isEmptyFunction(fun)) {
        return;
      }
    }
    auto ptr = std::addressof(ref);
    object_ = const_cast<void*>(static_cast<void const*>(ptr));
    call_ = &FunctionRef::template call<Fun>;
  }

  template <typename Fun,
            std::enable_if_t<std::is_function<Fun>::value, int> = 0,
            std::enable_if_t<std::is_invocable_r_v<ReturnType, Fun&, Args&&...>,
                             int> = 0>
  constexpr /* implicit */ FunctionRef(Fun* fun) noexcept {
    if (fun) {
      object_ = const_cast<void*>(reinterpret_cast<void const*>(fun));
      call_ = &FunctionRef::template call<Fun*>;
    }
  }

  ReturnType operator()(Args... args) const {
    return call_(static_cast<Args&&>(args)..., object_);
  }

  constexpr explicit operator bool() const noexcept {
    return object_;
  }

  constexpr friend bool operator==(FunctionRef<ReturnType(Args...)> ref,
                                   std::nullptr_t) noexcept {
    return ref.object_ == nullptr;
  }
  constexpr friend bool operator!=(FunctionRef<ReturnType(Args...)> ref,
                                   std::nullptr_t) noexcept {
    return ref.object_ != nullptr;
  }

  constexpr friend bool operator==(
      std::nullptr_t, FunctionRef<ReturnType(Args...)> ref) noexcept {
    return ref.object_ == nullptr;
  }
  constexpr friend bool operator!=(
      std::nullptr_t, FunctionRef<ReturnType(Args...)> ref) noexcept {
    return ref.object_ != nullptr;
  }
};

}  // namespace folly

#endif  // FAITHFUL_FUNCTION_H