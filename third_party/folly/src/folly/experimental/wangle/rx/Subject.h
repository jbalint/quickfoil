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

#include <folly/experimental/wangle/rx/Observable.h>
#include <folly/experimental/wangle/rx/Observer.h>

namespace folly { namespace wangle {

/// Subject interface. A Subject is both an Observable and an Observer. There
/// is a default implementation of the Observer methods that just forwards the
/// observed events to the Subject's observers.
template <class T>
struct Subject : public Observable<T>, public Observer<T> {
  typedef typename Observable<T>::ObserversGuard ObserversGuard;
  void onNext(const T& val) override {
    ObserversGuard guard(this);
    for (auto& kv : Observable<T>::getObservers()) {
      kv.second->onNext(val);
    }
  }
  void onError(Error e) override {
    ObserversGuard guard(this);
    for (auto& kv : Observable<T>::getObservers()) {
      kv.second->onError(e);
    }
  }
  void onCompleted() override {
    ObserversGuard guard(this);
    for (auto& kv : Observable<T>::getObservers()) {
      kv.second->onCompleted();
    }
  }
};

}}
