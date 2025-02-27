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
#ifndef LIEF_MACHO_BINARY_H_
#define LIEF_MACHO_BINARY_H_

#include <vector>
#include <map>

#include "LIEF/types.hpp"
#include "LIEF/visibility.h"

#include "LIEF/Abstract/Binary.hpp"

#include "LIEF/MachO/type_traits.hpp"
#include "LIEF/MachO/Header.hpp"

namespace LIEF {
namespace MachO {

class BinaryParser;
class Builder;
class DyldInfo;
class BuildVersion;
class EncryptionInfo;
class DyldEnvironment;
class SubFramework;
class SegmentSplitInfo;
class DataInCode;
class CodeSignature;
class RPathCommand;
class ThreadCommand;
class VersionMin;
class SourceVersion;
class FunctionStarts;
class DynamicSymbolCommand;
class MainCommand;
class SymbolCommand;
class Symbol;
class UUIDCommand;
class DylinkerCommand;
class DylibCommand;
class SegmentCommand;
class LoadCommand;
class Header;

//! Class which represent a MachO binary
class LIEF_API Binary : public LIEF::Binary  {

  friend class BinaryParser;
  friend class Builder;
  friend class DyldInfo;

  public:
  using range_t = std::pair<uint64_t, uint64_t>;

  public:
  Binary(const Binary&) = delete;
  Binary& operator=(const Binary&) = delete;

  //! Return a reference to MachO::Header
  Header&       header();
  const Header& header() const;

  //! Return binary's @link MachO::LoadCommand load commands @endlink
  it_commands       commands();
  it_const_commands commands() const;

  //! Return binary's @link MachO::Fileset
  it_fileset_binaries       filesets();
  it_const_fileset_binaries filesets() const;

  //! Return binary's @link MachO::Symbol symbols @endlink
  it_symbols       symbols();
  it_const_symbols symbols() const;

  //! If a symbol with the given name exists
  bool has_symbol(const std::string& name) const;

  //! Return Symbol from the given name. If the symbol does not
  //! exists, it returns a null pointer
  const Symbol* get_symbol(const std::string& name) const;
  Symbol* get_symbol(const std::string& name);

  //! Check if the given symbol is an exported one
  static bool is_exported(const Symbol& symbol);

  //! Return binary's exported symbols
  it_exported_symbols       exported_symbols();
  it_const_exported_symbols exported_symbols() const;

  //! Check if the given symbol is an imported one
  static bool is_imported(const Symbol& symbol);

  //! Return binary's imported symbols
  it_imported_symbols       imported_symbols();
  it_const_imported_symbols imported_symbols() const;

  //! Return binary imported libraries (MachO::DylibCommand)
  it_libraries       libraries();
  it_const_libraries libraries() const;

  //! Return binary's @link MachO::SegmentCommand segments @endlink
  it_segments       segments();
  it_const_segments segments() const;

  //! Return binary's @link MachO::Section sections @endlink
  it_sections       sections();
  it_const_sections sections() const;

  //! Return binary's @link MachO::Relocation relocations @endlink
  it_relocations       relocations();
  it_const_relocations relocations() const;

  //! Reconstruct the binary object and write it in `filename`
  //! @param filename Path to write the reconstructed binary
  virtual void write(const std::string& filename) override;

  //! Reconstruct the binary object and return his content as bytes
  std::vector<uint8_t> raw();

  //! Check if the current binary has the given MachO::LOAD_COMMAND_TYPES
  bool has(LOAD_COMMAND_TYPES type) const;

  const LoadCommand& get(LOAD_COMMAND_TYPES type) const;
  LoadCommand& get(LOAD_COMMAND_TYPES type);

  //! Insert the new Load Command
  LoadCommand& add(const LoadCommand& command);

  //! Insert the new Load command at ``index``
  LoadCommand& add(const LoadCommand& command, size_t index);

  //! Insert the given DylibCommand
  LoadCommand& add(const DylibCommand& library);

  //! Add a new LC_SEGMENT command
  LoadCommand& add(const SegmentCommand& segment);

  //! Insert a new shared library through a ``LC_LOAD_DYLIB`` command
  LoadCommand& add_library(const std::string& name);

  //! Add section in the __TEXT segment
  Section* add_section(const Section& section);

