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
#ifndef LIEF_PE_HEADER_H_
#define LIEF_PE_HEADER_H_
#include <array>
#include <set>
#include <iostream>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"

#include "LIEF/PE/enums.hpp"

namespace LIEF {
namespace PE {
struct pe_header;
class LIEF_API Header : public Object {
  public:
  using signature_t = std::array<uint8_t, /* PE Magic */ 4>;
  Header();
  Header(const pe_header *header);
  virtual ~Header();

  Header& operator=(const Header&);
  Header(const Header&);

  const signature_t& signature() const;
  MACHINE_TYPES      machine() const;
  uint16_t           numberof_sections() const;
  uint32_t           time_date_stamp() const;
  uint32_t           pointerto_symbol_table() const;
  uint32_t           numberof_symbols() const;
  uint16_t           sizeof_optional_header() const;
  HEADER_CHARACTERISTICS           characteristics() const;

  bool has_characteristic(HEADER_CHARACTERISTICS c) const;
  std::set<HEADER_CHARACTERISTICS> characteristics_list() const;

  void machine(MACHINE_TYPES type);
  void numberof_sections(uint16_t nbOfSections);
  void time_date_stamp(uint32_t timestamp);
  void pointerto_symbol_table(uint32_t pointerToSymbol);
  void numberof_symbols(uint32_t nbOfSymbols);
  void sizeof_optional_header(uint16_t sizeOfOptionalHdr);
  void characteristics(HEADER_CHARACTERISTICS characteristics);
  void signature(const signature_t& sig);

  void add_characteristic(HEADER_CHARACTERISTICS c);
  void remove_characteristic(HEADER_CHARACTERISTICS c);

  virtual void accept(Visitor& visitor) const override;

  bool operator==(const Header& rhs) const;
  bool operator!=(const Header& rhs) const;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Header& entry);

  private:
  signature_t   signature_;
  MACHINE_TYPES machine_;
  uint16_t      numberOfSections_;
  uint32_t      timeDateStamp_;
  uint32_t      pointerToSymbolTable_;
  uint32_t      numberOfSymbols_;
  uint16_t      sizeOfOptionalHeader_;
  HEADER_CHARACTERISTICS characteristics_;

};
}
}

#endif
