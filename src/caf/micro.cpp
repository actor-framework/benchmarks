/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

// this file contains some micro benchmarks
// for various CAF implementation details

#include <chrono>
#include <cstdint>
#include <iostream>
#include <tuple>
#include <vector>

#include <benchmark/benchmark.h>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using std::cerr;
using std::cout;
using std::endl;
using std::vector;

namespace v2 {

template <class...>
struct msg_type_id;

template <class T>
struct get_msg_type_id;

template <class T, class R, class... Ts>
struct get_msg_type_id<R (T::*)(Ts...)> {
  static constexpr uint16_t value = msg_type_id<std::decay_t<Ts>...>::value;
};

template <class T, class R, class... Ts>
struct get_msg_type_id<R (T::*)(Ts...) const> {
  static constexpr uint16_t value = msg_type_id<std::decay_t<Ts>...>::value;
};

template <class... Ts>
struct get_msg_type_id<std::tuple<Ts...>> {
  static constexpr uint16_t value = msg_type_id<std::decay_t<Ts>...>::value;
};

template <class T>
constexpr uint16_t get_msg_type_id_v = get_msg_type_id<T>::value;

class message_data : public caf::ref_counted {
public:
  message_data(uint16_t id) : id_(id) {
    // nop
  }

  uint16_t type_id() const noexcept {
    return id_;
  }

  virtual message_data* copy() const = 0;

  virtual caf::error save(caf::serializer& sink) const = 0;

private:
  uint16_t id_;
};

template <class... Ts>
class message_data_impl : public message_data {
public:
  using super = message_data;

  using tuple_type = std::tuple<Ts...>;

  template <class... Us>
  message_data_impl(Us&&... xs)
    : super(msg_type_id<Ts...>::value), content_(std::forward<Us>(xs)...) {
    // nop
  }

  tuple_type& content() {
    return content_;
  }

  const tuple_type& content() const {
    return content_;
  }

  message_data_impl* copy() const override {
    return new message_data_impl(content_);
  }

  caf::error save(caf::serializer& sink) const override {
    return std::apply(sink, content_);
  }

  caf::error load(caf::deserializer& source) {
    return std::apply(source, content_);
  }

private:
  tuple_type content_;
};

template <class... Ts>
class typed_message {
public:
  using tuple_type = std::tuple<Ts...>;

  uint16_t type_id() const noexcept {
    return msg_type_id<tuple_type>::value;
  }

  auto& content() {
    return data_->content_;
  }

  constexpr operator bool() const {
    return static_cast<bool>(data_);
  }

private:
  caf::intrusive_cow_ptr<message_data_impl<Ts...>> data_;
};

class message {
public:
  using data_ptr = caf::intrusive_cow_ptr<message_data>;

  uint16_t type_id() const noexcept {
    return data_ ? data_->type_id() : 0;
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
    data_.reset(nullptr, add_ref);
  }

  caf::error save(caf::serializer& sink) {
    if (data_) {
      if (auto err = sink(type_id()))
        return err;
      return data_->save(sink);
    }
    uint16_t dummy = 0;
    if (auto err = sink(dummy))
      return err;
    return caf::none;
  }

  caf::error load(caf::deserializer& source);

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
    return std::apply(std::forward<T>(x), [](auto&&... ys) {
      return make_message(std::forward<decltype(ys)>(ys)...);
    });
  } else {
    using impl = message_data_impl<T, std::decay_t<Ts>...>;
    message::data_ptr ptr{
      new impl(std::forward<T>(x), std::forward<Ts>(xs)...)};
    return message{std::move(ptr)};
  }
}

template <class T>
bool holds_alternative(const message& x) noexcept {
  return x.type_id() == msg_type_id<T>::value;
}

template <class T>
struct fun_trait;

template <class R, class... Ts>
struct fun_trait<R(Ts...)> {
  using arg_types = std::tuple<std::decay_t<Ts>...>;
  using result_type = std::decay_t<R>;
  using message_data_type = message_data_impl<std::decay_t<Ts>...>;
};

