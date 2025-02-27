#ifndef LIEF_ELF_EXE_LAYOUT_H_
#define LIEF_ELF_EXE_LAYOUT_H_
#include <LIEF/types.hpp>
#include <LIEF/visibility.h>
#include <LIEF/ELF/Binary.hpp>
#include <LIEF/ELF/Builder.hpp>
#include <LIEF/ELF/Symbol.hpp>
#include <LIEF/iostream.hpp>
#include "logging.hpp"
#include "Layout.hpp"
namespace LIEF {
namespace ELF {

class Note;

//! Compute the size and the offset of the elements
//! needed to rebuild the ELF file.
class LIEF_LOCAL ExeLayout : public Layout {
  public:
  using Layout::Layout;

  template<class ELF_T>
  size_t dynamic_size() {
    // The size of the .dynamic / PT_DYNAMIC area
    // is the number of elements times the size of each element (Elf64_Dyn or Elf32_Dyn)
    using Elf_Dyn = typename ELF_T::Elf_Dyn;
    return binary_->dynamic_entries_.size() * sizeof(Elf_Dyn);
  }

  template<class ELF_T>
  size_t dynstr_size() {
    // The .dynstr section contains:
    // - library names (DT_NEEDED / DT_SONAME / DT_RPATH / DT_RUNPATH)
    // - The symbol names from the .dynsym section
    // - Names associated with:
    //   * Symbol definition
    //   * Symbol version requirement
    //   * Symbol version definition
    if (not raw_dynstr_.empty()) {
      return raw_dynstr_.size();
    }
    LIEF_SW_START(sw);
    // Start with dynamic entries: NEEDED / SONAME etc
    vector_iostream raw_dynstr;
    raw_dynstr.write<uint8_t>(0);
    for (DynamicEntry* entry : binary_->dynamic_entries_) {
      switch (entry->tag()) {
      case DYNAMIC_TAGS::DT_NEEDED:
        {
          const std::string& name = entry->as<DynamicEntryLibrary>()->name();
          offset_name_map_[name] = raw_dynstr.tellp();
          raw_dynstr.write(name);
          break;
        }

      case DYNAMIC_TAGS::DT_SONAME:
        {
          const std::string& name = entry->as<DynamicSharedObject>()->name();
          offset_name_map_[name] = raw_dynstr.tellp();
          raw_dynstr.write(name);
          break;
        }

      case DYNAMIC_TAGS::DT_RPATH:
        {
          const std::string& name = entry->as<DynamicEntryRpath>()->name();
          offset_name_map_[name] = raw_dynstr.tellp();
          raw_dynstr.write(name);
          break;
        }

      case DYNAMIC_TAGS::DT_RUNPATH:
        {
          const std::string& name = entry->as<DynamicEntryRunPath>()->name();
          offset_name_map_[name] = raw_dynstr.tellp();
          raw_dynstr.write(name);
          break;
        }

      default: {}
      }
    }

    // Dynamic symbols names
    size_t offset_counter = raw_dynstr.tellp();
    std::vector<std::string> string_table_optimized =
      Builder::optimize<Symbol, decltype(binary_->dynamic_symbols_)>(binary_->dynamic_symbols_,
                                    [] (const Symbol* sym) { return sym->name(); },
                                    offset_counter,
                                    &offset_name_map_);
    for (const std::string& name : string_table_optimized) {
      raw_dynstr.write(name);
    }

    // Symbol definition
    for (const SymbolVersionDefinition& svd: binary_->symbols_version_definition()) {
      for (const SymbolVersionAux& sva : svd.symbols_aux()) {
        const std::string& sva_name = sva.name();
        auto it = offset_name_map_.find(sva_name);
        if (it != std::end(offset_name_map_)) {
          continue;
        }
        offset_name_map_[sva_name] = raw_dynstr.tellp();
        raw_dynstr.write(sva_name);
      }
    }
    // Symbol version requirement
    for (const SymbolVersionRequirement& svr: binary_->symbols_version_requirement()) {
      const std::string& libname = svr.name();
      auto it = offset_name_map_.find(libname);
      if (it == std::end(offset_name_map_)) {
        offset_name_map_[libname] = raw_dynstr.tellp();
        raw_dynstr.write(libname);
      }
      for (const SymbolVersionAuxRequirement& svar : svr.auxiliary_symbols()) {
        const std::string& name = svar.name();
        auto it = offset_name_map_.find(name);
        if (it != std::end(offset_name_map_)) {
          continue;
        }
        offset_name_map_[name] = raw_dynstr.tellp();
        raw_dynstr.write(name);
      }
    }
    // Symbol version definition
    for (const SymbolVersionDefinition& svd: binary_->symbols_version_definition()) {
      for (const SymbolVersionAux& svar : svd.symbols_aux()) {
        const std::string& name = svar.name();
        auto it = offset_name_map_.find(name);
        if (it != std::end(offset_name_map_)) {
          continue;
        }
        offset_name_map_[name] = raw_dynstr.tellp();
        raw_dynstr.write(name);
      }
    }
    raw_dynstr.move(raw_dynstr_);
    LIEF_SW_END(".dynstr values computed in {}", duration_cast<std::chrono::milliseconds>(sw.elapsed()));
    return raw_dynstr_.size();
  }

  template<class ELF_T>
  size_t dynsym_size() {
    using Elf_Sym = typename ELF_T::Elf_Sym;
    return binary_->dynamic_symbols_.size() * sizeof(Elf_Sym);
  }

