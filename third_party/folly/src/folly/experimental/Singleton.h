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

// SingletonVault - a library to manage the creation and destruction
// of interdependent singletons.
//
// Basic usage of this class is very simple; suppose you have a class
// called MyExpensiveService, and you only want to construct one (ie,
// it's a singleton), but you only want to construct it if it is used.
//
// In your .h file:
// class MyExpensiveService { ... };
//
// In your .cpp file:
// namespace { folly::Singleton<MyExpensiveService> the_singleton; }
//
// Code can access it via:
//
// MyExpensiveService* instance = Singleton<MyExpensiveService>::get();
// or
// std::weak_ptr<MyExpensiveService> instance =
//     Singleton<MyExpensiveService>::get_weak();
//
// You also can directly access it by the variable defining the
// singleton rather than via get(), and even treat that variable like
// a smart pointer (dereferencing it or using the -> operator).
//
// Please note, however, that all non-weak_ptr interfaces are
// inherently subject to races with destruction.  Use responsibly.
//
// The singleton will be created on demand.  If the constructor for
// MyExpensiveService actually makes use of *another* Singleton, then
// the right thing will happen -- that other singleton will complete
// construction before get() returns.  However, in the event of a
// circular dependency, a runtime error will occur.
//
// You can have multiple singletons of the same underlying type, but
// each must be given a unique name:
//
// namespace {
// folly::Singleton<MyExpensiveService> s1("name1");
// folly::Singleton<MyExpensiveService> s2("name2");
// }
// ...
// MyExpensiveService* svc1 = Singleton<MyExpensiveService>::get("name1");
// MyExpensiveService* svc2 = Singleton<MyExpensiveService>::get("name2");
//
// By default, the singleton instance is constructed via new and
// deleted via delete, but this is configurable:
//
// namespace { folly::Singleton<MyExpensiveService> the_singleton(create,
//                                                                destroy); }
//
// Where create and destroy are functions, Singleton<T>::CreateFunc
// Singleton<T>::TeardownFunc.
//
// What if you need to destroy all of your singletons?  Say, some of
// your singletons manage threads, but you need to fork?  Or your unit
// test wants to clean up all global state?  Then you can call
// SingletonVault::singleton()->destroyInstances(), which invokes the
// TeardownFunc for each singleton, in the reverse order they were
// created.  It is your responsibility to ensure your singletons can
// handle cases where the singletons they depend on go away, however.
// Singletons won't be recreated after destroyInstances call. If you
// want to re-enable singleton creation (say after fork was called) you
// should call reenableInstances.

#pragma once
#include <folly/Exception.h>
#include <folly/Hash.h>
#include <folly/RWSpinLock.h>

#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <string>
#include <unordered_map>
#include <functional>
#include <typeinfo>
#include <typeindex>

#include <glog/logging.h>

namespace folly {

// For actual usage, please see the Singleton<T> class at the bottom
// of this file; that is what you will actually interact with.

// SingletonVault is the class that manages singleton instances.  It
// is unaware of the underlying types of singletons, and simply
// manages lifecycles and invokes CreateFunc and TeardownFunc when
// appropriate.  In general, you won't need to interact with the
// SingletonVault itself.
//
// A vault goes through a few stages of life:
//
//   1. Registration phase; singletons can be registered, but no
//      singleton can be created.
//   2. registrationComplete() has been called; singletons can no
//      longer be registered, but they can be created.
//   3. A vault can return to stage 1 when destroyInstances is called.
//
// In general, you don't need to worry about any of the above; just
// ensure registrationComplete() is called near the top of your main()
// function, otherwise no singletons can be instantiated.

namespace detail {

const char* const kDefaultTypeDescriptorName = "(default)";
// A TypeDescriptor is the unique handle for a given singleton.  It is
// a combinaiton of the type and of the optional name, and is used as
// a key in unordered_maps.
class TypeDescriptor {
 public:
  TypeDescriptor(const std::type_info& ti, std::string name)
      : ti_(ti), name_(name) {
    if (name_ == kDefaultTypeDescriptorName) {
      LOG(DFATAL) << "Caller used the default name as their literal name; "
                  << "name your singleton something other than "
                  << kDefaultTypeDescriptorName;
    }
  }

  std::string name() const {
    std::string ret = ti_.name();
    ret += "/";
    if (name_.empty()) {
      ret += kDefaultTypeDescriptorName;
    } else {
      ret += name_;
    }
    return ret;
  }

  friend class TypeDescriptorHasher;

