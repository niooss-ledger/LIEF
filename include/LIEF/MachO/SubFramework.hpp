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
#ifndef LIEF_MACHO_SUB_FRAMEWORK_H_
#define LIEF_MACHO_SUB_FRAMEWORK_H_
#include <string>
#include <iostream>

#include "LIEF/visibility.h"
#include "LIEF/types.hpp"

#include "LIEF/MachO/LoadCommand.hpp"

namespace LIEF {
namespace MachO {

class BinaryParser;

struct sub_framework_command;

class LIEF_API SubFramework : public LoadCommand {
  friend class BinaryParser;
  public:
  SubFramework();
  SubFramework(const sub_framework_command *cmd);

  SubFramework& operator=(const SubFramework& copy);
  SubFramework(const SubFramework& copy);

  virtual SubFramework* clone() const override;

  const std::string& umbrella() const;
  void umbrella(const std::string& u);

  virtual ~SubFramework();

  bool operator==(const SubFramework& rhs) const;
  bool operator!=(const SubFramework& rhs) const;

  virtual void accept(Visitor& visitor) const override;

  virtual std::ostream& print(std::ostream& os) const override;

  private:
  std::string umbrella_;

};

}
}
#endif
