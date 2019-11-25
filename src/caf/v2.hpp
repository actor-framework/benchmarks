#include <cstdint>
#include <cstring>

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
    message::data_ptr ptr{new impl(std::forward<T>(x), std::forward<Ts>(xs)...),
                          false};
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

#define ADD_V2_MSG_TYPE(id, ...)                                               \
  namespace v2 {                                                               \
  template <>                                                                  \
  struct msg_type_id<__VA_ARGS__> {                                            \
    static constexpr uint16_t value = id;                                      \
  };                                                                           \
  }