  bool operator==(const TypeDescriptor& other) const {
    return ti_ == other.ti_ && name_ == other.name_;
  }

 private:
  const std::type_index ti_;
  const std::string name_;
};

class TypeDescriptorHasher {
 public:
  size_t operator()(const TypeDescriptor& ti) const {
    return folly::hash::hash_combine(ti.ti_, ti.name_);
  }
};
}

class SingletonVault {
 public:
  enum class Type { Strict, Relaxed };

  explicit SingletonVault(Type type = Type::Relaxed) : type_(type) {}

  // Destructor is only called by unit tests to check destroyInstances.
  ~SingletonVault();

  typedef std::function<void(void*)> TeardownFunc;
  typedef std::function<void*(void)> CreateFunc;

  // Register a singleton of a given type with the create and teardown
  // functions.
  void registerSingleton(detail::TypeDescriptor type,
                         CreateFunc create,
                         TeardownFunc teardown) {
    RWSpinLock::ReadHolder rh(&stateMutex_);

    stateCheck(SingletonVaultState::Running);
    if (UNLIKELY(registrationComplete_)) {
      throw std::logic_error(
        "Registering singleton after registrationComplete().");
    }

    RWSpinLock::WriteHolder wh(&mutex_);

    CHECK_THROW(singletons_.find(type) == singletons_.end(), std::logic_error);
    auto& entry = singletons_[type];
    entry.reset(new SingletonEntry);

    std::lock_guard<std::mutex> entry_guard(entry->mutex);
    CHECK(entry->instance == nullptr);
    CHECK(create);
    CHECK(teardown);
    entry->create = create;
    entry->teardown = teardown;
    entry->state = SingletonEntryState::Dead;
  }

  // Mark registration is complete; no more singletons can be
  // registered at this point.
  void registrationComplete() {
    scheduleDestroyInstances();

    RWSpinLock::WriteHolder wh(&stateMutex_);

    stateCheck(SingletonVaultState::Running);

    if (type_ == Type::Strict) {
      for (const auto& id_singleton_entry: singletons_) {
        const auto& singleton_entry = *id_singleton_entry.second;
        if (singleton_entry.state != SingletonEntryState::Dead) {
          throw std::runtime_error(
            "Singleton created before registration was complete.");
        }
      }
    }

    registrationComplete_ = true;
  }

  // Destroy all singletons; when complete, the vault can't create
  // singletons once again until reenableInstances() is called.
  void destroyInstances();

  // Enable re-creating singletons after destroyInstances() was called.
  void reenableInstances();

  // Retrieve a singleton from the vault, creating it if necessary.
  std::weak_ptr<void> get_weak(detail::TypeDescriptor type) {
    auto entry = get_entry_create(type);
    return entry->instance_weak;
  }

  // This function is inherently racy since we don't hold the
  // shared_ptr that contains the Singleton.  It is the caller's
  // responsibility to be sane with this, but it is preferable to use
  // the weak_ptr interface for true safety.
  void* get_ptr(detail::TypeDescriptor type) {
    auto entry = get_entry_create(type);
    if (UNLIKELY(entry->instance_weak.expired())) {
      throw std::runtime_error(
        "Raw pointer to a singleton requested after its destruction.");
    }
    return entry->instance_ptr;
  }

  // For testing; how many registered and living singletons we have.
  size_t registeredSingletonCount() const {
    RWSpinLock::ReadHolder rh(&mutex_);

    return singletons_.size();
  }

  size_t livingSingletonCount() const {
    RWSpinLock::ReadHolder rh(&mutex_);

    size_t ret = 0;
    for (const auto& p : singletons_) {
      std::lock_guard<std::mutex> entry_guard(p.second->mutex);
      if (p.second->instance) {
        ++ret;
      }
    }

    return ret;
  }

  // A well-known vault; you can actually have others, but this is the
  // default.
  static SingletonVault* singleton();

 private:
  // The two stages of life for a vault, as mentioned in the class comment.
  enum class SingletonVaultState {
    Running,
    Quiescing,
  };

  // Each singleton in the vault can be in three states: dead
  // (registered but never created), being born (running the
  // CreateFunc), and living (CreateFunc returned an instance).
  enum class SingletonEntryState {
    Dead,
    BeingBorn,
    Living,
  };

  void stateCheck(SingletonVaultState expected,
                  const char* msg="Unexpected singleton state change") {
    if (expected != state_) {
        throw std::logic_error(msg);
    }
  }

  // An actual instance of a singleton, tracking the instance itself,
  // its state as described above, and the create and teardown
  // functions.
  struct SingletonEntry {
    // mutex protects the entire entry
    std::mutex mutex;

