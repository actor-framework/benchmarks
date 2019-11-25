#include <cstdint>
#include <cstring>
#include <tuple>

#include "caf/deserializer.hpp"
#include "caf/detail/int_list.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/ref_counted.hpp"
#include "caf/serializer.hpp"
#include "caf/unifyn.hpp"

namespace v4 {

template <class>
struct type_id;

template <class T>
constexpr uint16_t type_id_v = type_id<T>::value;

template <uint16_t>
struct type_by_id;

template <uint16_t I>
using type_by_id_t = typename type_by_id<I>::type;

using rtti_t = const uint16_t*;

class message_data;

struct message_data_vtable {
  caf::intrusive_cow_ptr<message_data> (*copy)(const message_data&);

  void (*destroy)(message_data&) noexcept;

  caf::error (*save)(caf::serializer& sink, const message_data&);

  caf::error (*load)(caf::deserializer& source, message_data&);
};

class message_builder;

template <class... Ts>
caf::intrusive_cow_ptr<message_data> make_message_data(Ts&&... xs);

template <size_t Offset, class... Ts>
struct message_data_helper;

class message_data {
public:
  message_data(rtti_t id, const message_data_vtable* vtable)
    : rc_(1), rtti_(id), vtable_(vtable) {
    // nop
  }

  message_data(const message_data& other) : rc_(1), rtti_(other.rtti_) {
    // nop
  }


  ~message_data() noexcept {
    vtable_->destroy(*this);
  }

  auto rtti() const noexcept {
    return rtti_;
  }

  /// Increases reference count by one.
  void ref() const noexcept {
    rc_.fetch_add(1, std::memory_order_relaxed);
  }

  /// Decreases reference count by one and calls `request_deletion`
  /// when it drops to zero.
  void deref() noexcept {
    if (unique() || rc_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      this->~message_data();
      free(const_cast<message_data*>(this));
    }
  }

  /// Queries whether there is exactly one reference.
  bool unique() const noexcept {
    return rc_ == 1;
  }

  std::byte* storage() noexcept {
    return storage_;
  }

  const std::byte* storage() const noexcept {
    return storage_;
  }

  message_data* copy() const {
    throw std::logic_error("not implement");
  }

  caf::error save(caf::serializer& sink) const {
    return vtable_->save(sink, *this);
  }

  caf::error load(caf::deserializer& source) {
    return vtable_->load(source, *this);
  }

private:
  mutable std::atomic<size_t> rc_;
  rtti_t rtti_;
  const message_data_vtable* vtable_;

protected:
  std::byte storage_[];
};


/// @relates ref_counted
inline void intrusive_ptr_add_ref(const message_data* p) {
  p->ref();
}

/// @relates ref_counted
inline void intrusive_ptr_release(message_data* p) {
  p->deref();
}

template <class T>
constexpr size_t padded_size_v = ((sizeof(T) / alignof(std::max_align_t))
                                  + static_cast<size_t>(
                                    sizeof(T) % alignof(std::max_align_t) != 0))
                                 * alignof(std::max_align_t);

template <size_t Offset, class... Ts>
struct message_data_helper;

template <size_t Offset>
struct message_data_helper<Offset> {
  static void init(std::byte*) {
    // End of recursion.
  }

  static void copy_construct(std::byte*, const std::byte*) {
    // End of recursion.
  }

  static void destroy(std::byte*) noexcept {
    // End of recursion.
  }

  static caf::error save(caf::serializer&, const std::byte*) {
    return caf::none;
  }

  static caf::error load(caf::deserializer&, std::byte*) {
    return caf::none;
  }
};

template <size_t Offset, class T, class... Ts>
struct message_data_helper<Offset, T, Ts...> {
  using next = message_data_helper<Offset + padded_size_v<T>, Ts...>;

  template <class U, class... Us>
  static void init(std::byte* storage, U&& x, Us&&... xs) {
    if constexpr (!std::is_empty_v<T>)
      new (storage + Offset) T(std::forward<U>(x));
    // TODO: exception safety: if next::init throws than we need to unwind the
    //       stack here and call the destructor of T.
    next::init(storage, std::forward<Us>(xs)...);
  }