  template<class ELF_T>
  size_t static_sym_size() {
    using Elf_Sym = typename ELF_T::Elf_Sym;
    return binary_->static_symbols_.size() * sizeof(Elf_Sym);
  }

  template<class ELF_T>
  size_t dynamic_arraysize(DYNAMIC_TAGS tag) {
    using uint__ = typename ELF_T::uint;
    if (not binary_->has(tag)) {
      return 0;
    }
    return binary_->get(tag).as<const DynamicEntryArray&>()->size() * sizeof(uint__);
  }

  template<class ELF_T>
  size_t note_size() {
    if (raw_notes_.size() > 0) {
      return raw_notes_.size();
    }

    vector_iostream raw_notes/*(should_swap())*/;
    for (const Note& note : binary_->notes()) {
      size_t pos = raw_notes.tellp();
      // First we have to write the length of the Note's name
      const uint32_t namesz = static_cast<uint32_t>(note.name().size() + 1);
      raw_notes.write_conv<uint32_t>(namesz);

      // Then the length of the Note's description
      const uint32_t descsz = static_cast<uint32_t>(note.description().size());
      raw_notes.write_conv<uint32_t>(descsz);

      // Then the note's type
      const NOTE_TYPES type = note.type();
      raw_notes.write_conv<uint32_t>(static_cast<uint32_t>(type));

      // Then we write the note's name
      const std::string& name = note.name();
      raw_notes.write(name);

      // Alignment
      raw_notes.align(sizeof(uint32_t), 0);

      // description content (manipulated in 4 byte/uint32_t chunks)
      const std::vector<uint8_t>& description = note.description();
      const uint32_t *desc_ptr = reinterpret_cast<const uint32_t*>(description.data());
      size_t i = 0;
      for (; i < description.size() / sizeof(uint32_t); i++) {
        raw_notes.write_conv<uint32_t>(desc_ptr[i]);
      }
      if (description.size() % sizeof(uint32_t) != 0) {
        uint32_t padded = 0;
        uint8_t *ptr = reinterpret_cast<uint8_t*>(&padded);
        memcpy(ptr, desc_ptr + i, description.size() % sizeof(uint32_t));
        raw_notes.write_conv<uint32_t>(padded);
      }
      notes_off_map_.emplace(&note, pos);
    }
    raw_notes.move(raw_notes_);
    return raw_notes_.size();
  }

  template<class ELF_T>
  size_t symbol_sysv_hash_size() {
    const SysvHash& sysv_hash = binary_->sysv_hash();
    nchain_ = sysv_hash.nchain();
    if (nchain_ < binary_->dynamic_symbols_.size()) {
      LIEF_DEBUG("nchain of .hash section changes from {:d} to {:d}",
                nchain_, binary_->dynamic_symbols_.size());
      nchain_ = binary_->dynamic_symbols_.size();
    }
    return (sysv_hash.nbucket() + nchain_ + /* header */ 2) * sizeof(uint32_t);
  }

  template<class ELF_T>
  size_t section_table_size() {
    using Elf_Shdr = typename ELF_T::Elf_Shdr;
    return binary_->sections_.size() * sizeof(Elf_Shdr);
  }

