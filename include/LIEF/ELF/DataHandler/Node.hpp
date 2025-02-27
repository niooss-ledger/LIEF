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
#ifndef ELF_DATA_HANDLER_NODE_H_
#define ELF_DATA_HANDLER_NODE_H_

#include <vector>

#include "LIEF/types.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
namespace ELF {
namespace DataHandler {
class LIEF_API Node {
  public:
    enum Type : uint8_t {
      SECTION = 0,
      SEGMENT = 1,
      UNKNOWN = 2
    };
    Node();
    Node(uint64_t offset, uint64_t size, Type type);

    Node& operator=(const Node&);
    Node(const Node&);

    uint64_t size() const;
    uint64_t offset() const;
    Type     type() const;

    void size(uint64_t size);
    void type(Type type);
    void offset(uint64_t offset);

    bool operator==(const Node& rhs) const;
    bool operator!=(const Node& rhs) const;

    bool operator<(const Node& rhs) const;
    bool operator<=(const Node& rhs) const;

    bool operator>(const Node& rhs) const;
    bool operator>=(const Node& rhs) const;

  private:
    uint64_t size_;
    uint64_t offset_;
    Type     type_;
};

} // namespace DataHandler
} // namespace ELF
} // namespace LIEF

#endif