  static void copy_construct(std::byte* storage, const std::byte* source) {
    if constexpr (!std::is_empty_v<T>)
      new (storage + Offset) T(*reinterpret_cast<const T*>(source + Offset));
    next::copy_construct(storage, source);
  }

  static void destroy(std::byte* storage) noexcept {
    if constexpr (!std::is_trivially_destructible_v<T>)
      reinterpret_cast<T*>(storage + Offset)->~T();
    next::destroy(storage);
  }

  static caf::error save(caf::serializer& sink, const std::byte* storage) {
    if constexpr (!std::is_empty_v<T>)
      if (auto err = sink(*reinterpret_cast<const T*>(storage + Offset)))
        return err;
    return next::save(sink, storage);
  }

  static caf::error load(caf::deserializer& source, std::byte* storage) {
    if constexpr (!std::is_empty_v<T>)
      if (auto err = source(*reinterpret_cast<T*>(storage + Offset)))
        return err;
    return next::load(source, storage);
  }
};

template <class... Ts>
constexpr uint16_t default_rtti[] = {sizeof...(Ts), type_id_v<Ts>...};

template <class T>
struct get_default_rtti;

template <class... Ts>
struct get_default_rtti<caf::detail::type_list<Ts...>> {
  static constexpr rtti_t value = default_rtti<Ts...>;
};

template <class T>
constexpr rtti_t get_default_rtti_v = get_default_rtti<T>::value;

template <class... Ts>
const message_data_vtable* default_message_data_vtable() {
  using helper = message_data_helper<0, Ts...>;
  static constexpr message_data_vtable vtable{
    [](const message_data& x) {
      auto vptr = malloc(sizeof(message_data) + (padded_size_v<Ts> + ...));
      auto vtbl = default_message_data_vtable<Ts...>();
      auto ptr = new (vptr) message_data(default_rtti<Ts...>, vtbl);
      helper::copy_construct(ptr->storage(), x.storage());
      return caf::intrusive_cow_ptr<message_data>{ptr, false};
    },
    [](message_data& x) noexcept { helper::destroy(x.storage()); },
    [](caf::serializer& sink, const message_data& x) {
      return helper::save(sink, x.storage());
    },
    [](caf::deserializer& source, message_data& x) {
      return helper::load(source, x.storage());
    },

  };
  return &vtable;
}

template <class... Ts>
caf::intrusive_cow_ptr<message_data> make_message_data(Ts&&... xs) {
  static_assert(!(std::is_pointer_v<Ts> && ...));
  using helper = message_data_helper<0, std::decay_t<Ts>...>;
  auto vptr = malloc(sizeof(message_data)
                     + (padded_size_v<std::decay_t<Ts>> + ...));
  auto vtbl = default_message_data_vtable<std::decay_t<Ts>...>();
  auto ptr = new (vptr) message_data(default_rtti<Ts...>, vtbl);
  helper::init(ptr->storage(), std::forward<Ts>(xs)...);
  return caf::intrusive_cow_ptr<message_data>{ptr, false};
}

template <class... Ts>
struct typed_message_data_view {
  message_data& data;

  template <class T>
  explicit typed_message_data_view(T& from) : data(from.data()) {
    // nop
  }

  explicit typed_message_data_view(message_data& from) : data(from) {
    // nop
  }
};

template <class... Ts>
struct const_typed_message_data_view {
  const message_data& data;

  template <class T>
  explicit const_typed_message_data_view(const T& from) : data(from.cdata()) {
    // nop
  }

  explicit const_typed_message_data_view(const message_data& from) : data(from) {
    // nop
  }
};

template <size_t Remaining, class T, class... Ts>
struct offset_at{
  static constexpr size_t value = offset_at<Remaining - 1, Ts...>::value
                                  + padded_size_v<T>;
};

template <class T, class... Ts>
struct offset_at<0, T, Ts...> {
  static constexpr size_t value = 0;
};

template <size_t Index, class... Ts>
auto& get(typed_message_data_view<Ts...> x) {
  using type = caf::detail::tl_at_t<caf::detail::type_list<Ts...>, Index>;
  return *reinterpret_cast<type*>(x.data.storage()
                                  + offset_at<Index, Ts...>::value);
}

template <size_t Index, class... Ts>
const auto& get(const_typed_message_data_view<Ts...> x) {
  using type = caf::detail::tl_at_t<caf::detail::type_list<Ts...>, Index>;
  return *reinterpret_cast<const type*>(x.data.storage()
                                        + offset_at<Index, Ts...>::value);
}

class message_builder_element {
public:
  virtual ~message_builder_element() noexcept {
    // nop
  }

