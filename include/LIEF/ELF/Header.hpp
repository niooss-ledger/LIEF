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
#ifndef LIEF_ELF_HEADER_H_
#define LIEF_ELF_HEADER_H_

#include <iostream>
#include <array>
#include <vector>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/Abstract/enums.hpp"

#include "LIEF/ELF/type_traits.hpp"
#include "LIEF/ELF/enums.hpp"

namespace LIEF {
namespace ELF {
struct Elf32_Ehdr;
struct Elf64_Ehdr;
class LIEF_API Header : public Object {

  public:
  using identity_t = std::array<uint8_t, 16>;
  using abstract_architecture_t = std::pair<ARCHITECTURES, std::set<MODES>>;

  public:
  Header();
  Header(const Elf32_Ehdr *header);
  Header(const Elf64_Ehdr *header);

  Header& operator=(const Header&);
  Header(const Header&);

  virtual ~Header();

  //! @brief Define the object file type. (e.g. executable, library...)
  E_TYPE file_type() const;

  //! @brief LIEF abstract object type
  OBJECT_TYPES abstract_object_type() const;

  //! @brief Target architecture
  ARCH machine_type() const;

  //! @brief LIEF abstract architecture
  //!
  //! Empty it can't be abstracted
  abstract_architecture_t abstract_architecture() const;

  //! @brief LIEF abstract endianness
  ENDIANNESS abstract_endianness() const;

  //! @brief Version of the object file format
  VERSION object_file_version() const;

  //! @brief Executable entrypoint
  uint64_t entrypoint() const;

  //! @brief Offset of program table
  uint64_t program_headers_offset() const;

  //! @brief Offset of section table
  uint64_t section_headers_offset() const;

  //! @brief processor-specific flags
  uint32_t processor_flag() const;

  //! @brief Check if the given flag is present in processor_flag()
  bool has(ARM_EFLAGS f) const;

  //! @brief Return a list of ARM_EFLAGS present in processor_flag()
  arm_flags_list_t arm_flags_list() const;

  //! @brief Check if the given flag is present in processor_flag()
  bool has(MIPS_EFLAGS f) const;

  //! @brief Return a list of MIPS_EFLAGS present in processor_flag()
  mips_flags_list_t mips_flags_list() const;

  //! @brief Check if the given flag is present in processor_flag()
  bool has(PPC64_EFLAGS f) const;

  //! @brief Return a list of PPC64_EFLAGS present in processor_flag()
  ppc64_flags_list_t ppc64_flags_list() const;

  //! @brief Check if the given flag is present in processor_flag()
  bool has(HEXAGON_EFLAGS f) const;

  //! @brief Return a list of HEXAGON_EFLAGS present in processor_flag()
  hexagon_flags_list_t hexagon_flags_list() const;

  //! @brief Size of the current header
  //!
  //! This size should be 64 for a ``ELF64`` binary and 52 for
  //! a ``ELF32`` one.
  uint32_t header_size() const;

  //! @brief Return the size of a ``Segment header``
  //!
  //! This size should be 56 for a ``ELF64`` binary and 32 for
  //! a ``ELF32`` one.
  uint32_t program_header_size() const;

  //! @brief Return the the number of segment's headers
  //! registred in the header
  uint32_t numberof_segments() const;

  //! @brief Return the size of a ``Section header``
  //!
  //! This size should be 64 for a ``ELF64`` binary and 40 for
  //! a ``ELF32`` one.
  uint32_t section_header_size() const;

  //! @brief Return the the number of sections's headers
  //! registred in the header
  //!
  //! @warning Could differ from the real number of sections
  //! present in the binary
  uint32_t numberof_sections() const;

  //! @brief Return the section's index which holds
  //! section's names
  uint32_t section_name_table_idx() const;

  identity_t&       identity();
  const identity_t& identity() const;

  //! @brief Return the object's class. ``ELF64`` or ``ELF32``
  ELF_CLASS identity_class() const;

  //! @brief Specify the data encoding
  ELF_DATA identity_data() const;

  //! @see object_file_version
  VERSION identity_version() const;

  //! @brief Identifies the version of the ABI for which the object is prepared
  OS_ABI identity_os_abi() const;

  //! @brief ABI Version
  uint32_t identity_abi_version() const;

  void file_type(E_TYPE type);
  void machine_type(ARCH machineType);
  void object_file_version(VERSION version);
  void entrypoint(uint64_t entryPoint);
  void program_headers_offset(uint64_t programHeaderOffset);
  void section_headers_offset(uint64_t sectionHeaderOffset);
  void processor_flag(uint32_t processorFlag);
  void header_size(uint32_t headerSize);
  void program_header_size(uint32_t programHeaderSize);
  void numberof_segments(uint32_t n);
  void section_header_size(uint32_t sizeOfSectionHeaderEntries);
  void numberof_sections(uint32_t n);
  void section_name_table_idx(uint32_t sectionNameStringTableIdx);
  void identity(const std::string& identity);
  void identity(const identity_t& identity);
  void identity_class(ELF_CLASS i_class);
  void identity_data(ELF_DATA data);
  void identity_version(VERSION version);
  void identity_os_abi(OS_ABI osabi);
  void identity_abi_version(uint32_t version);

  virtual void accept(Visitor& visitor) const override;

  bool operator==(const Header& rhs) const;
  bool operator!=(const Header& rhs) const;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Header& hdr);

  private:
  //! Field which represent ElfXX_Ehdr->e_ident
  identity_t identity_;

  //! Field which represent ElfXX_Ehdr->e_type
  E_TYPE file_type_;

  //! Field which represent ElfXX_Ehdr->e_machine
  ARCH machine_type_;

  //! Field which represent ElfXX_Ehdr->e_version
  VERSION object_file_version_;

  //! Field which represent ElfXX_Ehdr->e_entry
  uint64_t entrypoint_;

  //! Field which represent ElfXX_Ehdr->e_phoff
  uint64_t program_headers_offset_;

  //! Field which represent ElfXX_Ehdr->e_shoff
  uint64_t section_headers_offset_;

  //! Field which represent ElfXX_Ehdr->e_flags
  uint32_t processor_flags_;

  //! Field which represent ElfXX_Ehdr->e_ehsize
  uint32_t header_size_;

  //! Field which represent ElfXX_Ehdr->e_phentsize
  uint32_t program_header_size_;

  //! Field which represent ElfXX_Ehdr->e_phnum
  uint32_t numberof_segments_;

  //! Field which represent ElfXX_Ehdr->e_shentsize
  uint32_t section_header_size_;

  //! Field which represent ElfXX_Ehdr->e_shnum
  uint32_t numberof_sections_;

  //! Field which represent ElfXX_Ehdr->e_shstrndx
  uint32_t section_string_table_idx_;

};
}
}
#endif
