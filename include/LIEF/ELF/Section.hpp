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
#ifndef LIEF_ELF_SECTION_H_
#define LIEF_ELF_SECTION_H_

#include <string>
#include <vector>
#include <iostream>
#include <set>

#include "LIEF/visibility.h"

#include "LIEF/Abstract/Section.hpp"

#include "LIEF/ELF/type_traits.hpp"
#include "LIEF/ELF/enums.hpp"


namespace LIEF {
namespace ELF {

namespace DataHandler {
class Handler;
}

class Segment;
class Parser;
class Binary;
class Builder;
class ExeLayout;
class ObjectFileLayout;

struct Elf64_Shdr;
struct Elf32_Shdr;

LIEF_API Section operator"" _section(const char* name);

//! @brief Class wich represent sections
class LIEF_API Section : public LIEF::Section {
  friend class Parser;
  friend class Binary;
  friend class Builder;
  friend class ExeLayout;
  friend class ObjectFileLayout;

  public:
  Section(uint8_t *data, ELF_CLASS type);
  Section(const Elf64_Shdr* header);
  Section(const Elf32_Shdr* header);
  Section(const std::string& name, ELF_SECTION_TYPES type = ELF_SECTION_TYPES::SHT_PROGBITS);

  Section();
  ~Section();

  Section& operator=(Section other);
  Section(const Section& other);
  void swap(Section& other);

  uint32_t          name_idx() const;
  ELF_SECTION_TYPES type() const;

  // ============================
  // LIEF::Section implementation
  // ============================

  //! @brief Section's content
  virtual std::vector<uint8_t> content() const override;

  //! @brief Set section content
  virtual void content(const std::vector<uint8_t>& data) override;

  void content(std::vector<uint8_t>&& data);

  //! @brief Section flags LIEF::ELF::ELF_SECTION_FLAGS
  uint64_t flags() const;

  //! @brief ``True`` if the section has the given flag
  //!
  //! @param[in] flag flag to test
  bool has(ELF_SECTION_FLAGS flag) const;

  //! @brief ``True`` if the section is in the given segment
  bool has(const Segment& segment) const;

  //! @brief Return section flags as a ``std::set``
  std::set<ELF_SECTION_FLAGS> flags_list() const;

  virtual uint64_t size() const override;

  virtual void size(uint64_t size) override;

  virtual void offset(uint64_t offset) override;

  virtual uint64_t offset() const override;


  //! @see offset
  uint64_t file_offset() const;
  uint64_t original_size() const;
  uint64_t alignment() const;
  uint64_t information() const;
  uint64_t entry_size() const;
  uint32_t link() const;


  //! Clear the content of the section with the given ``value``
  Section& clear(uint8_t value = 0);
  void add(ELF_SECTION_FLAGS flag);
  void remove(ELF_SECTION_FLAGS flag);

  void type(ELF_SECTION_TYPES type);
  void flags(uint64_t flags);
  void clear_flags();
  void file_offset(uint64_t offset);
  void link(uint32_t link);
  void information(uint32_t info);
  void alignment(uint64_t alignment);
  void entry_size(uint64_t entry_size);

  it_segments       segments();
  it_const_segments segments() const;

  virtual void accept(Visitor& visitor) const override;

  Section& operator+=(ELF_SECTION_FLAGS c);
  Section& operator-=(ELF_SECTION_FLAGS c);

  bool operator==(const Section& rhs) const;
  bool operator!=(const Section& rhs) const;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Section& section);

  private:
  // virtualAddress_, offset_ and size_ are inherited from LIEF::Section
  uint32_t              name_idx_;
  ELF_SECTION_TYPES     type_;
  uint64_t              flags_;
  uint64_t              original_size_;
  uint32_t              link_;
  uint32_t              info_;
  uint64_t              address_align_;
  uint64_t              entry_size_;
  segments_t            segments_;
  DataHandler::Handler* datahandler_{nullptr};
  std::vector<uint8_t>  content_c_;
};

}
}
#endif /* _ELF_SECTION_H_ */
