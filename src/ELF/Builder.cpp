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
#include <algorithm>
#include <set>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <functional>
#include <map>

#include "LIEF/exception.hpp"
#include "LIEF/utils.hpp"
#include "LIEF/BinaryStream/VectorStream.hpp"
#include "LIEF/ELF/Builder.hpp"

#include "LIEF/ELF/Binary.hpp"
#include "LIEF/ELF/Section.hpp"
#include "LIEF/ELF/Segment.hpp"
#include "LIEF/ELF/Symbol.hpp"
#include "LIEF/ELF/DynamicEntry.hpp"
#include "LIEF/ELF/DynamicEntryArray.hpp"
#include "LIEF/ELF/DynamicEntryLibrary.hpp"
#include "LIEF/ELF/DynamicSharedObject.hpp"
#include "LIEF/ELF/DynamicEntryRunPath.hpp"
#include "LIEF/ELF/DynamicEntryRpath.hpp"
#include "LIEF/ELF/Relocation.hpp"
#include "LIEF/ELF/SymbolVersion.hpp"
#include "LIEF/ELF/SymbolVersionDefinition.hpp"
#include "LIEF/ELF/SymbolVersionAux.hpp"
#include "LIEF/ELF/SymbolVersionRequirement.hpp"
#include "LIEF/ELF/SymbolVersionAuxRequirement.hpp"
#include "LIEF/ELF/Note.hpp"

#include "Builder.tcc"

#include "ExeLayout.hpp"
#include "ObjectFileLayout.hpp"

