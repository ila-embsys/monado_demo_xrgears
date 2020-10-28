// Copyright 2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
// Author: Ryan Pavlik <ryan.pavlik@collabora.com>

#pragma once

#include "wrap/android.content.h"

#include <string>

namespace wrap::android::content {
class Context;
}  // namespace wrap::android::content
namespace openxr_android {
using wrap::android::content::Context;

/*!
 * Find the single active OpenXR runtime on the system, and return its path.
 *
 * @param context An Android context, preferably an Activity Context.
 * @param[out] out_path The string to populate the path into.
 * @param launchSettingsIfNeeded Whether VR Settings will be launched if there
 * is an OpenXR runtime, but none or too many are enabled.
 * @param evenIfItBreaksHistory Whether VR Settings will be launched if needed,
 * even if context is not an Activity context - this passes
 * Intent.FLAG_ACTIVITY_NEW_TASK to context.startActivity()
 *
 * @return 0 on success, something else on failure.
 */
int getActiveRuntimeAbsolutePath(wrap::android::content::Context const &context, std::string &out_path,
                                 bool launchSettingsIfNeeded = true, bool evenIfItBreaksHistory = false);
}  // namespace openxr_android
