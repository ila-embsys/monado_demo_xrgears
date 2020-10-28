// Copyright 2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
// Author: Ryan Pavlik <ryan.pavlik@collabora.com>

#include "android_utilities.h"

#include "wrap/android.content.h"
#include "wrap/android.content.pm.h"
#include "wrap/android.os.h"
#include "wrap/android.provider.h"
#include "wrap/android.service.vr.h"
#include "wrap/android.widget.h"
#include "wrap/java.util.h"

#include <sstream>
#include <vector>
#include <android/log.h>

namespace openxr_android {
using wrap::android::content::ComponentName;
using wrap::android::content::Context;
using wrap::android::content::Intent;
using wrap::android::content::pm::PackageManager;
using wrap::android::content::pm::ResolveInfo;
using wrap::android::content::pm::ServiceInfo;
using wrap::android::os::Bundle;
using wrap::android::provider::Settings;
using wrap::android::service::vr::VrListenerService;
using wrap::android::widget::Toast;
using wrap::java::util::List;

#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, "openxr_loader", __VA_ARGS__)
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, "openxr_loader", __VA_ARGS__)

static constexpr const char metadataName[] = "org.khronos.openxr.OpenXRRuntime";
static constexpr const char vrListenerName[] = "android.service.vr.VrListenerService";
static ComponentName getVrListener(ResolveInfo const &resolveInfo) {
    if (resolveInfo.isNull()) {
        return {};
    }
    ServiceInfo serviceInfo = resolveInfo.getServiceInfo();
    if (serviceInfo.isNull()) {
        return {};
    }
    return ComponentName::construct(serviceInfo.getPackageName(), serviceInfo.getName());
}

static bool isListenerEnabled(Context const &context, ResolveInfo const &resolveInfo) {
    auto componentName = getVrListener(resolveInfo);
    if (componentName.isNull()) {
        return false;
    }
    return VrListenerService::isVrModePackageEnabled(context, componentName);
}

/*!
 * Gets the OpenXR runtime shared library name from a ResolveInfo.
 * @param resolveInfo A non-null ResolveInfo from findActiveRuntime()
 * @return A library name string, or an empty string if something went wrong.
 */
static std::string getRuntimeLibraryName(ResolveInfo const &resolveInfo) {
    ServiceInfo serviceInfo = resolveInfo.getServiceInfo();
    if (serviceInfo.isNull()) {
        return {};
    }
    Bundle bundle = serviceInfo.getMetaData();
    if (bundle.isNull()) {
        return {};
    }
    if (!bundle.containsKey(metadataName)) {
        return {};
    }
    return bundle.getString(metadataName);
}

static std::string getRuntimeAbsolutePathName(Context context, ResolveInfo const &resolveInfo) {
    if (resolveInfo.isNull()) {
        return {};
    }

    auto packageName = resolveInfo.getServiceInfo().getPackageName();
    auto info = context.getPackageManager().getApplicationInfo(
        packageName, PackageManager::GET_META_DATA | PackageManager::GET_SHARED_LIBRARY_FILES);
    auto libraryName = getRuntimeLibraryName(resolveInfo);
    auto libraryPath = info.getNativeLibraryDir() + "/" + libraryName;
    return libraryPath;
}
static void launchVrSettings(Context &context, bool okToBreakHistory) {
    Intent intent = Intent::construct(Settings::ACTION_VR_LISTENER_SETTINGS());
    try {
        context.startActivity(intent);
        return;
    } catch (jni::Exception const &e) {
        // wasn't given an Activity Context.
    }
    if (okToBreakHistory) {
        intent.setFlags(Intent::FLAG_ACTIVITY_NEW_TASK());
        context.startActivity(intent);
    }
}

/*!
 * Find the single active OpenXR runtime on the system.
 *
 * @param context An Android context, preferably an Activity Context.
 * @param launchSettingsIfNeeded Whether VR Settings will be launched if there
 * is an OpenXR runtime, but none or too many are enabled.
 * @param evenIfItBreaksHistory Whether VR Settings will be launched if needed,
 * even if context is not an Activity context - this passes
 * Intent.FLAG_ACTIVITY_NEW_TASK to context.startActivity()
 *
 * @return The single runtime's ResolveInfo, or a null ResolveInfo if something
 * went wrong.
 */
static ResolveInfo findActiveRuntime(Context context, bool launchSettingsIfNeeded, bool evenIfItBreaksHistory) {
    PackageManager packageManager = context.getPackageManager();
    Intent intent = Intent::construct(vrListenerName);
    List resolutions = packageManager.queryIntentServices(intent, PackageManager::GET_META_DATA);
    if (resolutions.isNull() || resolutions.size() == 0) {
        ALOGE("No OpenXR runtime installed!");
        return {};
    }
    std::vector<ResolveInfo> runtimes;
    {
        const int n = resolutions.size();
        for (int i = 0; i < n; ++i) {
            ResolveInfo resolveInfo{resolutions.get(i)};
            auto libName = getRuntimeLibraryName(resolveInfo);
            if (!libName.empty()) {
                runtimes.emplace_back(std::move(resolveInfo));
            }
        }
    }
    if (runtimes.empty()) {
        ALOGE("No OpenXR runtime installed!");
        return {};
    }
    std::vector<ResolveInfo> enabledRuntimes;
    for (const auto &runtime : runtimes) {
        if (isListenerEnabled(context, runtime)) {
            enabledRuntimes.emplace_back(runtime);
        }
    }
    if (enabledRuntimes.empty()) {
        ALOGE("No OpenXR runtimes are enabled. Please enable exactly one.");
        if (launchSettingsIfNeeded) {
            launchVrSettings(context, evenIfItBreaksHistory);
        }
        return {};
    }
    if (enabledRuntimes.size() > 1) {
        std::ostringstream ss;
        ss << "More than one OpenXR runtime is enabled. Please disable all but "
              "one OpenXR runtime in the VR Settings. ";

        for (const auto &resolveInfo : enabledRuntimes) {
            ss << resolveInfo.getServiceInfo().getName();
            ss << "(" << getRuntimeLibraryName(resolveInfo) << "); ";
        }

        ALOGE("%s", ss.str().c_str());
        if (launchSettingsIfNeeded) {
            launchVrSettings(context, evenIfItBreaksHistory);
        }
        return {};
    }

    // OK, we got exactly one.
    return enabledRuntimes.front();
}

int getActiveRuntimeAbsolutePath(wrap::android::content::Context const &context, std::string &out_path, bool launchSettingsIfNeeded,
                                 bool evenIfItBreaksHistory) {
    ResolveInfo resolveInfo = findActiveRuntime(context, launchSettingsIfNeeded, evenIfItBreaksHistory);
    if (resolveInfo.isNull()) {
        return -1;
    }
    auto path = getRuntimeAbsolutePathName(context, resolveInfo);
    out_path = path;
    return 0;
}
}  // namespace openxr_android