  template<class ELF_T>
  size_t symbol_gnu_hash_size() {
    // Mainly inspired from
    // * https://github.com/llvm-mirror/lld/blob/master/ELF/SyntheticSections.cpp
    //
    // Checking is performed here:
    // * https://github.com/lattera/glibc/blob/a2f34833b1042d5d8eeb263b4cf4caaea138c4ad/elf/dl-lookup.c#L228
    //
    // See also:
    // * p.9, https://www.akkadia.org/drepper/dsohowto.pdf
    using uint__ = typename ELF_T::uint;
    if (raw_gnu_hash_.size() > 0) {
      return raw_gnu_hash_.size();
    }
    uint32_t first_exported_symbol_index = 0;
    if (new_symndx_ >= 0) {
      first_exported_symbol_index = new_symndx_;
    } else {
      LIEF_WARN("First exported symbol index not set");
    }

    const GnuHash& gnu_hash = binary_->gnu_hash();

    const uint32_t nb_buckets = gnu_hash.nb_buckets();
    const uint32_t symndx     = first_exported_symbol_index;
    const uint32_t maskwords  = gnu_hash.maskwords();
    const uint32_t shift2     = gnu_hash.shift2();

    const std::vector<uint64_t>& filters = gnu_hash.bloom_filters();
    if (filters.size() > 0 and filters[0] == 0) {
      LIEF_DEBUG("Bloom filter is null");
    }

    if (shift2 == 0) {
      LIEF_DEBUG("Shift2 is null");
    }

    LIEF_DEBUG("Number of buckets       : 0x{:x}", nb_buckets);
    LIEF_DEBUG("First symbol idx        : 0x{:x}", symndx);
    LIEF_DEBUG("Number of bloom filters : 0x{:x}", maskwords);
    LIEF_DEBUG("Shift                   : 0x{:x}", shift2);

    // MANDATORY !
    std::stable_sort(
        std::begin(binary_->dynamic_symbols_) + symndx, std::end(binary_->dynamic_symbols_),
        [&nb_buckets] (const Symbol* lhs, const Symbol* rhs) {
          return
            (dl_new_hash(lhs->name().c_str()) % nb_buckets) <
            (dl_new_hash(rhs->name().c_str()) % nb_buckets);
      });
    it_symbols dynamic_symbols = binary_->dynamic_symbols();

    vector_iostream raw_gnuhash;
    raw_gnuhash.reserve(
        4 * sizeof(uint32_t) +          // header
        maskwords * sizeof(uint__) +    // bloom filters
        nb_buckets * sizeof(uint32_t) + // buckets
        (dynamic_symbols.size() - symndx) * sizeof(uint32_t)); // hash values

    // Write header
    // =================================
    raw_gnuhash
      .write_conv<uint32_t>(nb_buckets)
      .write_conv<uint32_t>(symndx)
      .write_conv<uint32_t>(maskwords)
      .write_conv<uint32_t>(shift2);

    // Compute Bloom filters
    // =================================
    std::vector<uint__> bloom_filters(maskwords, 0);
    size_t C = sizeof(uint__) * 8; // 32 for ELF, 64 for ELF64

    for (size_t i = symndx; i < dynamic_symbols.size(); ++i) {
      const uint32_t hash = dl_new_hash(dynamic_symbols[i].name().c_str());
      const size_t pos = (hash / C) & (gnu_hash.maskwords() - 1);
      uint__ V = (static_cast<uint__>(1) << (hash % C)) |
                 (static_cast<uint__>(1) << ((hash >> gnu_hash.shift2()) % C));
      bloom_filters[pos] |= V;
    }
    for (size_t idx = 0; idx < bloom_filters.size(); ++idx) {
     LIEF_DEBUG("Bloom filter [{:d}]: 0x{:x}", idx, bloom_filters[idx]);
    }

    raw_gnuhash.write_conv_array(bloom_filters);

    // Write buckets and hash
    // =================================
    int previous_bucket = -1;
    size_t hash_value_idx = 0;
    std::vector<uint32_t> buckets(nb_buckets, 0);
    std::vector<uint32_t> hash_values(dynamic_symbols.size() - symndx, 0);

    for (size_t i = symndx; i < dynamic_symbols.size(); ++i) {
      LIEF_DEBUG("Dealing with symbol {}", dynamic_symbols[i]);
      const uint32_t hash = dl_new_hash(dynamic_symbols[i].name().c_str());
      int bucket = hash % nb_buckets;

      if (bucket < previous_bucket) {
        throw corrupted("Previous bucket is greater than the current one ("
            + std::to_string(bucket) + " < " +  std::to_string(previous_bucket) + ")");
      }

      if (bucket != previous_bucket) {
        buckets[bucket] = i;
        previous_bucket = bucket;
        if (hash_value_idx > 0) {
          hash_values[hash_value_idx - 1] |= 1;
        }
      }

      hash_values[hash_value_idx] = hash & ~1;
      ++hash_value_idx;
    }

    if (hash_value_idx > 0) {
      hash_values[hash_value_idx - 1] |= 1;
    }

    raw_gnuhash
      .write_conv_array<uint32_t>(buckets)
      .write_conv_array<uint32_t>(hash_values);
    raw_gnuhash.move(raw_gnu_hash_);
    return raw_gnu_hash_.size();
  }

  template<class ELF_T>
  size_t dynamic_relocations_size() {
    using Elf_Rela   = typename ELF_T::Elf_Rela;
    using Elf_Rel    = typename ELF_T::Elf_Rel;
    const it_dynamic_relocations& dyn_relocs = binary_->dynamic_relocations();
    size_t computed_size = 0;
    bool is_rela = binary_->has(DYNAMIC_TAGS::DT_RELA);
    if (is_rela) {
      computed_size += dyn_relocs.size() * sizeof(Elf_Rela);
    } else {
      computed_size += dyn_relocs.size() * sizeof(Elf_Rel);
    }

    return computed_size;
  }

  template<class ELF_T>
  size_t pltgot_relocations_size() {
    using Elf_Rela   = typename ELF_T::Elf_Rela;
    using Elf_Rel    = typename ELF_T::Elf_Rel;
    const it_pltgot_relocations& pltgot_relocs = binary_->pltgot_relocations();
    bool is_rela = binary_->has(DYNAMIC_TAGS::DT_PLTREL) and
       binary_->get(DYNAMIC_TAGS::DT_PLTREL).value() == static_cast<uint64_t>(DYNAMIC_TAGS::DT_RELA);

    if (is_rela) {
      return pltgot_relocs.size() * sizeof(Elf_Rela);
    }
    return pltgot_relocs.size() * sizeof(Elf_Rel);
  }

  template<class ELF_T>
  size_t symbol_version() {
    return binary_->symbol_version_table_.size() * sizeof(uint16_t);
  }

  template<class ELF_T>
  size_t symbol_vdef_size() {
    using Elf_Verdef  = typename ELF_T::Elf_Verdef;
    using Elf_Verdaux = typename ELF_T::Elf_Verdaux;
    size_t computed_size = 0;
    for (const SymbolVersionDefinition& svd: binary_->symbols_version_definition()) {
      computed_size += sizeof(Elf_Verdef) + svd.symbols_aux().size() * sizeof(Elf_Verdaux);
    }
    return computed_size;
  }

  template<class ELF_T>
  size_t symbol_vreq_size() {
    using Elf_Verneed = typename ELF_T::Elf_Verneed;
    using Elf_Vernaux = typename ELF_T::Elf_Vernaux;
    size_t computed_size = 0;
    for (const SymbolVersionRequirement& svr: binary_->symbols_version_requirement()) {
      computed_size += sizeof(Elf_Verneed) + svr.auxiliary_symbols().size() * sizeof(Elf_Vernaux);
    }
    return computed_size;
  }

