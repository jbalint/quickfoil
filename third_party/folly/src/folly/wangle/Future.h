/*
 * Copyright 2014 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <algorithm>
#include <exception>
#include <functional>
#include <memory>
#include <type_traits>
#include <vector>

#include <folly/MoveWrapper.h>
#include <folly/wangle/Promise.h>
#include <folly/wangle/Try.h>

namespace folly { namespace wangle {

namespace detail {
  template <class> struct Core;
  template <class...> struct VariadicContext;
}
template <class> struct Promise;

template <typename T> struct isFuture;

template <class T>
class Future {
 public:
  typedef T value_type;

  // not copyable
  Future(Future const&) = delete;
  Future& operator=(Future const&) = delete;

  // movable
  Future(Future&&) noexcept;
  Future& operator=(Future&&);

  ~Future();

  /** Return the reference to result. Should not be called if !isReady().
    Will rethrow the exception if an exception has been
    captured.

    This function is not thread safe - the returned Future can only
    be executed from the thread that the executor runs it in.
    See below for a thread safe version
    */
  typename std::add_lvalue_reference<T>::type
  value();
  typename std::add_lvalue_reference<const T>::type
  value() const;

  /// Returns an inactive Future which will call back on the other side of
  /// executor (when it is activated).
  ///
  /// NB remember that Futures activate when they destruct. This is good,
  /// it means that this will work:
  ///
  ///   f.via(e).then(a).then(b);
  ///
  /// a and b will execute in the same context (the far side of e), because
  /// the Future (temporary variable) created by via(e) does not call back
  /// until it destructs, which is after then(a) and then(b) have been wired
  /// up.
  ///
  /// But this is still racy:
  ///
  ///   f = f.via(e).then(a);
  ///   f.then(b);
  ///
  /// If you need something like that, use a Later.
  template <typename Executor>
  Future<T> via(Executor* executor);

  /** True when the result (or exception) is ready. */
  bool isReady() const;

  /** A reference to the Try of the value */
  Try<T>& getTry();

  /** When this Future has completed, execute func which is a function that
    takes a Try<T>&&. A Future for the return type of func is
    returned. e.g.

    Future<string> f2 = f1.then([](Try<T>&&) { return string("foo"); });

    The Future given to the functor is ready, and the functor may call
    value(), which may rethrow if this has captured an exception. If func
    throws, the exception will be captured in the Future that is returned.
    */
  /* TODO n3428 and other async frameworks have something like then(scheduler,
     Future), we might want to support a similar API which could be
     implemented a little more efficiently than
     f.via(executor).then(callback) */
  template <class F>
  typename std::enable_if<
    !isFuture<typename std::result_of<F(Try<T>&&)>::type>::value,
    Future<typename std::result_of<F(Try<T>&&)>::type> >::type
  then(F&& func);

  /// Variant where func returns a Future<T> instead of a T. e.g.
  ///
  ///   Future<string> f2 = f1.then(
  ///     [](Try<T>&&) { return makeFuture<string>("foo"); });
  template <class F>
  typename std::enable_if<
    isFuture<typename std::result_of<F(Try<T>&&)>::type>::value,
    Future<typename std::result_of<F(Try<T>&&)>::type::value_type> >::type
  then(F&& func);

  /// Variant where func is an ordinary function (static method, method)
  ///
  ///   R doWork(Try<T>&&);
  ///
  ///   Future<R> f2 = f1.then(doWork);
  ///
  /// or
  ///
  ///   struct Worker {
  ///     static R doWork(Try<T>&&); }
  ///
  ///   Future<R> f2 = f1.then(&Worker::doWork);
  template <class = T, class R = std::nullptr_t>
  typename std::enable_if<!isFuture<R>::value, Future<R>>::type
  inline then(R(*func)(Try<T>&&)) {
    return then([func](Try<T>&& t) {
      return (*func)(std::move(t));
    });
  }

  /// Variant where func returns a Future<R> instead of a R. e.g.
  ///
  ///   struct Worker {
  ///     Future<R> doWork(Try<T>&&); }
  ///
  ///   Future<R> f2 = f1.then(&Worker::doWork);
  template <class = T, class R = std::nullptr_t>
  typename std::enable_if<isFuture<R>::value, R>::type
  inline then(R(*func)(Try<T>&&)) {
    return then([func](Try<T>&& t) {
      return (*func)(std::move(t));
    });
  }

  /// Variant where func is an member function
  ///
  ///   struct Worker {
  ///     R doWork(Try<T>&&); }
  ///
  ///   Worker *w;
  ///   Future<R> f2 = f1.then(w, &Worker::doWork);
  template <class = T, class R = std::nullptr_t, class Caller = std::nullptr_t>
  typename std::enable_if<!isFuture<R>::value, Future<R>>::type
  inline then(Caller *instance, R(Caller::*func)(Try<T>&&)) {
    return then([instance, func](Try<T>&& t) {
      return (instance->*func)(std::move(t));
    });
  }

  /// Variant where func returns a Future<R> instead of a R. e.g.
  ///
  ///   struct Worker {
  ///     Future<R> doWork(Try<T>&&); }
  ///
  ///   Worker *w;
  ///   Future<R> f2 = f1.then(w, &Worker::doWork);
  template <class = T, class R = std::nullptr_t, class Caller = std::nullptr_t>
  typename std::enable_if<isFuture<R>::value, R>::type
  inline then(Caller *instance, R(Caller::*func)(Try<T>&&)) {
    return then([instance, func](Try<T>&& t) {
      return (instance->*func)(std::move(t));
    });
  }

  /// Convenience method for ignoring the value and creating a Future<void>.
  /// Exceptions still propagate.
  Future<void> then();

  /// This is not the method you're looking for.
  ///
  /// This needs to be public because it's used by make* and when*, and it's
  /// not worth listing all those and their fancy template signatures as
  /// friends. But it's not for public consumption.
  template <class F>
  void setCallback_(F&& func);

  /// A Future's callback is executed when all three of these conditions have
  /// become true: it has a value (set by the Promise), it has a callback (set
  /// by then), and it is active (active by default).
  ///
  /// Inactive Futures will activate upon destruction.
  void activate() {
    core_->activate();
  }
  void deactivate() {
    core_->deactivate();
  }
  bool isActive() {
    return core_->isActive();
  }

  template <class E>
  void raise(E&& exception) {
    raise(std::make_exception_ptr(std::forward<E>(exception)));
  }

  /// Raise an interrupt. If the promise holder has an interrupt
  /// handler it will be called and potentially stop asynchronous work from
  /// being done. This is advisory only - a promise holder may not set an
  /// interrupt handler, or may do anything including ignore. But, if you know
  /// your future supports this the most likely result is stopping or
  /// preventing the asynchronous operation (if in time), and the promise
  /// holder setting an exception on the future. (That may happen
  /// asynchronously, of course.)
  void raise(std::exception_ptr interrupt);

  void cancel() {
    raise(FutureCancellation());
  }

 private:
  typedef detail::Core<T>* corePtr;

  // shared core state object
  corePtr core_;

  explicit
  Future(corePtr obj) : core_(obj) {}

  void detach();

  void throwIfInvalid() const;

  friend class Promise<T>;
};

