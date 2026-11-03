// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
/** @file AmplitudeMenuModes.h
 *  @brief Menu ordering and checked-state logic for exclusive amplitude modes.
 */
#pragma once

#include "ViewState.h"

#include <array>

namespace itsv {

// The five amplitude scaling modes are mutually exclusive. This ordering
// matches the Ampl menu rather than the AmplitudeMode enum's numeric values.
/// Exclusive amplitude modes in the order shown in the Ampl menu.
inline constexpr std::array<AmplitudeMode, 5> amplitudeMenuModes{{
  AmplitudeMode::Auto,
  AmplitudeMode::Fixed,
  AmplitudeMode::ZeroCentered,
  AmplitudeMode::FirstTrace,
  AmplitudeMode::AllSame
}};

/**
 * @brief Determine whether a scaling menu entry matches the active mode.
 * @param displayed Mode represented by the menu item.
 * @param active Current viewer scaling mode.
 * @return True when the entry should have a check mark.
 */
constexpr bool amplitudeMenuChecked(AmplitudeMode displayed, AmplitudeMode active) noexcept
{
  return displayed == active;
}

} // namespace itsv
