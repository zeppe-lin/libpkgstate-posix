// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include <libpkgstate/snapshot.h>
#include <libpkgstate/state_target_binding.h>

namespace pkgstate::detail {

[[nodiscard]] std::vector<std::uint8_t>
encode_generation_binding(const state_target_binding& binding);

[[nodiscard]] state_target_binding
decode_generation_binding(std::string_view bytes);

[[nodiscard]] std::vector<std::uint8_t>
encode_generation_snapshot(const snapshot& value);

[[nodiscard]] snapshot
decode_generation_snapshot(std::string_view bytes);

} // namespace pkgstate::detail