  virtual std::byte* init(std::byte* storage) const = 0;

  virtual std::byte* move_init(std::byte* storage) = 0;
};

template <class T>
class message_builder_element_impl : public message_builder_element {
public:
  message_builder_element_impl(T value) : value_(std::move(value)) {
    // nop
  }

  ~message_builder_element_impl() noexcept override {
    // nop
  }

  std::byte* init(std::byte* storage) const override {
    new (storage) T(value_);
    return storage + padded_size_v<T>;
  }

  std::byte* move_init(std::byte* storage) override {
    new (storage) T(std::move(value_));
    return storage + padded_size_v<T>;
  }

private:
  T value_;
};

struct meta_object {
  size_t size;
  caf::error (*save)(caf::serializer&, const std::byte*);
  caf::error (*load)(caf::deserializer&, std::byte*);
  void (*destroy)(std::byte*) noexcept;
  void (*default_construct)(std::byte*);
};

template <class T>
auto make_meta_object() {
  return meta_object{
    padded_size_v<T>,
    [](caf::serializer& sink, const std::byte* ptr) {
      return sink(*reinterpret_cast<const T*>(ptr));
    },
    [](caf::deserializer& source, std::byte* ptr) {
      return source(*reinterpret_cast<T*>(ptr));
    },
    [](std::byte* ptr) noexcept { reinterpret_cast<T*>(ptr)->~T(); },
    [](std::byte* ptr) { new (ptr) T(); },
  };
}

std::vector<meta_object> meta_objects;

std::vector<std::vector<uint16_t>> rtti_cache;

rtti_t get_rtti(const std::vector<uint16_t>& x) {
  auto i = std::find(rtti_cache.begin(), rtti_cache.end(), x);
  if (i == rtti_cache.end())
    i = rtti_cache.emplace(rtti_cache.end(), x);
  return i->data();
}

struct dynamic_message_data_vtable {
  static caf::intrusive_cow_ptr<message_data> copy(const message_data&) {
    throw std::logic_error("copy not implemented yet");
  }

  static void destroy(message_data& x) noexcept {
    auto rtti = x.rtti();
    auto ptr = x.storage();
    for (size_t i = 1; i <= rtti[0]; ++i) {
      auto& meta = meta_objects[rtti[i]];
      meta.destroy(ptr);
      ptr += meta.size;
    }
  }

  static caf::error save(caf::serializer& sink, const message_data& x) {
    auto rtti = x.rtti();
    auto ptr = x.storage();
    for (size_t i = 1; i <= rtti[0]; ++i) {
      auto& meta = meta_objects[rtti[i]];
      if (auto err = meta.save(sink, ptr))
        return err;
      ptr += meta.size;
    }
    return caf::none;
  }

  static caf::error load(caf::deserializer& source, message_data& x) {
    auto rtti = x.rtti();
    auto ptr = x.storage();
    for (size_t i = 1; i <= rtti[0]; ++i) {
      auto& meta = meta_objects[rtti[i]];
      if (auto err = meta.load(source, ptr))
        return err;
      ptr += meta.size;
    }
    return caf::none;
  }
};

message_data_vtable dynamic_vtable{
  dynamic_message_data_vtable::copy,
  dynamic_message_data_vtable::destroy,
  dynamic_message_data_vtable::save,
  dynamic_message_data_vtable::load,
};

class message_builder {
public:
  message_builder() {
    rtti_.emplace_back(0);
  }

  caf::intrusive_cow_ptr<message_data> make() {
    auto vptr = malloc(sizeof(message_data) + data_size_);
    auto ptr = new (vptr) message_data(get_rtti(rtti_), &dynamic_vtable);
    auto pos = ptr->storage();
    for (auto& elements : elements_)
      pos = elements->init(pos);
    return caf::intrusive_cow_ptr<message_data>{ptr, false};
  }