    // state changes notify state_condvar
    SingletonEntryState state = SingletonEntryState::Dead;
    std::condition_variable state_condvar;

    // the thread creating the singleton
    std::thread::id creating_thread;

    // The singleton itself and related functions.
    std::shared_ptr<void> instance;
    std::weak_ptr<void> instance_weak;
    void* instance_ptr = nullptr;
    CreateFunc create = nullptr;
    TeardownFunc teardown = nullptr;

    SingletonEntry() = default;
    SingletonEntry(const SingletonEntry&) = delete;
    SingletonEntry& operator=(const SingletonEntry&) = delete;
    SingletonEntry& operator=(SingletonEntry&&) = delete;
    SingletonEntry(SingletonEntry&&) = delete;
  };

  // Initializes static object, which calls destroyInstances on destruction.
  // Used to have better deletion ordering with singleton not managed by
  // folly::Singleton. The desruction will happen in the following order:
  // 1. Singletons, not managed by folly::Singleton, which were created after
  //    any of the singletons managed by folly::Singleton was requested.
  // 2. All singletons managed by folly::Singleton
  // 3. Singletons, not managed by folly::Singleton, which were created before
  //    any of the singletons managed by folly::Singleton was requested.
  static void scheduleDestroyInstances();

  SingletonEntry* get_entry(detail::TypeDescriptor type) {
    RWSpinLock::ReadHolder rh(&mutex_);

    auto it = singletons_.find(type);
    if (it == singletons_.end()) {
      throw std::out_of_range(std::string("non-existent singleton: ") +
                              type.name());
    }

    return it->second.get();
  }

  // Get a pointer to the living SingletonEntry for the specified
  // type.  The singleton is created as part of this function, if
  // necessary.
  SingletonEntry* get_entry_create(detail::TypeDescriptor type) {
    auto entry = get_entry(type);

    std::unique_lock<std::mutex> entry_lock(entry->mutex);

    if (entry->state == SingletonEntryState::BeingBorn) {
      // If this thread is trying to give birth to the singleton, it's
      // a circular dependency and we must panic.
      if (entry->creating_thread == std::this_thread::get_id()) {
        throw std::out_of_range(std::string("circular singleton dependency: ") +
                                type.name());
      }

      entry->state_condvar.wait(entry_lock, [&entry]() {
        return entry->state != SingletonEntryState::BeingBorn;
      });
    }

    if (entry->instance == nullptr) {
      RWSpinLock::ReadHolder rh(&stateMutex_);
      if (state_ == SingletonVaultState::Quiescing) {
        return entry;
      }

      CHECK(entry->state == SingletonEntryState::Dead);
      entry->state = SingletonEntryState::BeingBorn;
      entry->creating_thread = std::this_thread::get_id();

      entry_lock.unlock();
      // Can't use make_shared -- no support for a custom deleter, sadly.
      auto instance = std::shared_ptr<void>(entry->create(), entry->teardown);

      // We should schedule destroyInstances() only after the singleton was
      // created. This will ensure it will be destroyed before singletons,
      // not managed by folly::Singleton, which were initialized in its
      // constructor
      scheduleDestroyInstances();

      entry_lock.lock();

      CHECK(entry->state == SingletonEntryState::BeingBorn);
      entry->instance = instance;
      entry->instance_weak = instance;
      entry->instance_ptr = instance.get();
      entry->state = SingletonEntryState::Living;
      entry->state_condvar.notify_all();

      {
        RWSpinLock::WriteHolder wh(&mutex_);

        creation_order_.push_back(type);
      }
    }
    CHECK(entry->state == SingletonEntryState::Living);
    return entry;
  }

  mutable folly::RWSpinLock mutex_;
  typedef std::unique_ptr<SingletonEntry> SingletonEntryPtr;
  std::unordered_map<detail::TypeDescriptor,
                     SingletonEntryPtr,
                     detail::TypeDescriptorHasher> singletons_;
  std::vector<detail::TypeDescriptor> creation_order_;
  SingletonVaultState state_{SingletonVaultState::Running};
  bool registrationComplete_{false};
  folly::RWSpinLock stateMutex_;
  Type type_{Type::Relaxed};
};

// This is the wrapper class that most users actually interact with.
// It allows for simple access to registering and instantiating
// singletons.  Create instances of this class in the global scope of
// type Singleton<T> to register your singleton for later access via
// Singleton<T>::get().
template <typename T>
class Singleton {
 public:
  typedef std::function<T*(void)> CreateFunc;
  typedef std::function<void(T*)> TeardownFunc;