  template<class ELF_T>
  size_t interpreter_size() {
    // Access private field directly as
    // we want to avoid has_interpreter() check
    return binary_->interpreter_.size() + 1;
  }

  inline void relocate_dynamic(uint64_t size) {
    dynamic_size_ = size;
  }

  inline void relocate_dynstr(bool val) {
    relocate_dynstr_ = val;
  }

  inline void relocate_shstr(bool val) {
    relocate_shstrtab_ = val;
  }

  inline void relocate_strtab(bool val) {
    relocate_strtab_ = val;
  }

  inline void relocate_gnu_hash(bool val) {
    relocate_gnu_hash_ = val;
  }

  inline void relocate_sysv_hash(uint64_t size) {
    sysv_size_ = size;
  }

  inline void relocate_dynsym(uint64_t size) {
    dynsym_size_ = size;
  }

  inline void relocate_symver(uint64_t size) {
    sver_size_ = size;
  }

  inline void relocate_symverd(uint64_t size) {
    sverd_size_ = size;
  }

  inline void relocate_symverr(uint64_t size) {
    sverr_size_ = size;
  }

  inline void relocate_preinit_array(uint64_t size) {
    preinit_size_ = size;
  }

  inline void relocate_init_array(uint64_t size) {
    init_size_ = size;
  }

  inline void relocate_fini_array(uint64_t size) {
    fini_size_ = size;
  }

  inline void relocate_dyn_reloc(uint64_t size) {
    dynamic_reloc_size_ = size;
  }

  inline void relocate_plt_reloc(uint64_t size) {
    pltgot_reloc_size_ = size;
  }

  inline void relocate_interpreter(uint64_t size) {
    interp_size_ = size;
  }

  inline void relocate_notes(bool value) {
    relocate_notes_ = value;
  }

  inline void relocate_symtab(size_t size) {
    symtab_size_ = size;
  }

  inline const std::vector<uint8_t>& raw_dynstr() const {
    return raw_dynstr_;
  }

  inline const std::vector<uint8_t>& raw_shstr() const override {
    return raw_shstrtab_;
  }

  inline const std::vector<uint8_t>& raw_gnuhash() const {
    return raw_gnu_hash_;
  }

  inline const std::vector<uint8_t>& raw_notes() const {
    return raw_notes_;
  }