  template <class T>
  message_builder& append(T x) {
    elements_.emplace_back(new message_builder_element_impl<T>(std::move(x)));
    rtti_.emplace_back(type_id_v<T>);
    rtti_[0] += 1;
    data_size_ += padded_size_v<T>;
    return *this;
  }

private:
  size_t data_size_ = 0;
  std::vector<uint16_t> rtti_;
  std::vector<std::unique_ptr<message_builder_element>> elements_;
};

class message {
public:
  using data_ptr = caf::intrusive_cow_ptr<message_data>;

  /// @pre !empty()
  auto rtti() const noexcept {
    return data_->rtti();
  }

  size_t size() const noexcept {
    return data_ ? data_->rtti()[0] : 0;
  }

  message_data& data() {
    return data_.unshared();
  }

  const message_data& data() const {
    return *data_;
  }

  const message_data& cdata() const {
    return *data_;
  }

  constexpr operator bool() const {
    return static_cast<bool>(data_);
  }

  constexpr bool operator!() const {
    return !data_;
  }

  message() noexcept = default;

  explicit message(data_ptr data) noexcept : data_(std::move(data)) {
    // nop
  }

  void reset(message_data* ptr = nullptr, bool add_ref = true) noexcept {
    data_.reset(ptr, add_ref);
  }

  caf::error save(caf::serializer& sink) {
    if (data_) {
      auto idv = data_->rtti();
      size_t id_size = idv[0];
      for (size_t i = 0; i <= id_size; ++i)
        if (auto err = sink.apply(const_cast<uint16_t&>(idv[i])))
          return err;
      return data_->save(sink);
    }
    uint16_t dummy = 0;
    if (auto err = sink(dummy))
      return err;
    return caf::none;
  }

  caf::error load(caf::deserializer& source) {
    uint16_t ids_size = 0;
    if (auto err = source.apply(ids_size))
      return err;
    if (ids_size == 0) {
      data_.reset();
      return caf::none;
    }
    std::vector<uint16_t> ids(ids_size + 1);
    ids[0] = ids_size;
    for (auto i = 1; i <= ids_size; ++i)
      if (auto err = source.apply(ids[i]))
        return err;
    size_t data_size = 0;
    for (auto i = 1; i <= ids_size; ++i)
      data_size += meta_objects[ids[i]].size;
    auto vptr = malloc(sizeof(message_data) + data_size);
    auto ptr = new (vptr) message_data(get_rtti(ids), &dynamic_vtable);
    auto pos = ptr->storage();
    for (auto i = 1; i <= ids_size; ++i) {
      auto& meta = meta_objects[ids[i]];
      meta.default_construct(pos);
      if (auto err = meta.load(source, pos)) {
        auto rpos = pos;
        for (auto j = i; j > 0; --j) {
          auto& jmeta = meta_objects[ids[j]];
          jmeta.destroy(rpos);
          rpos -= jmeta.size;
        }
        ptr->~message_data();
        free(vptr);
        return err;
      }
      pos += meta.size;
    }
    data_.reset(ptr, false);
    return caf::none;
  }

private:
  caf::intrusive_cow_ptr<message_data> data_;
};

template <class T>
struct is_tuple : std::false_type {};

template <class... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type {};

template <class T>
constexpr bool is_tuple_v = is_tuple<T>::value;

template <class T, class... Ts>
message make_message(T&& x, Ts&&... xs) {
  if constexpr (sizeof...(Ts) == 0 && is_tuple_v<std::decay_t<T>>) {
    auto f = [](auto&&... ys) {
      return make_message(std::forward<decltype(ys)>(ys)...);
    };
    return std::apply(f,std::forward<T>(x));
  } else {
    return message{
      make_message_data(std::forward<T>(x), std::forward<Ts>(xs)...)};
  }
}

inline bool matches(rtti_t x, rtti_t y) noexcept {
  return x == y || memcmp(x, y, sizeof(uint16_t) * (x[0] + 1)) == 0;
}

template <class... Ts>
bool matches(const message& x) noexcept {
  return matches(x.rtti(), default_rtti<Ts...>);
}

template <class T>
struct fun_trait;

template <class R, class... Ts>
struct fun_trait<R(Ts...)> {
  using arg_types = caf::detail::type_list<std::decay_t<Ts>...>;
  using result_type = std::decay_t<R>;
  using message_view_type = const_typed_message_data_view<std::decay_t<Ts>...>;
};

template <class T, class R, class... Ts>
struct fun_trait<R (T::*)(Ts...)> : fun_trait<R(Ts...)> {};

template <class T, class R, class... Ts>
struct fun_trait<R (T::*)(Ts...) const> : fun_trait<R(Ts...)> {};

class behavior_impl : public caf::ref_counted {
public:
  virtual std::optional<message> invoke(message& msg) = 0;
};

template <class F, class View, size_t... I>
constexpr decltype(auto) apply_impl(F&& f, View x, std::index_sequence<I...>) {
  return std::invoke(std::forward<F>(f), get<I>(x)...);
}

template <class F, class... Ts>
constexpr decltype(auto) apply(F&& f, const_typed_message_data_view<Ts...> x) {
  return apply_impl(std::forward<F>(f), x,
                    std::make_index_sequence<sizeof...(Ts)>{});
}

template <class... Fs>
class default_behavior_impl : public behavior_impl {
public:
  std::optional<message> invoke(message& msg) override {
    return invoke_impl(msg, std::make_index_sequence<sizeof...(Fs)>{});
  }

