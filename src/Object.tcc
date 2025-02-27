/* Copyright 2017 - 2021 R. Thomas
 * Copyright 2017 - 2021 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <iomanip>
#include "LIEF/Object.hpp"

namespace LIEF {

template<class T>
bool Object::is() const {
  return typeid(*this) == typeid(T);
}

template<class T>
Object::output_t<T> Object::as() {
  return reinterpret_cast<Object::output_t<T>>(this);
}

template<class T>
Object::output_const_t<T> Object::as() const {
  return reinterpret_cast<Object::output_const_t<T>>(this);
}


} // namespace LIEF

