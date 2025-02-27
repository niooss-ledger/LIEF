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
#include "logging.hpp"
#include "LIEF/PE/LoadConfigurations.hpp"

#include "LoadConfigurations/LoadConfigurations.tcc"

namespace LIEF {
namespace PE {

template<typename PE_T>
void Parser::parse() {

  if (not this->parse_headers<PE_T>()) {
    return;
  }

  LIEF_DEBUG("[+] Processing DOS stub & Rich header");

  this->parse_dos_stub();
  this->parse_rich_header();

  LIEF_DEBUG("[+] Processing sections");

  try {
    this->parse_sections();
  } catch (const corrupted& e) {
    LIEF_WARN("{}", e.what());
  }

  LIEF_DEBUG("[+] Processing data directories");

  try {
    this->parse_data_directories<PE_T>();
  } catch (const exception& e) {
    LIEF_WARN("{}", e.what());
  }

  try {
    this->parse_symbols();
  } catch (const corrupted& e) {
    LIEF_WARN("{}", e.what());
  }

  this->parse_overlay();
}

template<typename PE_T>
bool Parser::parse_headers() {
  using pe_optional_header = typename PE_T::pe_optional_header;

  //DOS Header
  if (this->stream_->can_read<pe_dos_header>(0)) {
    const auto pe_hdr = this->stream_->peek<pe_dos_header>(0);
    this->binary_->dos_header_ = &pe_hdr;
  } else {
    LIEF_ERR("DOS Header corrupted");
    return false;
  }


  //PE32 Header
  const size_t pe32_header_off = this->binary_->dos_header().addressof_new_exeheader();
  if (this->stream_->can_read<pe_header>(pe32_header_off)) {
    const auto pe_hdr = this->stream_->peek<pe_header>(pe32_header_off);
    this->binary_->header_ = &pe_hdr;
  } else {
    LIEF_ERR("PE32 Header corrupted");
    return false;
  }

  // Optional Header
  const size_t optional_header_off = this->binary_->dos_header().addressof_new_exeheader() + sizeof(pe_header);
  if (this->stream_->can_read<pe_optional_header>(optional_header_off)) {
    const auto pe_opt_header = this->stream_->peek<pe_optional_header>(optional_header_off);
    this->binary_->optional_header_ = &pe_opt_header;
  } else {
    LIEF_ERR("Optional header corrupted");
    return false;
  }

  return true;
}

template<typename PE_T>
void Parser::parse_data_directories() {
  using pe_optional_header = typename PE_T::pe_optional_header;
  const uint32_t directories_offset =
      this->binary_->dos_header().addressof_new_exeheader() +
      sizeof(pe_header) +
      sizeof(pe_optional_header);
  const uint32_t nbof_datadir = static_cast<uint32_t>(DATA_DIRECTORY::NUM_DATA_DIRECTORIES);

  const pe_data_directory* data_directory = this->stream_->peek_array<pe_data_directory>(directories_offset, nbof_datadir, /* check */false);
  if (data_directory == nullptr) {
    LIEF_ERR("Data Directories corrupted!");
    return;
  }

  this->binary_->data_directories_.reserve(nbof_datadir);
  // WARNING: The PE specifications require that the data directory table ends with a null entry (RVA / Size,
  // set to 0).
  // Nevertheless it seems that this requirement is not enforced by the PE loader.
  // The binary bc203f2b6a928f1457e9ca99456747bcb7adbbfff789d1c47e9479aac11598af contains a non-null final
  // data directory (watermarking?)
  for (size_t i = 0; i < nbof_datadir; ++i) {
    std::unique_ptr<DataDirectory> directory{new DataDirectory{&data_directory[i], static_cast<DATA_DIRECTORY>(i)}};
    LIEF_DEBUG("Processing directory #{:d} ()", i, to_string(static_cast<DATA_DIRECTORY>(i)));
    LIEF_DEBUG("  - RVA:  0x{:04x}", data_directory[i].RelativeVirtualAddress);
    LIEF_DEBUG("  - Size: 0x{:04x}", data_directory[i].Size);
    if (directory->RVA() > 0) {
      // Data directory is not always associated with section
      const uint64_t offset = this->binary_->rva_to_offset(directory->RVA());
      try {
        directory->section_ = &(this->binary_->section_from_offset(offset));
      } catch (const LIEF::not_found&) {
        LIEF_WARN("Unable to find the section associated with {}", to_string(static_cast<DATA_DIRECTORY>(i)));
      }
    }
    this->binary_->data_directories_.push_back(directory.release());
  }

  try {
    // Import Table
    if (this->binary_->data_directory(DATA_DIRECTORY::IMPORT_TABLE).RVA() > 0) {
      LIEF_DEBUG("Processing Import Table");
      const uint32_t import_rva = this->binary_->data_directory(DATA_DIRECTORY::IMPORT_TABLE).RVA();
      const uint64_t offset     = this->binary_->rva_to_offset(import_rva);

      try {
        Section& section = this->binary_->section_from_offset(offset);
        section.add_type(PE_SECTION_TYPES::IMPORT);
      } catch (const not_found&) {
        LIEF_WARN("Unable to find the section associated with Import Table");
      }
      this->parse_import_table<PE_T>();
    }
  } catch (const exception& e) {
    LIEF_WARN("{}", e.what());
  }

  // Exports
  if (this->binary_->data_directory(DATA_DIRECTORY::EXPORT_TABLE).RVA() > 0) {
    LIEF_DEBUG("[+] Processing Exports");

    try {
      this->parse_exports();
    } catch (const exception& e) {
      LIEF_WARN("{}", e.what());
    }
  }

  // Signature
  if (this->binary_->data_directory(DATA_DIRECTORY::CERTIFICATE_TABLE).RVA() > 0) {
    try {
      this->parse_signature();
    } catch (const exception& e) {
      LIEF_WARN("{}", e.what());
    }
  }


  // TLS
  if (this->binary_->data_directory(DATA_DIRECTORY::TLS_TABLE).RVA() > 0) {
    LIEF_DEBUG("[+] Decomposing TLS");

    const uint32_t tls_rva = this->binary_->data_directory(DATA_DIRECTORY::TLS_TABLE).RVA();
    const uint64_t offset  = this->binary_->rva_to_offset(tls_rva);
    try {
      Section& section = this->binary_->section_from_offset(offset);
      section.add_type(PE_SECTION_TYPES::TLS);
      this->parse_tls<PE_T>();
    } catch (const not_found&) {
      LIEF_WARN("Unable to find the section associated with TLS");
    } catch (const exception& e) {
      LIEF_WARN("{}", e.what());
    }
  }

  // Load Config
  if (this->binary_->data_directory(DATA_DIRECTORY::LOAD_CONFIG_TABLE).RVA() > 0) {

    const uint32_t load_config_rva = this->binary_->data_directory(DATA_DIRECTORY::LOAD_CONFIG_TABLE).RVA();
    const uint64_t offset          = this->binary_->rva_to_offset(load_config_rva);
    try {
      Section& section = this->binary_->section_from_offset(offset);
      section.add_type(PE_SECTION_TYPES::LOAD_CONFIG);
      this->parse_load_config<PE_T>();
    } catch (const not_found&) {
      LIEF_WARN("Unable to find the section associated with Load Config");
    } catch (const exception& e) {
      LIEF_WARN("{}", e.what());
    }
  }


  // Relocations
  if (this->binary_->data_directory(DATA_DIRECTORY::BASE_RELOCATION_TABLE).RVA() > 0) {

    LIEF_DEBUG("[+] Decomposing relocations");
    const uint32_t relocation_rva = this->binary_->data_directory(DATA_DIRECTORY::BASE_RELOCATION_TABLE).RVA();
    const uint64_t offset         = this->binary_->rva_to_offset(relocation_rva);
    try {
      Section& section = this->binary_->section_from_offset(offset);
      section.add_type(PE_SECTION_TYPES::RELOCATION);
      this->parse_relocations();
    } catch (const not_found&) {
      LIEF_WARN("Unable to find the section associated with relocations");
    } catch (const exception& e) {
      LIEF_WARN("{}", e.what());
    }
  }


  // Debug
  if (this->binary_->data_directory(DATA_DIRECTORY::DEBUG).RVA() > 0) {

    LIEF_DEBUG("[+] Decomposing debug");
    const uint32_t rva    = this->binary_->data_directory(DATA_DIRECTORY::DEBUG).RVA();
    const uint64_t offset = this->binary_->rva_to_offset(rva);
    try {
      Section& section = this->binary_->section_from_offset(offset);
      section.add_type(PE_SECTION_TYPES::DEBUG);
      this->parse_debug();
    } catch (const not_found&) {
      LIEF_WARN("Unable to find the section associated with debug");
    } catch (const exception& e) {
      LIEF_WARN("{}", e.what());
    }
  }


  // Resources
  if (this->binary_->data_directory(DATA_DIRECTORY::RESOURCE_TABLE).RVA() > 0) {

    LIEF_DEBUG("[+] Decomposing resources");
    const uint32_t resources_rva = this->binary_->data_directory(DATA_DIRECTORY::RESOURCE_TABLE).RVA();
    const uint64_t offset        = this->binary_->rva_to_offset(resources_rva);
    try {
      Section& section  = this->binary_->section_from_offset(offset);
      section.add_type(PE_SECTION_TYPES::RESOURCE);
      this->parse_resources();
    } catch (const not_found&) {
      LIEF_WARN("Unable to find the section associated with resources");
    } catch (const exception& e) {
      LIEF_WARN("{}", e.what());
    }

  }
}

template<typename PE_T>
void Parser::parse_import_table() {
  using uint__ = typename PE_T::uint;

  const uint32_t import_rva    = this->binary_->data_directory(DATA_DIRECTORY::IMPORT_TABLE).RVA();
  const uint64_t import_offset = this->binary_->rva_to_offset(import_rva);

  if (not this->stream_->can_read<pe_import>(import_offset)) {
    return;
  }


  this->stream_->setpos(import_offset);
  while (this->stream_->can_read<pe_import>()) {
    pe_import header        = this->stream_->read<pe_import>();
    Import import           = &header;
    import.directory_       = &(this->binary_->data_directory(DATA_DIRECTORY::IMPORT_TABLE));
    import.iat_directory_   = &(this->binary_->data_directory(DATA_DIRECTORY::IAT));
    import.type_            = this->type_;

    if (import.name_RVA_ == 0) {
      LIEF_DEBUG("Name's RVA is null");
      break;
    }

    // Offset to the Import (Library) name
    const uint64_t offset_name = this->binary_->rva_to_offset(import.name_RVA_);
    import.name_               = this->stream_->peek_string_at(offset_name);

    // We assume that a DLL name should be at least 4 length size and "printable
    if (not is_valid_dll_name(import.name())) {
      continue; // skip
    }

    // Offset to import lookup table
    uint64_t LT_offset = 0;

    if (import.import_lookup_table_RVA_ > 0) {
      LT_offset = this->binary_->rva_to_offset(import.import_lookup_table_RVA_);
    }

    // Offset to the import address table
    uint64_t IAT_offset = 0;

    if (import.import_address_table_RVA_ > 0) {
      IAT_offset = this->binary_->rva_to_offset(import.import_address_table_RVA_);
    }

    uint__ IAT = 0, table = 0;

    if (IAT_offset > 0 and this->stream_->can_read<uint__>(IAT_offset)) {
      IAT   = this->stream_->peek<uint__>(IAT_offset);
      table = IAT;
      IAT_offset += sizeof(uint__);
    }

    if (LT_offset > 0 and this->stream_->can_read<uint__>(LT_offset)) {
      table      = this->stream_->peek<uint__>(LT_offset);
      LT_offset += sizeof(uint__);
    }

    size_t idx = 0;

    while (table != 0 or IAT != 0) {
      ImportEntry entry;
      entry.iat_value_ = IAT;
      entry.data_      = table > 0 ? table : IAT; // In some cases, ILT can be corrupted
      entry.type_      = this->type_;
      entry.rva_       = import.import_address_table_RVA_ + sizeof(uint__) * (idx++);

      if (not entry.is_ordinal()) {
        const size_t hint_off = this->binary_->rva_to_offset(entry.hint_name_rva());
        const size_t name_off = hint_off + sizeof(uint16_t);
        entry.name_ = this->stream_->peek_string_at(name_off);
        if (this->stream_->can_read<uint16_t>(hint_off)) {
          entry.hint_ = this->stream_->peek<uint16_t>(hint_off);
        }

        // Check that the import name is valid
        if (is_valid_import_name(entry.name())) {
          import.entries_.push_back(std::move(entry));
        }
      } else {
        import.entries_.push_back(std::move(entry));
      }

      if (IAT_offset > 0 and this->stream_->can_read<uint__>(IAT_offset)) {
        IAT = this->stream_->peek<uint__>(IAT_offset);
        IAT_offset += sizeof(uint__);
      } else {
        IAT = 0;
      }

      if (LT_offset > 0 and this->stream_->can_read<uint__>(LT_offset)) {
        table = this->stream_->peek<uint__>(LT_offset);
        LT_offset += sizeof(uint__);
      } else {
        table = 0;
      }
    }
    this->binary_->imports_.push_back(std::move(import));
  }

  this->binary_->has_imports_ = this->binary_->imports_.size() > 0;
}

template<typename PE_T>
void Parser::parse_tls() {
  using pe_tls = typename PE_T::pe_tls;
  using uint__ = typename PE_T::uint;

  LIEF_DEBUG("[+] Parsing TLS");

  const uint32_t tls_rva = this->binary_->data_directory(DATA_DIRECTORY::TLS_TABLE).RVA();
  const uint64_t offset  = this->binary_->rva_to_offset(tls_rva);

  this->stream_->setpos(offset);
  if (not this->stream_->can_read<pe_tls>()) {
    return;
  }

  const pe_tls& tls_header = this->stream_->read<pe_tls>();

  TLS& tls = this->binary_->tls_;
  tls = &tls_header;

  const uint64_t imagebase = this->binary_->optional_header().imagebase();


  if (tls_header.RawDataStartVA >= imagebase and tls_header.RawDataEndVA > tls_header.RawDataStartVA) {
    const uint64_t start_data_rva = tls_header.RawDataStartVA - imagebase;
    const uint64_t stop_data_rva  = tls_header.RawDataEndVA - imagebase;

    const uint__ start_template_offset  = this->binary_->rva_to_offset(start_data_rva);
    const uint__ end_template_offset    = this->binary_->rva_to_offset(stop_data_rva);

    const size_t size_to_read = end_template_offset - start_template_offset;

    if (size_to_read > Parser::MAX_DATA_SIZE) {
      LIEF_DEBUG("TLS's template is too large!");
    } else {
      const uint8_t* template_ptr = this->stream_->peek_array<uint8_t>(start_template_offset, size_to_read, /* check */false);
      if (template_ptr == nullptr) {
        LIEF_WARN("TLS's template corrupted");
      } else {
        tls.data_template({
            template_ptr,
            template_ptr + size_to_read
        });
      }
    }
  }

  if (tls.addressof_callbacks() > imagebase) {
    uint64_t callbacks_offset = this->binary_->rva_to_offset(tls.addressof_callbacks() - imagebase);
    this->stream_->setpos(callbacks_offset);
    size_t count = 0;
    while (this->stream_->can_read<uint__>() and count++ < Parser::MAX_TLS_CALLBACKS) {
      uint__ callback_rva = this->stream_->read<uint__>();
      if (static_cast<uint32_t>(callback_rva) == 0) {
        break;
      }
      tls.callbacks_.push_back(callback_rva);
    }
  }

  tls.directory_ = &(this->binary_->data_directory(DATA_DIRECTORY::TLS_TABLE));

  try {
    Section& section = this->binary_->section_from_offset(offset);
    tls.section_     = &section;
  } catch (const not_found&) {
    LIEF_WARN("No section associated with TLS");
  }

  this->binary_->has_tls_ = true;
}


template<typename PE_T>
void Parser::parse_load_config() {
  using load_configuration_t    = typename PE_T::load_configuration_t;
  using load_configuration_v0_t = typename PE_T::load_configuration_v0_t;
  using load_configuration_v1_t = typename PE_T::load_configuration_v1_t;
  using load_configuration_v2_t = typename PE_T::load_configuration_v2_t;
  using load_configuration_v3_t = typename PE_T::load_configuration_v3_t;
  using load_configuration_v4_t = typename PE_T::load_configuration_v4_t;
  using load_configuration_v5_t = typename PE_T::load_configuration_v5_t;
  using load_configuration_v6_t = typename PE_T::load_configuration_v6_t;
  using load_configuration_v7_t = typename PE_T::load_configuration_v7_t;

  LIEF_DEBUG("[+] Parsing Load Config");

  const uint32_t ldc_rva = this->binary_->data_directory(DATA_DIRECTORY::LOAD_CONFIG_TABLE).RVA();
  const uint64_t offset  = this->binary_->rva_to_offset(ldc_rva);

  if (not this->stream_->can_read<uint32_t>(offset)) {
    return;
  }

  const uint32_t size = this->stream_->peek<uint32_t>(offset);
  size_t current_size = 0;
  WIN_VERSION version_found = WIN_VERSION::WIN_UNKNOWN;

  for (auto&& p : PE_T::load_configuration_sizes) {
    if (p.second > current_size and p.second <= size) {
      std::tie(version_found, current_size) = p;
    }
  }

  LIEF_DEBUG("Version found: {} (size: 0x{:x})", to_string(version_found), size);
  std::unique_ptr<LoadConfiguration> ld_conf;

  switch (version_found) {

    case WIN_VERSION::WIN_SEH:
      {
        if (not this->stream_->can_read<load_configuration_v0_t>(offset)) {
          break;
        }

        const load_configuration_v0_t& header = this->stream_->peek<load_configuration_v0_t>(offset);
        ld_conf = std::unique_ptr<LoadConfigurationV0>{new LoadConfigurationV0{&header}};
        break;
      }

    case WIN_VERSION::WIN8_1:
      {

        if (not this->stream_->can_read<load_configuration_v1_t>(offset)) {
          break;
        }
        const load_configuration_v1_t& header = this->stream_->peek<load_configuration_v1_t>(offset);
        ld_conf = std::unique_ptr<LoadConfigurationV1>{new LoadConfigurationV1{&header}};
        break;
      }

    case WIN_VERSION::WIN10_0_9879:
      {

        if (not this->stream_->can_read<load_configuration_v2_t>(offset)) {
          break;
        }
        const load_configuration_v2_t& header = this->stream_->peek<load_configuration_v2_t>(offset);
        ld_conf = std::unique_ptr<LoadConfigurationV2>{new LoadConfigurationV2{&header}};
        break;
      }

    case WIN_VERSION::WIN10_0_14286:
      {

        if (not this->stream_->can_read<load_configuration_v3_t>(offset)) {
          break;
        }
        const load_configuration_v3_t& header = this->stream_->peek<load_configuration_v3_t>(offset);

        ld_conf = std::unique_ptr<LoadConfigurationV3>{new LoadConfigurationV3{&header}};
        break;
      }

    case WIN_VERSION::WIN10_0_14383:
      {

        if (not this->stream_->can_read<load_configuration_v4_t>(offset)) {
          break;
        }
        const load_configuration_v4_t& header = this->stream_->peek<load_configuration_v4_t>(offset);

        ld_conf = std::unique_ptr<LoadConfigurationV4>{new LoadConfigurationV4{&header}};
        break;
      }

    case WIN_VERSION::WIN10_0_14901:
      {

        if (not this->stream_->can_read<load_configuration_v5_t>(offset)) {
          break;
        }
        const load_configuration_v5_t& header = this->stream_->peek<load_configuration_v5_t>(offset);

        ld_conf = std::unique_ptr<LoadConfigurationV5>{new LoadConfigurationV5{&header}};
        break;
      }

    case WIN_VERSION::WIN10_0_15002:
      {

        if (not this->stream_->can_read<load_configuration_v6_t>(offset)) {
          break;
        }
        const load_configuration_v6_t& header = this->stream_->peek<load_configuration_v6_t>(offset);

        ld_conf = std::unique_ptr<LoadConfigurationV6>{new LoadConfigurationV6{&header}};
        break;
      }

    case WIN_VERSION::WIN10_0_16237:
      {

        if (not this->stream_->can_read<load_configuration_v7_t>(offset)) {
          break;
        }
        const load_configuration_v7_t& header = this->stream_->peek<load_configuration_v7_t>(offset);

        ld_conf = std::unique_ptr<LoadConfigurationV7>{new LoadConfigurationV7{&header}};
        break;
      }

    case WIN_VERSION::WIN_UNKNOWN:
    default:
      {
        if (not this->stream_->can_read<load_configuration_t>(offset)) {
          break;
        }
        const load_configuration_t& header = this->stream_->peek<load_configuration_t>(offset);
        ld_conf = std::unique_ptr<LoadConfiguration>{new LoadConfiguration{&header}};
      }
  }

  this->binary_->has_configuration_  = static_cast<bool>(ld_conf);
  this->binary_->load_configuration_ = ld_conf.release();
}

}
}