namespace LIEF {
namespace ELF {


Builder::~Builder() = default;

Builder::Builder(Binary& binary) :
  binary_{&binary},
  layout_{nullptr}
{
  const E_TYPE type = binary.header().file_type();
  switch (type) {
    case E_TYPE::ET_CORE:
    case E_TYPE::ET_DYN:
    case E_TYPE::ET_EXEC:
      {
        layout_ = std::make_unique<ExeLayout>(binary);
        break;
      }

    case E_TYPE::ET_REL:
      {
        layout_ = std::make_unique<ObjectFileLayout>(binary);
        break;
      }

    default:
      {
        LIEF_ERR("ELF {} are not supported", to_string(type));
        std::abort();
      }
  }
  this->ios_.reserve(binary.original_size());
  this->ios_.set_endian_swap(this->should_swap());
}


bool Builder::should_swap() const {
  switch (this->binary_->header().abstract_endianness()) {
#ifdef __BYTE_ORDER__
#if  defined(__ORDER_LITTLE_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    case ENDIANNESS::ENDIAN_BIG:
#elif defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    case ENDIANNESS::ENDIAN_LITTLE:
#endif
      return true;
#endif // __BYTE_ORDER__
    default:
      // we're good (or don't know what to do), consider bytes are in the expected order
      return false;
  }
}


void Builder::build() {
  if(this->binary_->type() == ELF_CLASS::ELFCLASS32) {
    this->build<ELF32>();
  } else {
    this->build<ELF64>();
  }
}

const std::vector<uint8_t>& Builder::get_build() {
  return this->ios_.raw();
}


Builder& Builder::force_relocations(bool flag) {
  config_.force_relocations = flag;
  return *this;
}


void Builder::write(const std::string& filename) const {
  std::ofstream output_file{filename, std::ios::out | std::ios::binary | std::ios::trunc};
  if (not output_file) {
    LIEF_ERR("Can't open {}!", filename);
    return;
  }
  std::vector<uint8_t> content;
  this->ios_.move(content);
  output_file.write(reinterpret_cast<const char*>(content.data()), content.size());
}


uint32_t Builder::sort_dynamic_symbols() {
  static const std::string dynsym_section_name = ".dynsym";
  const auto it_begin = std::begin(this->binary_->dynamic_symbols_);
  const auto it_end = std::end(this->binary_->dynamic_symbols_);

  const auto it_first_non_local_symbol =
      std::stable_partition(it_begin, it_end, [] (const Symbol* sym) {
        return sym->binding() == SYMBOL_BINDINGS::STB_LOCAL;
      });

  const uint32_t first_non_local_symbol_index =
      std::distance(it_begin, it_first_non_local_symbol);

  if (this->binary_->has_section(dynsym_section_name)) {
    Section& section = this->binary_->get_section(dynsym_section_name);
    if (section.information() != first_non_local_symbol_index) {
      // TODO: Erase null entries of dynamic symbol table and symbol version
      // table if information of .dynsym section is smaller than null entries
      // num.
      LIEF_DEBUG("information of {} section changes from {:d} to {:d}",
                dynsym_section_name,
                section.information(),
                first_non_local_symbol_index);
      section.information(first_non_local_symbol_index);
    }
  }

  const auto it_first_exported_symbol = std::stable_partition(
      it_first_non_local_symbol, it_end, [] (const Symbol* sym) {
        return sym->shndx() ==
               static_cast<uint16_t>(SYMBOL_SECTION_INDEX::SHN_UNDEF);
      });

  const uint32_t first_exported_symbol_index =
      std::distance(it_begin, it_first_exported_symbol);
  return first_exported_symbol_index;
}


void Builder::build_empty_symbol_gnuhash() {
  LIEF_DEBUG("Build empty GNU Hash");
  auto&& it_gnuhash = std::find_if(
      std::begin(this->binary_->sections_), std::end(this->binary_->sections_),
      [] (const Section* section)
      {
        return section != nullptr and section->type() == ELF_SECTION_TYPES::SHT_GNU_HASH;
      });

  if (it_gnuhash == std::end(this->binary_->sections_)) {
    throw corrupted("Unable to find the .gnu.hash section");
  }

  Section* gnu_hash_section = *it_gnuhash;

  vector_iostream content(this->should_swap());
  const uint32_t nb_buckets = 1;
  const uint32_t shift2     = 0;
  const uint32_t maskwords  = 1;
  const uint32_t symndx     = 1; // 0 is reserved

  // nb_buckets
  content.write_conv<uint32_t>(nb_buckets);

  // symndx
  content.write_conv<uint32_t>(symndx);

  // maskwords
  content.write_conv<uint32_t>(maskwords);

  // shift2
  content.write_conv<uint32_t>(shift2);

  // fill with 0
  content.align(gnu_hash_section->size(), 0);
  gnu_hash_section->content(content.raw());

}



void Builder::build(const Note& note, std::set<Section*>* sections) {
  using value_t = typename note_to_section_map_t::value_type;

  if (sections == nullptr) {
    LIEF_ERR("Section list should not be null");
    return;
  }

  Segment& segment_note = this->binary_->get(SEGMENT_TYPES::PT_NOTE);

  auto range_secname = note_to_section_map.equal_range(note.type());

  const bool known_section = (range_secname.first != range_secname.second);

  const auto it_section_name = std::find_if(
      range_secname.first, range_secname.second,
      [this] (value_t p) {
        return binary_->has_section(p.second);
      });

  bool has_section = (it_section_name != range_secname.second);

  std::string section_name;
  if (has_section) {
    section_name = it_section_name->second;
  } else if (known_section) {
    section_name = range_secname.first->second;
  } else {
    section_name = fmt::format(".note.{:x}", static_cast<uint32_t>(note.type()));
  }

  const std::unordered_map<const Note*, size_t>& offset_map = reinterpret_cast<ExeLayout*>(layout_.get())->note_off_map();
  const auto& it_offset = offset_map.find(&note);

  // Link section and notes
  if (binary_->has(note.type()) and has_section) {
    if (it_offset == std::end(offset_map)) {
      LIEF_ERR("Can't find {}", to_string(note.type()));
      return;
    }
    const size_t note_offset = it_offset->second;
    Section& section = this->binary_->get_section(section_name);
    if (sections->insert(&section).second) {
      section.offset(segment_note.file_offset() + note_offset);
      section.size(note.size());
      section.virtual_address(segment_note.virtual_address() + note_offset);
      // Special process for GNU_PROPERTY:
      // This kind of note has a dedicated segment while others don't
      // Therefore, when relocating this note, we need
      // to update the segment as well.
      if (note.type() == NOTE_TYPES::NT_GNU_PROPERTY_TYPE_0 and
          binary_->has(SEGMENT_TYPES::PT_GNU_PROPERTY)) {
        Segment& seg = binary_->get(SEGMENT_TYPES::PT_GNU_PROPERTY);
        seg.file_offset(section.offset());
        seg.physical_size(section.size());
        seg.virtual_address(section.virtual_address());
        seg.physical_address(section.virtual_address());
        seg.virtual_size(section.size());
      }
    } else /* We already handled this kind of note */ {
      section.virtual_address(0);
      section.size(section.size() + note.size());
    }
  }
}


Section& Builder::array_section(Binary& bin, uint64_t addr) {
  static const std::set<ELF_SECTION_TYPES> ARRAY_TYPES = {
    ELF_SECTION_TYPES::SHT_INIT_ARRAY,
    ELF_SECTION_TYPES::SHT_FINI_ARRAY,
    ELF_SECTION_TYPES::SHT_PREINIT_ARRAY,
  };

  for (Section* section : bin.sections_) {
    if (section->virtual_address() >= addr and
        addr < (section->virtual_address() + section->size())
        and ARRAY_TYPES.count(section->type()) > 0) {
      return *section;
    }
  }
  throw not_found("Can find the section associated with DT_ARRAY");
}

}
}
