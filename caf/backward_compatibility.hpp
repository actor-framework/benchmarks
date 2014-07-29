#ifndef BACKWARD_COMPATIBILITY_HPP
#define BACKWARD_COMPATIBILITY_HPP

#include "cppa/cppa.hpp"

/*
 * This header allows libcppa's benchmark programs to compile with
 * older versions (i.e. < 0.9) of the library. It provides macros such as
 * SELF_ARG and SELF_PREFIX to capture and use the 'self' pointer as requested
 * by releases >= 0.9, provides a typedef 'actor_hdl' to address for the
 * 'actor_ptr' => 'actor' change and ports some functions newly introduced
 * in 0.9 to older releases.
 */

#ifndef CPPA_VERSION
// first defined in libcppa version 0.9:
// if not defined, we are dealing with libcppa <= 0.8 releases

namespace { cppa::actor_ptr invalid_actor = nullptr; }
typedef cppa::actor_ptr actor_hdl;
inline void await_all_actors_done() {
    cppa::await_all_others_done();
}
template<typename... Ts>
inline void anon_send(const actor_hdl& hdl, Ts&&... args) {
    cppa::send_as(nullptr, hdl, std::forward<Ts>(args)...);
}
inline void anon_send_tuple(const actor_hdl& hdl, cppa::any_tuple tup) {
    cppa::send_tuple_as(nullptr, hdl, std::move(tup));
}

#define BLOCKING_SELF_ARG
#define SELF_ARG
#define SELF_PREFIX
#define SCOPED_SELF static_cast<void>(0)

#else // CPPA_VERSION
// compatibility to libcppa >= 0.9 releases

typedef cppa::actor actor_hdl;
#define BLOCKING_SELF_ARG blocking_actor* self,
#define SELF_ARG event_based_actor* self,
#define SELF_PREFIX self->
#define SCOPED_SELF scoped_actor self;

#endif // CPPA_VERSION

#endif // BACKWARD_COMPATIBILITY_HPP