  //! Add a section in the given MachO::SegmentCommand.
  //!
  //! @warning This method may corrupt the file if the segment is not the first one
  //! or the last one
  Section* add_section(const SegmentCommand& segment, const Section& section);

  virtual void remove_section(const std::string& name, bool clear = false) override;

  //! Remove the given command
  bool remove(const LoadCommand& command);

  //! Remove **all** Load Command with the given type (MachO::LOAD_COMMAND_TYPES)
  bool remove(LOAD_COMMAND_TYPES type);

  //! Remove the Load Command at ``index``
  bool remove_command(size_t index);

  //! Remove the LC_SIGNATURE command
  bool remove_signature();

  //! Extend the **size** of the Load command
  bool extend(const LoadCommand& command, uint64_t size);

  //! Extend the **content** of the Segment
  bool extend_segment(const SegmentCommand& segment, size_t size);

  //! Remove ``PIE`` flag
  bool disable_pie();

  //! Return binary's imagebase. ``0`` if not relevant
  uint64_t imagebase() const override;

  //! Size of the binary in memory when mapped
  uint64_t virtual_size() const;

  //! Return binary's loader (e.g. ``/usr/lib/dyld``)
  const std::string& loader() const;

  //! Check if a section with the given name exists
  bool has_section(const std::string& name) const;

  //! Return the section from the given name
  Section& get_section(const std::string& name);

  //! Return the section from the given name
  const Section& get_section(const std::string& name) const;

  //! Check if a segment with the given name exists
  bool has_segment(const std::string& name) const;

  //! Return the segment from the given name
  const SegmentCommand* get_segment(const std::string& name) const;
  SegmentCommand* get_segment(const std::string& name);

  //! Remove symbol with the given name
  bool remove_symbol(const std::string& name);

  //! Remove the given symbol
  bool remove(const Symbol& sym);

  //! Check if the given symbol can be safely removed.
  bool can_remove(const Symbol& sym) const;

  bool can_remove_symbol(const std::string& name) const;

  //! Remove the given symbol from the export table
  bool unexport(const std::string& name);

  //! Remove the given symbol from the export table
  bool unexport(const Symbol& sym);

  // ======
  // Helper
  // ======

  //! Return binary's @link MachO::Section section @endlink
  //! which holds the offset
  Section*       section_from_offset(uint64_t offset);
  const Section* section_from_offset(uint64_t offset) const;

  //! Return binary's @link MachO::Section section @endlink
  //! which holds the given virtual address
  Section*       section_from_virtual_address(uint64_t virtual_address);
  const Section* section_from_virtual_address(uint64_t virtual_address) const;

  //! Convert a virtual address to an offset in the file
  uint64_t virtual_address_to_offset(uint64_t virtualAddress) const;

  //! Convert the given offset into a virtual address.
  //!
  //! @param[in] offset The offset to convert.
  //! @param[in] slide If not 0, it will replace the default base address (if any)
  uint64_t offset_to_virtual_address(uint64_t offset, uint64_t slide = 0) const override;

  // Return binary's @link MachO::SegmentCommand segment command
  // which hold the offset
  SegmentCommand*       segment_from_offset(uint64_t offset);
  const SegmentCommand* segment_from_offset(uint64_t offset) const;

  //! Return the index of the given segment;
  size_t segment_index(const SegmentCommand& segment) const;

  //! Return binary's *fat offset*. ``0`` if not relevant.
  uint64_t fat_offset() const;

  // Return binary's @link MachO::SegmentCommand segment command
  // which hold the virtual address
  SegmentCommand*       segment_from_virtual_address(uint64_t virtual_address);
  const SegmentCommand* segment_from_virtual_address(uint64_t virtual_address) const;

  //! Return the range of virtual addresses
  range_t va_ranges() const;

  //! Return the range of offsets
  range_t off_ranges() const;

  //! Check if the given address is comprise between the lowest
  //! virtual address and the biggest one
  bool is_valid_addr(uint64_t address) const;

  //! Method so that the ``visitor`` can visit us
  virtual void accept(LIEF::Visitor& visitor) const override;

  virtual ~Binary();

  virtual std::ostream& print(std::ostream& os) const override;

  // LIEF Interface
  // ==============