  default_behavior_impl(Fs... fs) : fs_(std::move(fs)...) {
    // nop
  }

private:
  template <size_t... Is>
  std::optional<message> invoke_impl(message& msg, std::index_sequence<Is...>) {
    std::optional<message> result;
    auto dispatch = [&](auto& fun) {
      using fun_type = std::decay_t<decltype(fun)>;
      using trait = fun_trait<decltype(&fun_type::operator())>;
      if (matches(get_default_rtti_v<typename trait::arg_types>, msg.rtti())) {
        typename trait::message_view_type xs{msg};
        using fun_result = decltype(apply(fun, xs));
        if constexpr (std::is_same_v<void, fun_result>) {
          apply(fun, xs);
          result = message{};
        } else {
          result = make_message(apply(fun, xs));
        }
        return true;
      }
      return false;
    };
    if ((dispatch(std::get<Is>(fs_)) || ...))
      return result;
    return std::nullopt;
  }

  std::tuple<Fs...> fs_;
};

class behavior {
public:
  template <class F, class = std::enable_if_t<!std::is_same_v<F, behavior>>,
            class... Fs>
  behavior(F f, Fs&&... fs)
    : impl_(new default_behavior_impl<F, Fs...>(std::move(f),
                                                std::forward<Fs>(fs)...)) {
    // nop
  }
  behavior() = default;

  behavior(behavior&&) = default;

  behavior(const behavior&) = default;

  behavior& operator=(behavior&&) = default;

  behavior& operator=(const behavior&) = default;

  std::optional<message> operator()(message& msg) {
    if (!msg)
      abort();
    return impl_->invoke(msg);
  }

private:
  caf::intrusive_ptr<behavior_impl> impl_;
};

} // namespace v4

#define CAF_BEGIN_MSG_TYPES(project_name)                                      \
  constexpr uint16_t project_name##_counter_init = __COUNTER__;

#define CAF_ADD_MSG_TYPE(project_name, type_name)                              \
  namespace v4 {                                                               \
  template <>                                                                  \
  struct type_id<type_name> {                                                  \
    static constexpr uint16_t value = __COUNTER__                              \
                                      - project_name##_counter_init - 1;       \
  };                                                                           \
  template <>                                                                  \
  struct type_by_id<type_id<type_name>::value> {                               \
    using type = type_name;                                                    \
  };                                                                           \
  }

#define CAF_END_MSG_TYPES(project_name)                                        \
  template <long... Is>                                                        \
  void project_name##_announce_message_types(caf::detail::int_list<Is...>) {   \
    using namespace v4;                                                        \
    meta_objects = std::vector<meta_object>{                                   \
      make_meta_object<typename type_by_id<Is>::type>()...};                   \
  }                                                                            \
  inline void project_name##_announce_message_types() {                        \
    using namespace v4;                                                        \
    project_name##_announce_message_types(                                     \
      typename caf::detail::il_range<                                          \
        0, __COUNTER__ - project_name##_counter_init - 1>::type{});            \
  }