  void relocate() {
    /* PT_INTERP segment (optional)
     *
     */
    Segment* interp = nullptr;
    if (interp_size_ > 0 and not binary_->has(SEGMENT_TYPES::PT_INTERP)) {
      Segment interp_segment;
      interp_segment.alignment(0x8);
      interp_segment.type(SEGMENT_TYPES::PT_INTERP);
      interp_segment.add(ELF_SEGMENT_FLAGS::PF_R);
      interp_segment.content(std::vector<uint8_t>(interp_size_));
      interp = &binary_->add(interp_segment);
      LIEF_DEBUG("Interp Segment: 0x{:x}:0x{:x}", interp->virtual_address(), interp->virtual_size());
    }

    /* Segment 1.
     *    .interp
     *    .note.*
     *    .gnu.hash
     *    .hash
     *    .dynsym
     *    .dynstr
     *    .gnu.version
     *    .gnu.version_d
     *    .gnu.version_r
     *    .rela.dyn
     *    .rela.plt
     * Perm: READ ONLY
     * Align: 0x1000
     */
    uint64_t read_segment =
      interp_size_ +  sysv_size_ +
      dynsym_size_ +
      sver_size_ + sverd_size_ + sverr_size_ +
      dynamic_reloc_size_ + pltgot_reloc_size_;

    if (relocate_notes_) {
      read_segment += raw_notes_.size();
    }

    if (relocate_dynstr_) {
      read_segment += raw_dynstr_.size();
    }

    if (relocate_gnu_hash_) {
      read_segment += raw_gnu_hash_.size();
    }
    Segment* new_rsegment = nullptr;

    if (read_segment > 0) {
      Segment rsegment;
      rsegment.alignment(0x1000);
      rsegment.type(SEGMENT_TYPES::PT_LOAD);
      rsegment.add(ELF_SEGMENT_FLAGS::PF_R);
      rsegment.content(std::vector<uint8_t>(read_segment));
      new_rsegment = &binary_->add(rsegment);
      LIEF_DEBUG("R-Segment: 0x{:x}:0x{:x}", new_rsegment->virtual_address(), new_rsegment->virtual_size());
    }

    /* Segment 2
     *
     *  .init_array
     *  .fini_array
     *  .preinit_array
     *  .prefini_array
     *  .dynamic
     *  .got
     *  .got.plt
     * Perm: READ | WRITE
     * Align: 0x1000
     */
    const uint64_t read_write_segment =
      init_size_ + preinit_size_ + fini_size_ + dynamic_size_ ;

    Segment* new_rwsegment = nullptr;
    Segment rwsegment;
    if (read_write_segment > 0) {
      rwsegment.alignment(0x1000);
      rwsegment.type(SEGMENT_TYPES::PT_LOAD);
      rwsegment.add(ELF_SEGMENT_FLAGS::PF_R);
      rwsegment.add(ELF_SEGMENT_FLAGS::PF_W);
      rwsegment.content(std::vector<uint8_t>(read_write_segment));
      new_rwsegment = &binary_->add(rwsegment);
      LIEF_DEBUG("RW-Segment: 0x{:x}:0x{:x}", new_rwsegment->virtual_address(), new_rwsegment->virtual_size());
    }



    if (relocate_shstrtab_) {
      LIEF_DEBUG("[-] Relocate .shstrtab");

      // Remove the current .shstrtab section
      Header& hdr = binary_->header();
      Section* string_names_section = binary_->sections_[hdr.section_name_table_idx()];
      binary_->remove(*string_names_section, /* clear */ true);
      std::string sec_name = binary_->shstrtab_name();
      Section sec_str_section(sec_name, ELF_SECTION_TYPES::SHT_STRTAB);
      sec_str_section.content(std::vector<uint8_t>(raw_shstrtab_.size()));
      binary_->add(sec_str_section, /* loaded */ false);

      // Default behavior: push_back => index = binary_->sections_.size() - 1
      hdr.section_name_table_idx(binary_->sections_.size() - 1);
    }

    for (Relocation* reloc : binary_->relocations_) {
      relocations_addresses_[reloc->address()] = reloc;
    }

    uint64_t va_r_base  = new_rsegment  != nullptr ? new_rsegment->virtual_address() : 0;
    uint64_t va_rw_base = new_rwsegment != nullptr ? new_rwsegment->virtual_address() : 0;

    if (interp_size_ > 0) {
      Segment& pt_interp = binary_->get(SEGMENT_TYPES::PT_INTERP);
      Section* section = nullptr;
      it_sections sections = pt_interp.sections();
      if (sections.size() > 0) {
        section = &sections[0];
      }
      pt_interp.virtual_address(va_r_base);
      pt_interp.virtual_size(interp_size_);
      pt_interp.physical_address(va_r_base);
      pt_interp.file_offset(binary_->virtual_address_to_offset(va_r_base));
      pt_interp.physical_size(interp_size_);
      if (section != nullptr) {
        section->virtual_address(va_r_base);
        section->size(interp_size_);
        section->offset(binary_->virtual_address_to_offset(va_r_base));
        section->original_size_ = interp_size_;
      }

      va_r_base += interp_size_;
    }

    if (relocate_notes_) {
      Segment& note_segment = binary_->get(SEGMENT_TYPES::PT_NOTE);
      note_segment.virtual_address(va_r_base);
      note_segment.virtual_size(raw_notes_.size());
      note_segment.physical_address(va_r_base);
      note_segment.file_offset(binary_->virtual_address_to_offset(va_r_base));
      note_segment.physical_size(raw_notes_.size());
      va_r_base += raw_notes_.size();
    }

    if (dynamic_size_ > 0) {
      // Update .dynamic / PT_DYNAMIC
      // Update relocations associated with .init_array etc
      Segment& dynamic_segment = binary_->get(SEGMENT_TYPES::PT_DYNAMIC);
      Section& dynamic_section = binary_->dynamic_section();

      dynamic_segment.virtual_address(va_rw_base);
      dynamic_segment.virtual_size(dynamic_size_);
      dynamic_segment.physical_address(va_rw_base);
      dynamic_segment.file_offset(binary_->virtual_address_to_offset(va_rw_base));
      dynamic_segment.physical_size(dynamic_size_);

      dynamic_section.virtual_address(va_rw_base);
      dynamic_section.size(dynamic_size_);
      dynamic_section.offset(binary_->virtual_address_to_offset(va_rw_base));
      dynamic_section.original_size_ = dynamic_size_;
      va_rw_base += dynamic_size_;
    }

    if (dynsym_size_ > 0) {
      // Update .dynsym / DT_SYMTAB
      DynamicEntry& dt_symtab = binary_->get(DYNAMIC_TAGS::DT_SYMTAB);
      Section& dyn_sym_section  = binary_->section_from_virtual_address(dt_symtab.value());

      dyn_sym_section.virtual_address(va_r_base);
      dyn_sym_section.size(dynsym_size_);
      dyn_sym_section.offset(binary_->virtual_address_to_offset(va_r_base));
      dyn_sym_section.original_size_ = dynsym_size_;
      dt_symtab.value(va_r_base);

      va_r_base += dynsym_size_;
    }

    if (relocate_dynstr_) {
      // Update .dynstr section, DT_SYMTAB, DT_STRSZ
      DynamicEntry& dt_strtab  = binary_->get(DYNAMIC_TAGS::DT_STRTAB);
      DynamicEntry& dt_strsize = binary_->get(DYNAMIC_TAGS::DT_STRSZ);
      Section& dyn_str_section = binary_->section_from_virtual_address(dt_strtab.value());

      dyn_str_section.virtual_address(va_r_base);
      dyn_str_section.size(raw_dynstr_.size());
      dyn_str_section.offset(binary_->virtual_address_to_offset(va_r_base));
      dyn_str_section.original_size_ = raw_dynstr_.size();

      dt_strtab.value(va_r_base);
      dt_strsize.value(raw_dynstr_.size());

      va_r_base += raw_dynstr_.size();
    }


    if (sver_size_ > 0) {
      DynamicEntry& dt_versym = binary_->get(DYNAMIC_TAGS::DT_VERSYM);
      Section& versym_section = binary_->section_from_virtual_address(dt_versym.value());

      versym_section.virtual_address(va_r_base);
      versym_section.size(sver_size_);
      versym_section.offset(binary_->virtual_address_to_offset(va_r_base));
      versym_section.original_size_ = sver_size_;
      dt_versym.value(va_r_base);

      va_r_base += sver_size_;
    }

    if (sverd_size_ > 0) {
      DynamicEntry& dt_verdef = binary_->get(DYNAMIC_TAGS::DT_VERDEF);
      Section& verdef_section = binary_->section_from_virtual_address(dt_verdef.value());

      verdef_section.virtual_address(va_r_base);
      verdef_section.size(sverd_size_);
      verdef_section.offset(binary_->virtual_address_to_offset(va_r_base));
      verdef_section.original_size_ = sverd_size_;
      dt_verdef.value(va_r_base);

      va_r_base += sverd_size_;
    }

    if (sverr_size_ > 0) {
      DynamicEntry& dt_verreq = binary_->get(DYNAMIC_TAGS::DT_VERNEED);
      Section& verreq_section = binary_->section_from_virtual_address(dt_verreq.value());

      verreq_section.virtual_address(va_r_base);
      verreq_section.size(sverr_size_);
      verreq_section.offset(binary_->virtual_address_to_offset(va_r_base));
      verreq_section.original_size_ = sverr_size_;
      dt_verreq.value(va_r_base);

      va_r_base += sverr_size_;
    }

    if (dynamic_reloc_size_ > 0) {
      // Update:
      // - DT_REL / DT_RELA
      // - DT_RELSZ / DT_RELASZ
      // - .dyn.rel
      bool is_rela = binary_->has(DYNAMIC_TAGS::DT_RELA);
      DynamicEntry& dt_reloc = is_rela ?
        binary_->get(DYNAMIC_TAGS::DT_RELA) : binary_->get(DYNAMIC_TAGS::DT_REL);

      DynamicEntry& dt_relocsz = is_rela ?
        binary_->get(DYNAMIC_TAGS::DT_RELASZ) : binary_->get(DYNAMIC_TAGS::DT_RELSZ);
      Section& dyn_relocation_section = binary_->section_from_virtual_address(dt_reloc.value());
      LIEF_DEBUG("Update {}", dyn_relocation_section.name());
      dyn_relocation_section.virtual_address(va_r_base);
      dyn_relocation_section.size(dynamic_reloc_size_);
      dyn_relocation_section.offset(binary_->virtual_address_to_offset(va_r_base));
      dyn_relocation_section.original_size_ = dynamic_reloc_size_;

      dt_reloc.value(va_r_base);
      dt_relocsz.value(dynamic_reloc_size_);

      va_r_base += dynamic_reloc_size_;
    }

    if (pltgot_reloc_size_ > 0) {
      // Update:
      // - DT_JMPREL / DT_PLTRELSZ
      // - .plt.rel
      DynamicEntry& dt_reloc = binary_->get(DYNAMIC_TAGS::DT_JMPREL);
      DynamicEntry& dt_relocsz = binary_->get(DYNAMIC_TAGS::DT_PLTRELSZ);
      Section& relocation_section = binary_->section_from_virtual_address(dt_reloc.value());

      relocation_section.virtual_address(va_r_base);
      relocation_section.size(pltgot_reloc_size_);
      relocation_section.offset(binary_->virtual_address_to_offset(va_r_base));
      relocation_section.original_size_ = pltgot_reloc_size_;

      dt_reloc.value(va_r_base);
      dt_relocsz.value(pltgot_reloc_size_);

      va_r_base += pltgot_reloc_size_;
    }


    if (relocate_gnu_hash_) {
      // Update .gnu.hash section / DT_GNU_HASH
      DynamicEntry& dt_gnu_hash = binary_->get(DYNAMIC_TAGS::DT_GNU_HASH);
      Section& section = binary_->section_from_virtual_address(dt_gnu_hash.value());

      section.virtual_address(va_r_base);
      section.size(raw_gnu_hash_.size());
      section.offset(binary_->virtual_address_to_offset(va_r_base));
      section.original_size_ = raw_gnu_hash_.size();

      dt_gnu_hash.value(va_r_base);
      va_r_base += raw_gnu_hash_.size();
    }

    if (sysv_size_ > 0) {
      // Update .hash section / DT_HASH
      DynamicEntry& dt_hash = binary_->get(DYNAMIC_TAGS::DT_HASH);
      Section& section = binary_->section_from_virtual_address(dt_hash.value());

      section.virtual_address(va_r_base);
      section.size(sysv_size_);
      section.offset(binary_->virtual_address_to_offset(va_r_base));
      section.original_size_ = sysv_size_;

      dt_hash.value(va_r_base);
      va_r_base += sysv_size_;
    }


    // RW-Segment
    // ====================================
    if (init_size_ > 0) {  // .init_array
      auto& dt_init_array           = *binary_->get(DYNAMIC_TAGS::DT_INIT_ARRAY).as<DynamicEntryArray>();
      DynamicEntry& dt_init_arraysz = binary_->get(DYNAMIC_TAGS::DT_INIT_ARRAYSZ);
      Section& init_array_section   = binary_->get(ELF_SECTION_TYPES::SHT_INIT_ARRAY);

      // Update relocation range
      if (binary_->header().file_type() == E_TYPE::ET_DYN) {
        LIEF_WARN("Relocating .init_array might not work on Linux.");
        const std::vector<uint64_t>& array = dt_init_array.array();
        const size_t sizeof_p = binary_->type() == ELF_CLASS::ELFCLASS32 ?
          sizeof(uint32_t) : sizeof(uint64_t);

        // Since the values of the .init_array have moved elsewhere,
        // we need to change the relocation associated with the former .init_array
        for (size_t i = 0; i < array.size(); ++i) {
          auto it_reloc = relocations_addresses_.find(dt_init_array.value() + i * sizeof_p);
          if (it_reloc == std::end(relocations_addresses_)) {
            LIEF_ERR("Missing relocation for .init_array[{:d}]: 0x{:x}", i, array[i]);
            continue;
          }
          Relocation* reloc = it_reloc->second;
          reloc->address(va_rw_base + i * sizeof_p);
        }
      }

      init_array_section.virtual_address(va_rw_base);
      init_array_section.size(init_size_);
      init_array_section.offset(binary_->virtual_address_to_offset(va_rw_base));
      init_array_section.original_size_ = init_size_;

      dt_init_array.value(va_rw_base);
      dt_init_arraysz.value(init_size_);

      va_rw_base += init_size_;
    }

    if (preinit_size_ > 0) { // .preinit_array
      auto& dt_preinit_array = *binary_->get(DYNAMIC_TAGS::DT_PREINIT_ARRAY).as<DynamicEntryArray>();
      DynamicEntry& dt_preinit_arraysz = binary_->get(DYNAMIC_TAGS::DT_PREINIT_ARRAYSZ);
      Section& preinit_array_section   = binary_->get(ELF_SECTION_TYPES::SHT_PREINIT_ARRAY);

      if (binary_->header().file_type() == E_TYPE::ET_DYN) {
        const std::vector<uint64_t>& array = dt_preinit_array.array();
        const size_t sizeof_p = binary_->type() == ELF_CLASS::ELFCLASS32 ?
          sizeof(uint32_t) : sizeof(uint64_t);
        LIEF_WARN("Relocating .preinit_array might not work on Linux.");

        for (size_t i = 0; i < array.size(); ++i) {
          auto it_reloc = relocations_addresses_.find(dt_preinit_array.value() + i * sizeof_p);
          if (it_reloc == std::end(relocations_addresses_)) {
            LIEF_ERR("Missing relocation for .preinit_array[{:d}]: 0x{:x}", i, array[i]);
            continue;
          }
          Relocation* reloc = it_reloc->second;
          reloc->address(va_rw_base + i * sizeof_p);
        }
      }

      preinit_array_section.virtual_address(va_rw_base);
      preinit_array_section.size(preinit_size_);
      preinit_array_section.offset(binary_->virtual_address_to_offset(va_rw_base));
      preinit_array_section.original_size_ = preinit_size_;

      dt_preinit_array.value(va_rw_base);
      dt_preinit_arraysz.value(preinit_size_);

      va_rw_base += preinit_size_;
    }


    if (fini_size_ > 0) { // .fini_array
      auto& dt_fini_array           = *binary_->get(DYNAMIC_TAGS::DT_FINI_ARRAY).as<DynamicEntryArray>();
      DynamicEntry& dt_fini_arraysz = binary_->get(DYNAMIC_TAGS::DT_FINI_ARRAYSZ);
      Section& fini_array_section   = binary_->get(ELF_SECTION_TYPES::SHT_FINI_ARRAY);

      // Update relocation range

      if (binary_->header().file_type() == E_TYPE::ET_DYN) {
        const std::vector<uint64_t>& array = dt_fini_array.array();
        const size_t sizeof_p = binary_->type() == ELF_CLASS::ELFCLASS32 ?
          sizeof(uint32_t) : sizeof(uint64_t);

        LIEF_WARN("Relocating .fini_array might not work on Linux.");

        for (size_t i = 0; i < array.size(); ++i) {
          auto it_reloc = relocations_addresses_.find(dt_fini_array.value() + i * sizeof_p);
          if (it_reloc == std::end(relocations_addresses_)) {
            LIEF_ERR("Missing relocation for .fini_array[{:d}]: 0x{:x}", i, array[i]);
            continue;
          }
          Relocation* reloc = it_reloc->second;
          reloc->address(va_rw_base + i * sizeof_p);
        }
      }

      fini_array_section.virtual_address(va_rw_base);
      fini_array_section.size(fini_size_);
      fini_array_section.offset(binary_->virtual_address_to_offset(va_rw_base));
      fini_array_section.original_size_ = fini_size_;

      dt_fini_array.value(va_rw_base);
      dt_fini_arraysz.value(fini_size_);
      va_rw_base += fini_size_;
    }

    // Check if we need to relocate the .strtab that contains
    // symbol's names associated with debug symbol (not mandatory)
    size_t strtab_idx = 0;
    if (relocate_strtab_) {
      LIEF_DEBUG("Relocate .strtab");
      if (is_strtab_shared_shstrtab()) {
        LIEF_ERR("Inconsistency"); // The strtab should be located in the .shstrtab section
        std::abort();
      }
      if (strtab_section_ != nullptr) {
        LIEF_DEBUG("Removing the old section: {} 0x{:x} (size: 0x{:x})",
                   strtab_section_->name(), strtab_section_->file_offset(), strtab_section_->size());
        binary_->remove(*strtab_section_, /* clear */ true);
        strtab_idx = binary_->sections().size() - 1;
      } else {
        strtab_idx = binary_->sections().size();
      }
      Section strtab{".strtab", ELF_SECTION_TYPES::SHT_STRTAB};
      strtab.content(raw_strtab_);
      strtab.alignment(1);
      Section& new_strtab = binary_->add(strtab, /* loaded */ false);
      LIEF_DEBUG("New .strtab section: #{:d} {} 0x{:x} (size: {:x})",
                 strtab_idx, new_strtab.name(), new_strtab.file_offset(), new_strtab.size());
      if (binary_->has(ELF_SECTION_TYPES::SHT_SYMTAB)) {
        Section& sec = binary_->get(ELF_SECTION_TYPES::SHT_SYMTAB);
        LIEF_DEBUG("Link section {} with the new .strtab (idx: #{:d})", sec.name(), strtab_idx);
        sec.link(strtab_idx);
      }
      set_strtab_section(new_strtab);
    }

    if (strtab_section_ != nullptr) {
      strtab_section_->content(raw_strtab_);
    }
    LIEF_DEBUG("strtab_idx: {:d}", strtab_idx);
    // Sections that are not associated with segments (mostly debug information)
    // currently we only handle the static symbol table: .symtab
    if (symtab_size_ > 0) {
      LIEF_DEBUG("Relocate .symtab");
      if (binary_->has(ELF_SECTION_TYPES::SHT_SYMTAB)) {
        Section& sec = binary_->get(ELF_SECTION_TYPES::SHT_SYMTAB);
        if (strtab_idx == 0) {
          strtab_idx = sec.link();
        }
        LIEF_DEBUG("Removing the old section: {} 0x{:x} (size: 0x{:x})",
                   sec.name(), sec.file_offset(), sec.size());
        binary_->remove(sec, /* clear */ true);
      }

      Section symtab{".symtab", ELF_SECTION_TYPES::SHT_SYMTAB};
      symtab.content(std::vector<uint8_t>(symtab_size_));

      const size_t sizeof_sym = binary_->type() == ELF_CLASS::ELFCLASS32 ?
                                sizeof(Elf32_Sym) : sizeof(Elf64_Sym);
      symtab.entry_size(sizeof_sym);
      symtab.alignment(8);
      symtab.link(strtab_idx);
      Section& new_symtab = binary_->add(symtab, /* loaded */ false);
      LIEF_DEBUG("New .symtab section: {} 0x{:x} (size: {:x})",
                 new_symtab.name(), new_symtab.file_offset(), new_symtab.size());
    }

    // Process note sections
    if (binary_->has(SEGMENT_TYPES::PT_NOTE)) {
      using value_t = typename note_to_section_map_t::value_type;
      const Segment& segment_note = this->binary_->get(SEGMENT_TYPES::PT_NOTE);

      for (const Note& note : binary_->notes()) {
        auto range_secname = note_to_section_map.equal_range(note.type());
        const bool known_section = (range_secname.first != range_secname.second);

        const NOTE_TYPES type = note.type();

        const auto it_section_name = std::find_if(
            range_secname.first, range_secname.second,
            [this] (value_t p) {
              return binary_->has_section(p.second);
            });

        bool has_section = (it_section_name != range_secname.second);

        const auto& it_offset = notes_off_map_.find(&note);

        std::string section_name;
        if (has_section) {
          section_name = it_section_name->second;
        } else if (known_section) {
          section_name = range_secname.first->second;
        } else {
          section_name = fmt::format(".note.{:x}", static_cast<uint32_t>(type));
        }

        // If the binary does not have the note "type"
        // but still have the section, then remove the section
        if (not binary_->has(note.type()) and has_section) {
          binary_->remove_section(section_name, true);
        }

        // If the binary has the note type but does not have
        // the section (likly because the user added the note manually)
        // then, create the section
        if (this->binary_->has(type) and not has_section) {
          if (it_offset == std::end(notes_off_map_)) {
            LIEF_ERR("Can't find {}", to_string(type));
            return;
          }
          const size_t note_offset = it_offset->second;

          const Note& note = binary_->get(type);

          Section section{section_name, ELF_SECTION_TYPES::SHT_NOTE};
          section += ELF_SECTION_FLAGS::SHF_ALLOC;

          Section& section_added = binary_->add(section, /*loaded */ false);
          section_added.offset(segment_note.file_offset() + note_offset);
          section_added.size(note.size());
          section.virtual_address(segment_note.virtual_address() + note_offset);
          section_added.alignment(4);
        }
      }
    }
  }

