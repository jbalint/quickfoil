/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_UTILITY_ELEMENT_DELETER_HPP_
#define QUICKFOIL_UTILITY_ELEMENT_DELETER_HPP_

#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

namespace quickfoil {

template <class T>
void DeleteElements(Vector<T*>* vec) {
  if (vec == nullptr) {
    return;
  }

  for (const T* element : *vec) {
    delete element;
  }
  vec->clear();
}

template <class T>
class ElementDeleter {
 public:
  ElementDeleter(Vector<T*>* vec)
      : vec_(vec) {
  }

  ~ElementDeleter() {
    DeleteElements(vec_);
    vec_ = nullptr;
  }

  void Release() {
    vec_ = nullptr;
  }

 private:
  Vector<T*>* vec_;

  DISALLOW_COPY_AND_ASSIGN(ElementDeleter);
};

}

#endif /* QUICKFOIL_UTILITY_ELEMENTDELETER_HPP_ */