  //! Patch the content at virtual address @p address with @p patch_value
  //!
  //! @param[in] address Address to patch
  //! @param[in] patch_value Patch to apply
  //! @param[in] addr_type Specify if the address should be used as an absolute virtual address or an RVA
  virtual void patch_address(uint64_t address, const std::vector<uint8_t>& patch_value, LIEF::Binary::VA_TYPES addr_type = LIEF::Binary::VA_TYPES::AUTO) override;

  //! Patch the address with the given value
  //!
  //! @param[in] address Address to patch
  //! @param[in] patch_value Patch to apply
  //! @param[in] size Size of the value in **bytes** (1, 2, ... 8)
  //! @param[in] addr_type Specify if the address should be used as an absolute virtual address or an RVA
  virtual void patch_address(uint64_t address, uint64_t patch_value, size_t size = sizeof(uint64_t), LIEF::Binary::VA_TYPES addr_type = LIEF::Binary::VA_TYPES::AUTO) override;

  //! Return the content located at virtual address
  virtual std::vector<uint8_t> get_content_from_virtual_address(uint64_t virtual_address, uint64_t size,
      LIEF::Binary::VA_TYPES addr_type = LIEF::Binary::VA_TYPES::AUTO) const override;

  virtual uint64_t entrypoint() const override;

  //! Check if the binary is position independent
  virtual bool is_pie() const override;

  //! Check if the binary uses ``NX`` protection
  virtual bool has_nx() const override;

  //! ``true`` if the binary has an entrypoint.
  //!
  //! Basically for libraries it will return ``false``
  bool has_entrypoint() const;

  //! ``true`` if the binary has a MachO::UUIDCommand command.
  bool has_uuid() const;

  //! Return the MachO::UUIDCommand
  UUIDCommand&       uuid();
  const UUIDCommand& uuid() const;

  //! ``true`` if the binary has a MachO::MainCommand command.
  bool has_main_command() const;

  //! Return the MachO::MainCommand
  MainCommand&       main_command();
  const MainCommand& main_command() const;

  //! ``true`` if the binary has a MachO::DylinkerCommand.
  bool has_dylinker() const;

  //! Return the MachO::DylinkerCommand
  DylinkerCommand&       dylinker();
  const DylinkerCommand& dylinker() const;

  //! ``true`` if the binary has a MachO::DyldInfo command.
  bool has_dyld_info() const;

  //! Return the MachO::Dyld command
  DyldInfo&       dyld_info();
  const DyldInfo& dyld_info() const;

  //! ``true`` if the binary has a MachO::FunctionStarts command.
  bool has_function_starts() const;

  //! Return the MachO::FunctionStarts command
  FunctionStarts&       function_starts();
  const FunctionStarts& function_starts() const;

  //! ``true`` if the binary has a MachO::SourceVersion command.
  bool has_source_version() const;

  //! Return the MachO::SourceVersion command
  SourceVersion&       source_version();
  const SourceVersion& source_version() const;

  //! ``true`` if the binary has a MachO::VersionMin command.
  bool has_version_min() const;

  //! Return the MachO::VersionMin command
  VersionMin&       version_min();
  const VersionMin& version_min() const;


  //! ``true`` if the binary has a MachO::ThreadCommand command.
  bool has_thread_command() const;

  //! Return the MachO::ThreadCommand command
  ThreadCommand&       thread_command();
  const ThreadCommand& thread_command() const;

  //! ``true`` if the binary has a MachO::RPathCommand command.
  bool has_rpath() const;

  //! Return the MachO::RPathCommand command
  RPathCommand&       rpath();
  const RPathCommand& rpath() const;

  //! ``true`` if the binary has a MachO::SymbolCommand command.
  bool has_symbol_command() const;

  //! Return the MachO::SymbolCommand
  SymbolCommand&       symbol_command();
  const SymbolCommand& symbol_command() const;

  //! ``true`` if the binary has a MachO::DynamicSymbolCommand command.
  bool has_dynamic_symbol_command() const;

  //! Return the MachO::SymbolCommand
  DynamicSymbolCommand&       dynamic_symbol_command();
  const DynamicSymbolCommand& dynamic_symbol_command() const;

  //! ``true`` if the binary is signed with `LC_CODE_SIGNATURE` command
  bool has_code_signature() const;

  //! Return the MachO::CodeSignature
  CodeSignature&       code_signature();
  const CodeSignature& code_signature() const;