/**
  Make a completed Future by moving in a value. e.g.

    string foo = "foo";
    auto f = makeFuture(std::move(foo));

  or

    auto f = makeFuture<string>("foo");
*/
template <class T>
Future<typename std::decay<T>::type> makeFuture(T&& t);

/** Make a completed void Future. */
Future<void> makeFuture();

/** Make a completed Future by executing a function. If the function throws
  we capture the exception, otherwise we capture the result. */
template <class F>
auto makeFutureTry(
  F&& func,
  typename std::enable_if<
    !std::is_reference<F>::value, bool>::type sdf = false)
  -> Future<decltype(func())>;

template <class F>
auto makeFutureTry(
  F const& func)
  -> Future<decltype(func())>;

/// Make a failed Future from an exception_ptr.
/// Because the Future's type cannot be inferred you have to specify it, e.g.
///
///   auto f = makeFuture<string>(std::current_exception());
template <class T>
Future<T> makeFuture(std::exception_ptr const& e);

/** Make a Future from an exception type E that can be passed to
  std::make_exception_ptr(). */
template <class T, class E>
typename std::enable_if<std::is_base_of<std::exception, E>::value,
                        Future<T>>::type
makeFuture(E const& e);

/** Make a Future out of a Try */
template <class T>
Future<T> makeFuture(Try<T>&& t);

/** When all the input Futures complete, the returned Future will complete.
  Errors do not cause early termination; this Future will always succeed
  after all its Futures have finished (whether successfully or with an
  error).

  The Futures are moved in, so your copies are invalid. If you need to
  chain further from these Futures, use the variant with an output iterator.

  XXX is this still true?
  This function is thread-safe for Futures running on different threads.

  The return type for Future<T> input is a Future<std::vector<Try<T>>>
  */
template <class InputIterator>
Future<std::vector<Try<
  typename std::iterator_traits<InputIterator>::value_type::value_type>>>
whenAll(InputIterator first, InputIterator last);

/// This version takes a varying number of Futures instead of an iterator.
/// The return type for (Future<T1>, Future<T2>, ...) input
/// is a Future<std::tuple<Try<T1>, Try<T2>, ...>>.
/// The Futures are moved in, so your copies are invalid.
template <typename... Fs>
typename detail::VariadicContext<
  typename std::decay<Fs>::type::value_type...>::type
whenAll(Fs&&... fs);

/** The result is a pair of the index of the first Future to complete and
  the Try. If multiple Futures complete at the same time (or are already
  complete when passed in), the "winner" is chosen non-deterministically.

  This function is thread-safe for Futures running on different threads.
  */
template <class InputIterator>
Future<std::pair<
  size_t,
  Try<typename std::iterator_traits<InputIterator>::value_type::value_type>>>
whenAny(InputIterator first, InputIterator last);

/** when n Futures have completed, the Future completes with a vector of
  the index and Try of those n Futures (the indices refer to the original
  order, but the result vector will be in an arbitrary order)

  Not thread safe.
  */
template <class InputIterator>
Future<std::vector<std::pair<
  size_t,
  Try<typename std::iterator_traits<InputIterator>::value_type::value_type>>>>
whenN(InputIterator first, InputIterator last, size_t n);

/** Wait for the given future to complete on a semaphore. Returns a completed
 * future containing the result.
 *
 * NB if the promise for the future would be fulfilled in the same thread that
 * you call this, it will deadlock.
 */
template <class T>
Future<T> waitWithSemaphore(Future<T>&& f);

/** Wait for up to `timeout` for the given future to complete. Returns a future
 * which may or may not be completed depending whether the given future
 * completed in time
 *
 * Note: each call to this starts a (short-lived) thread and allocates memory.
 */
template <typename T, class Duration>
Future<T> waitWithSemaphore(Future<T>&& f, Duration timeout);

}} // folly::wangle

#include <folly/wangle/Future-inl.h>