  inline const std::unordered_map<std::string, size_t>& dynstr_map() const {
    return offset_name_map_;
  }

  inline const std::unordered_map<const Note*, size_t>& note_off_map() const {
    return notes_off_map_;
  }

  inline uint32_t sysv_nchain() const {
    return nchain_;
  }

  virtual ~ExeLayout() = default;

  private:
  ExeLayout() = delete;

  std::unordered_map<std::string, size_t> offset_name_map_;
  std::unordered_map<const Note*, size_t> notes_off_map_;

  std::vector<uint8_t> raw_notes_;
  bool relocate_notes_{false};

  std::vector<uint8_t> raw_dynstr_;
  bool relocate_dynstr_{false};

  bool relocate_shstrtab_{false};

  bool relocate_strtab_{false};

  std::vector<uint8_t> raw_gnu_hash_;
  bool relocate_gnu_hash_{false};

  uint64_t sysv_size_{0};

  uint64_t dynamic_size_{0};
  uint64_t dynsym_size_{0};

  uint64_t pltgot_reloc_size_{0};
  uint64_t dynamic_reloc_size_{0};

  uint64_t sver_size_{0};
  uint64_t sverd_size_{0};
  uint64_t sverr_size_{0};

  uint64_t preinit_size_{0};
  uint64_t init_size_{0};
  uint64_t fini_size_{0};

  uint64_t interp_size_{0};
  uint32_t nchain_{0};
  uint64_t symtab_size_{0};

  //uint64_t pltgot_reloc_size_{0};
  std::unordered_map<uint64_t, Relocation*> relocations_addresses_;
};
}
}

#endif