  //! ``true`` if the binary is signed with the command `DYLIB_CODE_SIGN_DRS`
  bool has_code_signature_dir() const;

  //! Return the MachO::CodeSignature
  CodeSignature&       code_signature_dir();
  const CodeSignature& code_signature_dir() const;

  //! ``true`` if the binaryhas a MachO::DataInCode command.
  bool has_data_in_code() const;

  //! Return the MachO::Signature
  DataInCode&       data_in_code();
  const DataInCode& data_in_code() const;

  //! ``true`` if the binary has segment split info.
  bool has_segment_split_info() const;

  //! Return the MachO::SegmentSplitInfo
  SegmentSplitInfo&       segment_split_info();
  const SegmentSplitInfo& segment_split_info() const;

  //! ``true`` if the binary has a sub framework command.
  bool has_sub_framework() const;

  //! @brief ``true`` if the binary has Encryption Info.
  bool has_encryption_info() const;

  //! @brief Return the MachO::DyldEnvironment
  EncryptionInfo&       encryption_info();
  const EncryptionInfo& encryption_info() const;

  //! Return the MachO::SubFramework
  SubFramework&       sub_framework();
  const SubFramework& sub_framework() const;

  //! ``true`` if the binary has Dyld envrionment variables.
  bool has_dyld_environment() const;

  //! Return the MachO::DyldEnvironment
  DyldEnvironment&       dyld_environment();
  const DyldEnvironment& dyld_environment() const;

  //! ``true`` if the binary has Build Version command.
  bool has_build_version() const;

  //! Return the MachO::BuildVersion
  BuildVersion&       build_version();
  const BuildVersion& build_version() const;

  template<class T>
  LIEF_LOCAL bool has_command() const;

  template<class T>
  LIEF_LOCAL T& command();

  template<class T>
  LIEF_LOCAL const T& command() const;

  template<class T>
  size_t count_commands() const;

  LoadCommand&       operator[](LOAD_COMMAND_TYPES type);
  const LoadCommand& operator[](LOAD_COMMAND_TYPES type) const;

  virtual LIEF::Binary::functions_t ctor_functions() const override;
  LIEF::Binary::functions_t functions() const;
  LIEF::Binary::functions_t unwind_functions() const;

  //! ``true`` if the binary has filesets.
  bool has_filesets() const;

  private:
  //! Default constructor
  Binary();

  // Shift content next to LC table
  void shift(size_t value);

  void shift_command(size_t width, size_t from_offset);

  //! Insert Segment command in the cache field (segments_)
  //! and keep a consistent state of the indexes.
  size_t add_cached_segment(SegmentCommand& segment);

  template<class T>
  LIEF_LOCAL void patch_relocation(Relocation& relocation, uint64_t from, uint64_t shift);

  virtual LIEF::Header              get_abstract_header() const override;
  virtual LIEF::sections_t          get_abstract_sections() override;
  virtual LIEF::symbols_t           get_abstract_symbols() override;
  virtual LIEF::relocations_t       get_abstract_relocations() override;
  virtual LIEF::Binary::functions_t get_abstract_exported_functions() const override;
  virtual LIEF::Binary::functions_t get_abstract_imported_functions() const override;
  virtual std::vector<std::string>  get_abstract_imported_libraries() const override;

  inline relocations_t& relocations_list() {
    return this->relocations_;
  }

  inline const relocations_t& relocations_list() const {
    return this->relocations_;
  }

  inline size_t pointer_size() const {
    return this->is64_ ? sizeof(uint64_t) : sizeof(uint32_t);
  }

  bool        is64_;
  Header      header_;
  commands_t  commands_;
  symbols_t   symbols_;
  libraries_t libraries_;
  sections_t  sections_;
  segments_t  segments_;

  std::vector<std::unique_ptr<Binary>> filesets_;

  // Cached relocations from segment / sections
  mutable relocations_t relocations_;
  int32_t available_command_space_ = 0;

  // This is used to improve performances of
  // offset_to_virtual_address
  std::map<uint64_t, SegmentCommand*> offset_seg_;


  protected:
  uint64_t fat_offset_ = 0;
  uint64_t fileset_offset_ = 0;
};

} // namespace MachO
} // namespace LIEF
#endif