template <class T, class R, class... Ts>
struct fun_trait<R (T::*)(Ts...)> : fun_trait<R(Ts...)> {};

template <class T, class R, class... Ts>
struct fun_trait<R (T::*)(Ts...) const> : fun_trait<R(Ts...)> {};

class behavior_impl : public caf::ref_counted {
public:
  virtual std::optional<message> invoke(message& msg) = 0;
};

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
      if (get_msg_type_id_v<typename trait::arg_types> == msg.type_id()) {
        auto& xs = static_cast<typename trait::message_data_type&>(msg.data());
        using fun_result = decltype(std::apply(fun, xs.content()));
        if constexpr (std::is_same_v<void, fun_result>) {
          std::apply(fun, xs.content());
          result = message{};
        } else {
          result = make_message(std::apply(fun, xs.content()));
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

struct serialization_info {
  uint16_t id;
  caf::error (*deserialize)(caf::deserializer&, message&);
};

struct serialization_info_list {
  size_t size;
  serialization_info* buf;
  ~serialization_info_list() {
    delete[] buf;
  }
};

serialization_info_list s_list;

size_t fill_type_registry(std::initializer_list<serialization_info> xs) {
  if (s_list.buf == nullptr) {
    s_list.size = xs.size();
    s_list.buf = new serialization_info[xs.size()];
    std::copy(xs.begin(), xs.end(), s_list.buf);
    return xs.size();
  }
  auto old_size = s_list.size;
  auto old_buf = s_list.buf;
  s_list.size += xs.size();
  s_list.buf = new serialization_info[s_list.size];
  std::copy(old_buf, old_buf + old_size, s_list.buf);
  std::copy(xs.begin(), xs.end(), s_list.buf + old_size);
  delete[] old_buf;
  return s_list.size;
}

const serialization_info& type_registry_entry(uint16_t id) {
  auto first = s_list.buf;
  auto last = first + s_list.size;
  return *std::find_if(first, last, [id](const auto& x) { return x.id == id; });
}

template <class... Ts>
constexpr serialization_info make_serialization_info(uint16_t id) {
  if constexpr ((caf::allowed_unsafe_message_type<Ts>::value || ...)) {
    return {id, nullptr};
  } else {
    return {id, [](caf::deserializer& source, message& x) -> caf::error {
              using impl_type = message_data_impl<Ts...>;
              if (x.type_id() != msg_type_id<Ts...>::value) {
                auto ptr = caf::make_counted<impl_type>();
                if (auto err = ptr->load(source))
                  return err;
                x.reset(ptr.release(), false);
                return caf::none;
              }
              return static_cast<impl_type&>(x.data()).load(source);
            }};
  }
}

caf::error message::load(caf::deserializer& source) {
  uint16_t id;
  if (auto err = source(id))
    return err;
  if (id == 0) {
    reset();
    return caf::none;
  }
  return type_registry_entry(id).deserialize(source, *this);
}

// -- message type IDs ---------------------------------------------------------

template <>
struct msg_type_id<size_t> {
  static constexpr uint16_t value = 99;
};

} // namespace v2

namespace v3 {

template <class>
struct type_id;

template <class T>
constexpr auto type_id_v = type_id<T>::value;

using rtti_t = const uint8_t*;

inline bool matches(rtti_t x, rtti_t y) noexcept {
  // We perform the bounds check implicitly when comparing the first byte.
  if(x==y)return true;
  return memcmp(x, y, x[0] + 1) == 0;
}

template <class... Ts>
struct msg_type_id {
  static_assert(sizeof...(Ts) <= 255);
  static constexpr uint8_t value[] = {static_cast<uint8_t>(sizeof...(Ts)),
                                      type_id_v<Ts>...};
};

template <class... Ts>
constexpr rtti_t msg_type_id_v = msg_type_id<Ts...>::value;

template <class T>
struct get_msg_type_id;

template <class T, class R, class... Ts>
struct get_msg_type_id<R (T::*)(Ts...)> {
  static constexpr auto value = msg_type_id_v<std::decay_t<Ts>...>;
};

template <class T, class R, class... Ts>
struct get_msg_type_id<R (T::*)(Ts...) const> {
  static constexpr auto value = msg_type_id_v<std::decay_t<Ts>...>;
};

template <class... Ts>
struct get_msg_type_id<std::tuple<Ts...>> {
  static constexpr auto value = msg_type_id_v<std::decay_t<Ts>...>;
};

template <class T>
constexpr auto get_msg_type_id_v = get_msg_type_id<T>::value;

class message_data : public caf::ref_counted {
public:
  explicit message_data(rtti_t id) : rtti_(id) {
    // nop
  }

  auto rtti() const noexcept {
    return rtti_;
  }

  virtual message_data* copy() const = 0;

  virtual caf::error save(caf::serializer& sink) const = 0;

private:
  rtti_t rtti_;
};

template <class... Ts>
class message_data_impl : public message_data {
public:
  using super = message_data;

  using tuple_type = std::tuple<Ts...>;

  template <class... Us>
  message_data_impl(Us&&... xs)
    : super(msg_type_id_v<Ts...>), content_(std::forward<Us>(xs)...) {
    // nop
  }

  tuple_type& content() {
    return content_;
  }

  const tuple_type& content() const {
    return content_;
  }

  message_data_impl* copy() const override {
    return new message_data_impl(content_);
  }

  caf::error save(caf::serializer& sink) const override {
    return std::apply(sink, content_);
  }

  caf::error load(caf::deserializer& source) {
    return std::apply(source, content_);
  }

private:
  tuple_type content_;
};

template <class... Ts>
class typed_message {
public:
  using tuple_type = std::tuple<Ts...>;

  uint16_t type_id() const noexcept {
    return msg_type_id<tuple_type>::value;
  }

  auto& content() {
    return data_->content_;
  }

  constexpr operator bool() const {
    return static_cast<bool>(data_);
  }

private:
  caf::intrusive_cow_ptr<message_data_impl<Ts...>> data_;
};

class message {
public:
  using data_ptr = caf::intrusive_cow_ptr<message_data>;

  /// @pre !empty()
  auto rtti() const noexcept {
    return data_->rtti();
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
    data_.reset(nullptr, add_ref);
  }

  caf::error save(caf::serializer& sink) {
    if (data_) {
      auto idv = data_->rtti();
      if (auto err = sink.apply_raw(idv[0], const_cast<uint8_t*>(idv + 1)))
        return err;
      return data_->save(sink);
    }
    uint16_t dummy = 0;
    if (auto err = sink(dummy))
      return err;
    return caf::none;
  }

  caf::error load(caf::deserializer& source);

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
    return std::apply(std::forward<T>(x), [](auto&&... ys) {
      return make_message(std::forward<decltype(ys)>(ys)...);
    });
  } else {
    using impl = message_data_impl<T, std::decay_t<Ts>...>;
    message::data_ptr ptr{
      new impl(std::forward<T>(x), std::forward<Ts>(xs)...)};
    return message{std::move(ptr)};
  }
}

template <class T>
bool holds_alternative(const message& x) noexcept {
  return matches(x.rtti(), get_msg_type_id_v<T>);
}

template <class T>
struct fun_trait;

template <class R, class... Ts>
struct fun_trait<R(Ts...)> {
  using arg_types = std::tuple<std::decay_t<Ts>...>;
  using result_type = std::decay_t<R>;
  using message_data_type = message_data_impl<std::decay_t<Ts>...>;
};

template <class T, class R, class... Ts>
struct fun_trait<R (T::*)(Ts...)> : fun_trait<R(Ts...)> {};

template <class T, class R, class... Ts>
struct fun_trait<R (T::*)(Ts...) const> : fun_trait<R(Ts...)> {};

class behavior_impl : public caf::ref_counted {
public:
  virtual std::optional<message> invoke(message& msg) = 0;
};

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
      if (matches(get_msg_type_id_v<typename trait::arg_types>, msg.rtti())) {
        auto& xs = static_cast<typename trait::message_data_type&>(msg.data());
        using fun_result = decltype(std::apply(fun, xs.content()));
        if constexpr (std::is_same_v<void, fun_result>) {
          std::apply(fun, xs.content());
          result = message{};
        } else {
          result = make_message(std::apply(fun, xs.content()));
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

struct serialization_info {
  rtti_t id;
  caf::error (*deserialize)(caf::deserializer&, message&);
};

struct serialization_info_list {
  size_t size;
  serialization_info* buf;
  ~serialization_info_list() {
    delete[] buf;
  }
};

serialization_info_list s_list;

size_t fill_type_registry(std::initializer_list<serialization_info> xs) {
  if (s_list.buf == nullptr) {
    s_list.size = xs.size();
    s_list.buf = new serialization_info[xs.size()];
    std::copy(xs.begin(), xs.end(), s_list.buf);
    return xs.size();
  }
  auto old_size = s_list.size;
  auto old_buf = s_list.buf;
  s_list.size += xs.size();
  s_list.buf = new serialization_info[s_list.size];
  std::copy(old_buf, old_buf + old_size, s_list.buf);
  std::copy(xs.begin(), xs.end(), s_list.buf + old_size);
  delete[] old_buf;
  return s_list.size;
}

const serialization_info& type_registry_entry(uint8_t* id) {
  auto first = s_list.buf;
  auto last = first + s_list.size;
  auto predicate = [id](const auto& x) { return matches(x.id, id); };
  return *std::find_if(first, last, predicate);
}

template <class... Ts>
constexpr serialization_info make_serialization_info() {
  auto id = msg_type_id_v<Ts...>;
  if constexpr ((caf::allowed_unsafe_message_type<Ts>::value || ...)) {
    return {id, nullptr};
  } else {
    return {id, [](caf::deserializer& source, message& x) -> caf::error {
              using impl_type = message_data_impl<Ts...>;
              if (!x || !matches(x.rtti(), msg_type_id_v<Ts...>)) {
                auto ptr = caf::make_counted<impl_type>();
                if (auto err = ptr->load(source))
                  return err;
                x.reset(ptr.release(), false);
                return caf::none;
              }
              return static_cast<impl_type&>(x.data()).load(source);
            }};
  }
}

caf::error message::load(caf::deserializer& source) {
  uint8_t id_size;
  if (auto err = source(id_size))
    return err;
  if (id_size == 0) {
    reset();
    return caf::none;
  }
  // Use a small stack buffer whenever possible to avoid heap allocations.
  if (id_size <= 16) {
    std::array<uint8_t, 16> id;
    id[0] = id_size;
    if (auto err = source.apply_raw(id_size, id.data() + 1))
      return err;
    return type_registry_entry(id.data()).deserialize(source, *this);
  }
  // std::vector fallback.
  std::vector<uint8_t> id(id_size + 1);
  id[0] = id_size;
  if (auto err = source.apply_raw(id_size, id.data() + 1))
    return err;
  return type_registry_entry(id.data()).deserialize(source, *this);
}

} // namespace v3


using namespace caf;

// -- constants and global state -----------------------------------------------

namespace {

constexpr size_t num_messages = 1000000;

size_t s_invoked = 0;

} // namespace

// -- benchmarking of message creation -----------------------------------------

void NativeMessageCreation(benchmark::State &state) {
  for (auto _ : state) {
    auto msg = make_message(size_t{0});
    benchmark::DoNotOptimize(msg);
  }
}

BENCHMARK(NativeMessageCreation);

void NativeMessageCreationV2(benchmark::State &state) {
  for (auto _ : state) {
    auto msg = v2::make_message(size_t{0});
    benchmark::DoNotOptimize(msg);
  }
}

BENCHMARK(NativeMessageCreationV2);

void NativeMessageCreationV3(benchmark::State &state) {
  for (auto _ : state) {
    auto msg = v3::make_message(size_t{0});
    benchmark::DoNotOptimize(msg);
  }
}

BENCHMARK(NativeMessageCreationV3);

// void DynamicMessageCreation(benchmark::State &state) {
//   for (auto _ : state) {
//     message_builder mb;
//     message msg = mb.append(size_t{0}).to_message();
//     benchmark::DoNotOptimize(msg);
//   }
// }
//
// BENCHMARK(DynamicMessageCreation);

// -- custom message type ------------------------------------------------------

struct foo {
  int a;
  int b;
};

inline bool operator==(const foo &lhs, const foo &rhs) {
  return lhs.a == rhs.a && lhs.b == rhs.b;
}

struct bar {
  foo a;
  std::string b;
};

inline bool operator==(const bar &lhs, const bar &rhs) {
  return lhs.a == rhs.a && lhs.b == rhs.b;
}

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(foo)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(bar)

#define ADD_V2_MSG_TYPE(id, ...)                                               \
  namespace v2 {                                                               \
  template <>                                                                  \
  struct msg_type_id<__VA_ARGS__> {                                            \
    static constexpr uint16_t value = id;                                      \
  };                                                                           \
  }

ADD_V2_MSG_TYPE(100, int)
ADD_V2_MSG_TYPE(101, int, int)
ADD_V2_MSG_TYPE(102, double)
ADD_V2_MSG_TYPE(103, double, double)
ADD_V2_MSG_TYPE(104, std::string)
ADD_V2_MSG_TYPE(105, std::string, std::string)
ADD_V2_MSG_TYPE(106, foo)
ADD_V2_MSG_TYPE(107, bar)

static size_t v2_dummy = v2::fill_type_registry({
  v2::make_serialization_info<int>(100),
  v2::make_serialization_info<int, int>(101),
  v2::make_serialization_info<double>(102),
  v2::make_serialization_info<double, double>(103),
  v2::make_serialization_info<std::string>(104),
  v2::make_serialization_info<std::string, std::string>(105),
});

#define ADD_V3_TYPE(id, type)                                                  \
  namespace v3 {                                                               \
  template <>                                                                  \
  struct type_id<type> {                                                       \
    static constexpr uint8_t value = id;                                       \
  };                                                                           \
  }

ADD_V3_TYPE(100, size_t)
ADD_V3_TYPE(101, int)
ADD_V3_TYPE(102, double)
ADD_V3_TYPE(103, std::string)
ADD_V3_TYPE(104, foo)
ADD_V3_TYPE(105, bar)

static size_t v3_dummy = v3::fill_type_registry({
  v3::make_serialization_info<int>(),
  v3::make_serialization_info<int, int>(),
  v3::make_serialization_info<double>(),
  v3::make_serialization_info<double, double>(),
  v3::make_serialization_info<std::string>(),
  v3::make_serialization_info<std::string, std::string>(),
});

// -- pattern matching benchmark -----------------------------------------------

template <class... Ts> message make_dynamic_message(Ts &&... xs) {
  message_builder mb;
  return mb.append_all(std::forward<Ts>(xs)...).to_message();
}

template <class... Ts> config_value cfg_lst(Ts &&... xs) {
  config_value::list lst{config_value{std::forward<Ts>(xs)}...};
  return config_value{std::move(lst)};
}

using namespace std::string_literals;

struct Messages : benchmark::Fixture {

  message native_two_ints = make_message(1, 2);
  message native_two_doubles = make_message(1.0, 2.0);
  message native_two_strings = make_message("hi", "there");
  message native_one_foo = make_message(foo{1, 2});
  message native_one_bar = make_message(bar{foo{1, 2}});

  v2::message v2_native_two_ints = v2::make_message(1, 2);
  v2::message v2_native_two_doubles = v2::make_message(1.0, 2.0);
  v2::message v2_native_two_strings = v2::make_message("hi"s, "there"s);
  v2::message v2_native_one_foo = v2::make_message(foo{1, 2});
  v2::message v2_native_one_bar = v2::make_message(bar{foo{1, 2}});

  v3::message v3_native_two_ints = v3::make_message(1, 2);
  v3::message v3_native_two_doubles = v3::make_message(1.0, 2.0);
  v3::message v3_native_two_strings = v3::make_message("hi"s, "there"s);
  v3::message v3_native_one_foo = v3::make_message(foo{1, 2});
  v3::message v3_native_one_bar = v3::make_message(bar{foo{1, 2}});

  message dynamic_two_ints = make_dynamic_message(1, 2);
  message dynamic_two_doubles = make_dynamic_message(1.0, 2.0);
  message dynamic_two_strings = make_dynamic_message("hi", "there");
  message dynamic_one_foo = make_dynamic_message(foo{1, 2});
  message dynamic_one_bar = make_dynamic_message(bar{foo{1, 2}});

  /// A message featuring a recursive data type (config_value).
  message recursive;

  std::vector<char> native_two_strings_serialized;

  std::vector<char> v2_native_two_strings_serialized;

  std::vector<char> v3_native_two_strings_serialized;

  /// The serialized representation of `recursive` from the binary serializer.
  std::vector<char> recursive_binary_serialized;

  /// The serialized representation of `recursive` from the stream serializer.
  std::vector<char> recursive_stream_serialized;

  actor_system_config cfg;

  actor_system sys;

  Messages() : sys(cfg.set("scheduler.policy", atom("testing"))) {
    // config_value::dictionary dict;
    // put(dict, "scheduler.policy", atom("none"));
    // put(dict, "scheduler.max-threads", 42);
    // put(dict, "nodes.preload",
    //     cfg_lst("sun", "venus", "mercury", "earth", "mars"));
    // recursive = make_message(config_value{std::move(dict)});
    {
      stream_serializer<vectorbuf> sink{sys, native_two_strings_serialized};
      native_two_strings.save(sink);
    }
    {
      stream_serializer<vectorbuf> sink{sys, v2_native_two_strings_serialized};
      v2_native_two_strings.save(sink);
    }
    {
      stream_serializer<vectorbuf> sink{sys, v3_native_two_strings_serialized};
      v3_native_two_strings.save(sink);
    }
    {
      binary_serializer sink{sys, recursive_binary_serialized};
      inspect(sink, recursive);
    }
    {
      stream_serializer<vectorbuf> sink{sys, recursive_stream_serialized};
      inspect(sink, recursive);
    }
  }

  behavior bhvr{
    [&](int) { s_invoked = 1; },
    [&](int, int) { s_invoked = 2; },
    [&](double) { s_invoked = 3; },
    [&](double, double) { s_invoked = 4; },
    [&](const std::string&) { s_invoked = 5; },
    [&](const std::string&, const std::string&) { s_invoked = 6; },
    [&](const foo&) { s_invoked = 7; },
    [&](const bar&) { s_invoked = 8; },
  };

  v2::behavior v2_bhvr{
    [&](int) { s_invoked = 1; },
    [&](int, int) { s_invoked = 2; },
    [&](double) { s_invoked = 3; },
    [&](double, double) { s_invoked = 4; },
    [&](const std::string&) { s_invoked = 5; },
    [&](const std::string&, const std::string&) { s_invoked = 6; },
    [&](const foo&) { s_invoked = 7; },
    [&](const bar&) { s_invoked = 8; },
  };

  v3::behavior v3_bhvr{
    [&](int) { s_invoked = 1; },
    [&](int, int) { s_invoked = 2; },
    [&](double) { s_invoked = 3; },
    [&](double, double) { s_invoked = 4; },
    [&](const std::string&) { s_invoked = 5; },
    [&](const std::string&, const std::string&) { s_invoked = 6; },
    [&](const foo&) { s_invoked = 7; },
    [&](const bar&) { s_invoked = 8; },
  };

  bool match(benchmark::State& state, message& msg,
             size_t expected_handler_id) {
    s_invoked = 0;
    bhvr(msg);
    if (s_invoked != expected_handler_id) {
      state.SkipWithError("Wrong handler called!");
      return false;
    }
    return true;
  }

  bool match(benchmark::State& state, v2::message& msg,
             size_t expected_handler_id) {
    s_invoked = 0;
    v2_bhvr(msg);
    if (s_invoked != expected_handler_id) {
      printf("UH OH: invoked %d instead of %d!\n", (int) s_invoked,
             (int) expected_handler_id);
      state.SkipWithError("Wrong handler called!");
      return false;
    }
    return true;
  }

  bool match(benchmark::State& state, v3::message& msg,
             size_t expected_handler_id) {
    s_invoked = 0;
    v3_bhvr(msg);
    if (s_invoked != expected_handler_id) {
      printf("UH OH: invoked %d instead of %d!\n", (int) s_invoked,
             (int) expected_handler_id);
      state.SkipWithError("Wrong handler called!");
      return false;
    }
    return true;
  }
};

BENCHMARK_DEFINE_F(Messages, MatchNative)(benchmark::State &state) {
  for (auto _ : state) {
    if (!match(state, native_two_ints, 2)
        || !match(state, native_two_doubles, 4)
        || !match(state, native_two_strings, 6)
        || !match(state, native_one_foo, 7) || !match(state, native_one_bar, 8))
      break;
  }
}

BENCHMARK_REGISTER_F(Messages, MatchNative);

BENCHMARK_DEFINE_F(Messages, MatchNativeV2)(benchmark::State &state) {
  for (auto _ : state) {
    if (!match(state, v2_native_two_ints, 2)
        || !match(state, v2_native_two_doubles, 4)
        || !match(state, v2_native_two_strings, 6)
        || !match(state, v2_native_one_foo, 7)
        || !match(state, v2_native_one_bar, 8))
      break;
  }
}

BENCHMARK_REGISTER_F(Messages, MatchNativeV2);

BENCHMARK_DEFINE_F(Messages, MatchNativeV3)(benchmark::State &state) {
  for (auto _ : state) {
    if (!match(state, v3_native_two_ints, 2)
        || !match(state, v3_native_two_doubles, 4)
        || !match(state, v3_native_two_strings, 6)
        || !match(state, v3_native_one_foo, 7)
        || !match(state, v3_native_one_bar, 8))
      break;
  }
}

BENCHMARK_REGISTER_F(Messages, MatchNativeV3);

// BENCHMARK_DEFINE_F(Messages, MatchDynamic)(benchmark::State &state) {
//   for (auto _ : state) {
//     if (!match(state, dynamic_two_ints, 2)
//         || !match(state, dynamic_two_doubles, 4)
//         || !match(state, dynamic_two_strings, 6)
//         || !match(state, dynamic_one_foo, 7)
//         || !match(state, dynamic_one_bar, 8))
//       break;
//   }
// }
//
// BENCHMARK_REGISTER_F(Messages, MatchDynamic);

// -- serialization of simple string messages ----------------------------------

BENCHMARK_DEFINE_F(Messages, SerializeStringMessage)(benchmark::State &state) {
  for (auto _ : state) {
    std::vector<char> buf;
    buf.reserve(512);
    binary_serializer bs{sys, buf};
    native_two_strings.save(bs);
    benchmark::DoNotOptimize(buf);
  }
}

BENCHMARK_REGISTER_F(Messages, SerializeStringMessage);

BENCHMARK_DEFINE_F(Messages, SerializeStringMessageV2)(benchmark::State &state) {
  for (auto _ : state) {
    std::vector<char> buf;
    buf.reserve(512);
    binary_serializer bs{sys, buf};
    v2_native_two_strings.save(bs);
    benchmark::DoNotOptimize(buf);
  }
}

BENCHMARK_REGISTER_F(Messages, SerializeStringMessageV2);

BENCHMARK_DEFINE_F(Messages, SerializeStringMessageV3)(benchmark::State &state) {
  for (auto _ : state) {
    std::vector<char> buf;
    buf.reserve(512);
    binary_serializer bs{sys, buf};
    v3_native_two_strings.save(bs);
    benchmark::DoNotOptimize(buf);
  }
}

BENCHMARK_REGISTER_F(Messages, SerializeStringMessageV3);

BENCHMARK_DEFINE_F(Messages, DeserializeStringMessage)(benchmark::State &state) {
  for (auto _ : state) {
    message msg;
    binary_deserializer bs{sys, native_two_strings_serialized};
    msg.load(bs);
    benchmark::DoNotOptimize(msg);
  }
}

BENCHMARK_REGISTER_F(Messages, DeserializeStringMessage);

BENCHMARK_DEFINE_F(Messages, DeserializeStringMessageV2)(benchmark::State &state) {
  for (auto _ : state) {
    v2::message msg;
    binary_deserializer bs{sys, v2_native_two_strings_serialized};
    msg.load(bs);
    benchmark::DoNotOptimize(msg);
  }
}

BENCHMARK_REGISTER_F(Messages, DeserializeStringMessageV2);

BENCHMARK_DEFINE_F(Messages, DeserializeStringMessageV3)(benchmark::State &state) {
  for (auto _ : state) {
    v3::message msg;
    binary_deserializer bs{sys, v3_native_two_strings_serialized};
    msg.load(bs);
    benchmark::DoNotOptimize(msg);
  }
}

BENCHMARK_REGISTER_F(Messages, DeserializeStringMessageV3);

// -- serialization of recursive data ------------------------------------------

// BENCHMARK_DEFINE_F(Messages, BinarySerializer)(benchmark::State &state) {
//   for (auto _ : state) {
//     std::vector<char> buf;
//     buf.reserve(512);
//     binary_serializer bs{sys, buf};
//     inspect(bs, recursive);
//     benchmark::DoNotOptimize(buf);
//   }
// }
//
// BENCHMARK_REGISTER_F(Messages, BinarySerializer);
//
// BENCHMARK_DEFINE_F(Messages, StreamSerializer)(benchmark::State &state) {
//   for (auto _ : state) {
//     std::vector<char> buf;
//     buf.reserve(512);
//     stream_serializer<vectorbuf> bs{sys, buf};
//     inspect(bs, recursive);
//     benchmark::DoNotOptimize(buf);
//   }
// }
//
// BENCHMARK_REGISTER_F(Messages, StreamSerializer);
//
// BENCHMARK_DEFINE_F(Messages, BinaryDeserializer)(benchmark::State &state) {
//   for (auto _ : state) {
//     message result;
//     binary_deserializer source{sys, recursive_binary_serialized};
//     inspect(source, result);
//     benchmark::DoNotOptimize(result);
//   }
// }
//
// BENCHMARK_REGISTER_F(Messages, BinaryDeserializer);
//
// BENCHMARK_DEFINE_F(Messages, StreamDeserializer)(benchmark::State &state) {
//   for (auto _ : state) {
//     message result;
//     stream_deserializer<charbuf> source{sys, recursive_stream_serialized};
//     inspect(source, result);
//     benchmark::DoNotOptimize(result);
//   }
// }
//
// BENCHMARK_REGISTER_F(Messages, StreamDeserializer);

BENCHMARK_MAIN();