  // Generally your program life cycle should be fine with calling
  // get() repeatedly rather than saving the reference, and then not
  // call get() during process shutdown.
  static T* get(SingletonVault* vault = nullptr /* for testing */) {
    return get_ptr({typeid(T), ""}, vault);
  }

  static T* get(const char* name,
                SingletonVault* vault = nullptr /* for testing */) {
    return get_ptr({typeid(T), name}, vault);
  }

  // If, however, you do need to hold a reference to the specific
  // singleton, you can try to do so with a weak_ptr.  Avoid this when
  // possible but the inability to lock the weak pointer can be a
  // signal that the vault has been destroyed.
  static std::weak_ptr<T> get_weak(
      SingletonVault* vault = nullptr /* for testing */) {
    return get_weak("", vault);
  }

  static std::weak_ptr<T> get_weak(
      const char* name, SingletonVault* vault = nullptr /* for testing */) {
    auto weak_void_ptr =
      (vault ?: SingletonVault::singleton())->get_weak({typeid(T), name});

    // This is ugly and inefficient, but there's no other way to do it, because
    // there's no static_pointer_cast for weak_ptr.
    auto shared_void_ptr = weak_void_ptr.lock();
    if (!shared_void_ptr) {
      return std::weak_ptr<T>();
    }
    return std::static_pointer_cast<T>(shared_void_ptr);
  }

  // Allow the Singleton<t> instance to also retrieve the underlying
  // singleton, if desired.
  T* ptr() { return get_ptr(type_descriptor_, vault_); }
  T& operator*() { return *ptr(); }
  T* operator->() { return ptr(); }

  template <typename CreateFunc = std::nullptr_t>
  explicit Singleton(CreateFunc c = nullptr,
                     Singleton::TeardownFunc t = nullptr,
                     SingletonVault* vault = nullptr /* for testing */)
      : Singleton({typeid(T), ""}, c, t, vault) {}

  template <typename CreateFunc = std::nullptr_t>
  explicit Singleton(const char* name,
                     CreateFunc c = nullptr,
                     Singleton::TeardownFunc t = nullptr,
                     SingletonVault* vault = nullptr /* for testing */)
      : Singleton({typeid(T), name}, c, t, vault) {}

 private:
  explicit Singleton(detail::TypeDescriptor type,
                     std::nullptr_t,
                     Singleton::TeardownFunc t,
                     SingletonVault* vault) :
      Singleton (type,
                 []() { return new T; },
                 std::move(t),
                 vault) {
  }

  explicit Singleton(detail::TypeDescriptor type,
                     Singleton::CreateFunc c,
                     Singleton::TeardownFunc t,
                     SingletonVault* vault)
      : type_descriptor_(type) {
    if (c == nullptr) {
      throw std::logic_error(
        "nullptr_t should be passed if you want T to be default constructed");
    }
    SingletonVault::TeardownFunc teardown;
    if (t == nullptr) {
      teardown = [](void* v) { delete static_cast<T*>(v); };
    } else {
      teardown = [t](void* v) { t(static_cast<T*>(v)); };
    }

    if (vault == nullptr) {
      vault = SingletonVault::singleton();
    }
    vault_ = vault;
    vault->registerSingleton(type, c, teardown);
  }

  static T* get_ptr(detail::TypeDescriptor type_descriptor = {typeid(T), ""},
                    SingletonVault* vault = nullptr /* for testing */) {
    return static_cast<T*>(
        (vault ?: SingletonVault::singleton())->get_ptr(type_descriptor));
  }

  // Don't use this function, it's private for a reason!  Using it
  // would defeat the *entire purpose* of the vault in that we lose
  // the ability to guarantee that, after a destroyInstances is
  // called, all instances are, in fact, destroyed.  You should use
  // weak_ptr if you need to hold a reference to the singleton and
  // guarantee briefly that it exists.
  //
  // Yes, you can just get the weak pointer and lock it, but hopefully
  // if you have taken the time to read this far, you see why that
  // would be bad.
  static std::shared_ptr<T> get_shared(
      detail::TypeDescriptor type_descriptor = {typeid(T), ""},
      SingletonVault* vault = nullptr /* for testing */) {
    return std::static_pointer_cast<T>(
      (vault ?: SingletonVault::singleton())->get_weak(type_descriptor).lock());
  }

  detail::TypeDescriptor type_descriptor_;
  SingletonVault* vault_;
};
}
