// Copyright (c) 2017-2020 The Khronos Group Inc.
// Copyright (c) 2017-2019 Valve Corporation
// Copyright (c) 2017-2019 LunarG, Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: Mark Young <marky@lunarg.com>
// Author: Dave Houlton <daveh@lunarg.com>
//

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif  // defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)

#include "manifest_file.hpp"

#ifdef OPENXR_HAVE_COMMON_CONFIG
#include "common_config.h"
#endif  // OPENXR_HAVE_COMMON_CONFIG

#include "loader_platform.hpp"
#include "platform_utils.hpp"
#include "loader_logger.hpp"

#include <json/json.h>
#include <openxr/openxr.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef FALLBACK_CONFIG_DIRS
#define FALLBACK_CONFIG_DIRS "/etc/xdg"
#endif  // !FALLBACK_CONFIG_DIRS

#ifndef FALLBACK_DATA_DIRS
#define FALLBACK_DATA_DIRS "/usr/local/share:/usr/share"
#endif  // !FALLBACK_DATA_DIRS

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif  // !SYSCONFDIR

#ifdef XRLOADER_DISABLE_EXCEPTION_HANDLING
#if JSON_USE_EXCEPTIONS
#error \
    "Loader is configured to not catch exceptions, but jsoncpp was built with exception-throwing enabled, which could violate the C ABI. One of those two things needs to change."
#endif  // JSON_USE_EXCEPTIONS
#endif  // !XRLOADER_DISABLE_EXCEPTION_HANDLING

#include "runtime_interface.hpp"

// Utility functions for finding files in the appropriate paths

static inline bool StringEndsWith(const std::string &value, const std::string &ending) {
    if (ending.size() > value.size()) {
        return false;
    }
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

// If the file found is a manifest file name, add it to the out_files manifest list.
static void AddIfJson(const std::string &full_file, std::vector<std::string> &manifest_files) {
    if (full_file.empty() || !StringEndsWith(full_file, ".json")) {
        return;
    }
    manifest_files.push_back(full_file);
}

// Copy all paths listed in the cur_path string into output_path and append the appropriate relative_path onto the end of each.
static void CopyIncludedPaths(bool is_directory_list, const std::string &cur_path, const std::string &relative_path,
                              std::string &output_path) {
    if (!cur_path.empty()) {
        std::size_t last_found = 0;
        std::size_t found = cur_path.find_first_of(PATH_SEPARATOR);

        // Handle any path listings in the string (separated by the appropriate path separator)
        while (found != std::string::npos) {
            std::size_t length = found - last_found;
            output_path += cur_path.substr(last_found, length);
            if (is_directory_list && (cur_path[found - 1] != '\\' && cur_path[found - 1] != '/')) {
                output_path += DIRECTORY_SYMBOL;
            }
            output_path += relative_path;
            output_path += PATH_SEPARATOR;

            last_found = found;
            found = cur_path.find_first_of(PATH_SEPARATOR, found + 1);
        }

        // If there's something remaining in the string, copy it over
        size_t last_char = cur_path.size() - 1;
        if (last_found != last_char) {
            output_path += cur_path.substr(last_found);
            if (is_directory_list && (cur_path[last_char] != '\\' && cur_path[last_char] != '/')) {
                output_path += DIRECTORY_SYMBOL;
            }
            output_path += relative_path;
            output_path += PATH_SEPARATOR;
        }
    }
}

// Look for data files in the provided paths, but first check the environment override to determine if we should use that instead.
static void ReadDataFilesInSearchPaths(ManifestFileType type, const std::string &override_env_var, const std::string &relative_path,
                                       bool &override_active) {
    bool is_directory_list = true;
    bool is_runtime = (type == MANIFEST_TYPE_RUNTIME);
    std::string override_path;
    std::string search_path;

    if (!override_env_var.empty()) {
        bool permit_override = true;
#ifndef XR_OS_WINDOWS
        if (geteuid() != getuid() || getegid() != getgid()) {
            // Don't allow setuid apps to use the env var
            permit_override = false;
        }
#endif
        if (permit_override) {
            override_path = PlatformUtilsGetSecureEnv(override_env_var.c_str());
            if (!override_path.empty()) {
                // The runtime override is actually a specific list of filenames, not directories
                if (is_runtime) {
                    is_directory_list = false;
                }
            }
        }
    }

    if (!override_path.empty()) {
        CopyIncludedPaths(is_directory_list, override_path, "", search_path);
        override_active = true;
    } else {
        override_active = false;
        (void)relative_path;
    }

}

ManifestFile::ManifestFile(ManifestFileType type, const std::string &filename, const std::string &library_path)
    : _filename(filename), _type(type), _library_path(library_path) {}

bool ManifestFile::IsValidJson(const Json::Value &root_node, JsonVersion &version) {
    if (root_node["file_format_version"].isNull() || !root_node["file_format_version"].isString()) {
        LoaderLogger::LogErrorMessage("", "ManifestFile::IsValidJson - JSON file missing \"file_format_version\"");
        return false;
    }
    std::string file_format = root_node["file_format_version"].asString();
    const int num_fields = sscanf(file_format.c_str(), "%u.%u.%u", &version.major, &version.minor, &version.patch);

    // Only version 1.0.0 is defined currently.  Eventually we may have more version, but
    // some of the versions may only be valid for layers or runtimes specifically.
    if (num_fields != 3 || version.major != 1 || version.minor != 0 || version.patch != 0) {
        std::ostringstream error_ss;
        error_ss << "ManifestFile::IsValidJson - JSON \"file_format_version\" " << version.major << "." << version.minor << "."
                 << version.patch << " is not supported";
        LoaderLogger::LogErrorMessage("", error_ss.str());
        return false;
    }

    return true;
}

static void GetExtensionProperties(const std::vector<ExtensionListing> &extensions, std::vector<XrExtensionProperties> &props) {
    for (const auto &ext : extensions) {
        auto it =
            std::find_if(props.begin(), props.end(), [&](XrExtensionProperties &prop) { return prop.extensionName == ext.name; });
        if (it != props.end()) {
            it->extensionVersion = std::max(it->extensionVersion, ext.extension_version);
        } else {
            XrExtensionProperties prop = {};
            prop.type = XR_TYPE_EXTENSION_PROPERTIES;
            prop.next = nullptr;
            strncpy(prop.extensionName, ext.name.c_str(), XR_MAX_EXTENSION_NAME_SIZE - 1);
            prop.extensionName[XR_MAX_EXTENSION_NAME_SIZE - 1] = '\0';
            prop.extensionVersion = ext.extension_version;
            props.push_back(prop);
        }
    }
}

// Return any instance extensions found in the manifest files in the proper form for
// OpenXR (XrExtensionProperties).
void ManifestFile::GetInstanceExtensionProperties(std::vector<XrExtensionProperties> &props) {
    GetExtensionProperties(_instance_extensions, props);
}

const std::string &ManifestFile::GetFunctionName(const std::string &func_name) const {
    if (!_functions_renamed.empty()) {
        auto found = _functions_renamed.find(func_name);
        if (found != _functions_renamed.end()) {
            return found->second;
        }
    }
    return func_name;
}

RuntimeManifestFile::RuntimeManifestFile(const std::string &filename, const std::string &library_path)
    : ManifestFile(MANIFEST_TYPE_RUNTIME, filename, library_path) {}

static void ParseExtension(Json::Value const &ext, std::vector<ExtensionListing> &extensions) {
    Json::Value ext_name = ext["name"];
    Json::Value ext_version = ext["extension_version"];

    // Allow "extension_version" as a String or a UInt to maintain backwards compatibility, even though it should be a String.
    // Internal Issue 1411: https://gitlab.khronos.org/openxr/openxr/-/issues/1411
    // Internal MR !1867: https://gitlab.khronos.org/openxr/openxr/-/merge_requests/1867
    if (ext_name.isString() && (ext_version.isString() || ext_version.isUInt())) {
        ExtensionListing ext_listing = {};
        ext_listing.name = ext_name.asString();
        if (ext_version.isUInt()) {
            ext_listing.extension_version = ext_version.asUInt();
        } else {
            ext_listing.extension_version = atoi(ext_version.asString().c_str());
        }
        extensions.push_back(ext_listing);
    }
}

void ManifestFile::ParseCommon(Json::Value const &root_node) {
    const Json::Value &inst_exts = root_node["instance_extensions"];
    if (!inst_exts.isNull() && inst_exts.isArray()) {
        for (const auto &ext : inst_exts) {
            ParseExtension(ext, _instance_extensions);
        }
    }
    const Json::Value &funcs_renamed = root_node["functions"];
    if (!funcs_renamed.isNull() && !funcs_renamed.empty()) {
        for (Json::ValueConstIterator func_it = funcs_renamed.begin(); func_it != funcs_renamed.end(); ++func_it) {
            if (!(*func_it).isString()) {
                LoaderLogger::LogWarningMessage(
                    "", "ManifestFile::ParseCommon " + _filename + " \"functions\" section contains non-string values.");
                continue;
            }
            std::string original_name = func_it.key().asString();
            std::string new_name = (*func_it).asString();
            _functions_renamed.emplace(original_name, new_name);
        }
    }
}

// Find all manifest files in the appropriate search paths/registries for the given type.
XrResult RuntimeManifestFile::FindManifestFiles(ManifestFileType type,
                                                std::vector<std::unique_ptr<RuntimeManifestFile>> &manifest_files) {
    XrResult result = XR_SUCCESS;
    if (MANIFEST_TYPE_RUNTIME != type) {
        LoaderLogger::LogErrorMessage("", "RuntimeManifestFile::FindManifestFiles - unknown manifest file requested");
        return XR_ERROR_FILE_ACCESS_ERROR;
    }
    std::string filename = PlatformUtilsGetSecureEnv(OPENXR_RUNTIME_JSON_ENV_VAR);
    if (!filename.empty()) {
        LoaderLogger::LogInfoMessage(
            "", "RuntimeManifestFile::FindManifestFiles - using environment variable override runtime file " + filename);
    } else {
#if defined(XR_KHR_LOADER_INIT_SUPPORT)
        std::string lib_path;
        result = GetPlatformRuntimeLocation(lib_path);
        if (XR_SUCCESS == result) {
            manifest_files.emplace_back(new RuntimeManifestFile("", lib_path));
        }
#else
        if (!PlatformGetGlobalRuntimeFileName(XR_VERSION_MAJOR(XR_CURRENT_API_VERSION), filename)) {
            LoaderLogger::LogErrorMessage(
                "", "RuntimeManifestFile::FindManifestFiles - failed to determine active runtime file path for this environment");
            return XR_ERROR_FILE_ACCESS_ERROR;
        }
#endif
        LoaderLogger::LogInfoMessage("", "RuntimeManifestFile::FindManifestFiles - using global runtime file " + filename);
    }

    return result;
}

ApiLayerManifestFile::ApiLayerManifestFile(ManifestFileType type, const std::string &filename, const std::string &layer_name,
                                           const std::string &description, const JsonVersion &api_version,
                                           const uint32_t &implementation_version, const std::string &library_path)
    : ManifestFile(type, filename, library_path),
      _api_version(api_version),
      _layer_name(layer_name),
      _description(description),
      _implementation_version(implementation_version) {}

void ApiLayerManifestFile::CreateIfValid(ManifestFileType type, const std::string &filename,
                                         std::vector<std::unique_ptr<ApiLayerManifestFile>> &manifest_files) {
    std::ifstream json_stream(filename, std::ifstream::in);

    std::ostringstream error_ss("ApiLayerManifestFile::CreateIfValid ");
    if (!json_stream.is_open()) {
        error_ss << "failed to open " << filename << ".  Does it exist?";
        LoaderLogger::LogErrorMessage("", error_ss.str());
        return;
    }

    Json::CharReaderBuilder builder;
    std::string errors;
    Json::Value root_node = Json::nullValue;
    if (!Json::parseFromStream(builder, json_stream, &root_node, &errors) || root_node.isNull()) {
        error_ss << "failed to parse " << filename << ".";
        if (!errors.empty()) {
            error_ss << " (Error message: " << errors << ")";
        }
        error_ss << " Is it a valid layer manifest file?";
        LoaderLogger::LogErrorMessage("", error_ss.str());
        return;
    }
    JsonVersion file_version = {};
    if (!ManifestFile::IsValidJson(root_node, file_version)) {
        error_ss << "isValidJson indicates " << filename << " is not a valid manifest file.";
        LoaderLogger::LogErrorMessage("", error_ss.str());
        return;
    }

    Json::Value layer_root_node = root_node["api_layer"];

    // The API Layer manifest file needs the "api_layer" root as well as other sub-nodes.
    // If any of those aren't there, fail.
    if (layer_root_node.isNull() || layer_root_node["name"].isNull() || !layer_root_node["name"].isString() ||
        layer_root_node["api_version"].isNull() || !layer_root_node["api_version"].isString() ||
        layer_root_node["library_path"].isNull() || !layer_root_node["library_path"].isString() ||
        layer_root_node["implementation_version"].isNull() || !layer_root_node["implementation_version"].isString()) {
        error_ss << filename << " is missing required fields.  Verify all proper fields exist.";
        LoaderLogger::LogErrorMessage("", error_ss.str());
        return;
    }
    if (MANIFEST_TYPE_IMPLICIT_API_LAYER == type) {
        bool enabled = true;
        // Implicit layers require the disable environment variable.
        if (layer_root_node["disable_environment"].isNull() || !layer_root_node["disable_environment"].isString()) {
            error_ss << "Implicit layer " << filename << " is missing \"disable_environment\"";
            LoaderLogger::LogErrorMessage("", error_ss.str());
            return;
        }
        // Check if there's an enable environment variable provided
        if (!layer_root_node["enable_environment"].isNull() && layer_root_node["enable_environment"].isString()) {
            std::string env_var = layer_root_node["enable_environment"].asString();
            // If it's not set in the environment, disable the layer
            if (!PlatformUtilsGetEnvSet(env_var.c_str())) {
                enabled = false;
            }
        }
        // Check for the disable environment variable, which must be provided in the JSON
        std::string env_var = layer_root_node["disable_environment"].asString();
        // If the env var is set, disable the layer. Disable env var overrides enable above
        if (PlatformUtilsGetEnvSet(env_var.c_str())) {
            enabled = false;
        }

        // Not enabled, so pretend like it isn't even there.
        if (!enabled) {
            error_ss << "Implicit layer " << filename << " is disabled";
            LoaderLogger::LogInfoMessage("", error_ss.str());
            return;
        }
    }
    std::string layer_name = layer_root_node["name"].asString();
    std::string api_version_string = layer_root_node["api_version"].asString();
    JsonVersion api_version = {};
    const int num_fields = sscanf(api_version_string.c_str(), "%u.%u", &api_version.major, &api_version.minor);
    api_version.patch = 0;

    if ((num_fields != 2) || (api_version.major == 0 && api_version.minor == 0) ||
        api_version.major > XR_VERSION_MAJOR(XR_CURRENT_API_VERSION)) {
        error_ss << "layer " << filename << " has invalid API Version.  Skipping layer.";
        LoaderLogger::LogWarningMessage("", error_ss.str());
        return;
    }

    uint32_t implementation_version = atoi(layer_root_node["implementation_version"].asString().c_str());
    std::string library_path = layer_root_node["library_path"].asString();

    std::string description;
    if (!layer_root_node["description"].isNull() && layer_root_node["description"].isString()) {
        description = layer_root_node["description"].asString();
    }

    // Add this layer manifest file
    manifest_files.emplace_back(
        new ApiLayerManifestFile(type, filename, layer_name, description, api_version, implementation_version, library_path));

    // Add any extensions to it after the fact.
    manifest_files.back()->ParseCommon(layer_root_node);
}

void ApiLayerManifestFile::PopulateApiLayerProperties(XrApiLayerProperties &props) const {
    props.layerVersion = _implementation_version;
    props.specVersion = XR_MAKE_VERSION(_api_version.major, _api_version.minor, _api_version.patch);
    strncpy(props.layerName, _layer_name.c_str(), XR_MAX_API_LAYER_NAME_SIZE - 1);
    if (_layer_name.size() >= XR_MAX_API_LAYER_NAME_SIZE - 1) {
        props.layerName[XR_MAX_API_LAYER_NAME_SIZE - 1] = '\0';
    }
    strncpy(props.description, _description.c_str(), XR_MAX_API_LAYER_DESCRIPTION_SIZE - 1);
    if (_description.size() >= XR_MAX_API_LAYER_DESCRIPTION_SIZE - 1) {
        props.description[XR_MAX_API_LAYER_DESCRIPTION_SIZE - 1] = '\0';
    }
}

// Find all layer manifest files in the appropriate search paths/registries for the given type.
XrResult ApiLayerManifestFile::FindManifestFiles(ManifestFileType type) {
    std::string relative_path;
    std::string override_env_var;
    std::string registry_location;

    // Add the appropriate top-level folders for the relative path.  These should be
    // the string "openxr/" followed by the API major version as a string.
    relative_path = OPENXR_RELATIVE_PATH;
    relative_path += std::to_string(XR_VERSION_MAJOR(XR_CURRENT_API_VERSION));

    switch (type) {
        case MANIFEST_TYPE_IMPLICIT_API_LAYER:
            relative_path += OPENXR_IMPLICIT_API_LAYER_RELATIVE_PATH;
            override_env_var = "";
            break;
        case MANIFEST_TYPE_EXPLICIT_API_LAYER:
            relative_path += OPENXR_EXPLICIT_API_LAYER_RELATIVE_PATH;
            override_env_var = OPENXR_API_LAYER_PATH_ENV_VAR;
            break;
        default:
            LoaderLogger::LogErrorMessage("", "ApiLayerManifestFile::FindManifestFiles - unknown manifest file requested");
            return XR_ERROR_FILE_ACCESS_ERROR;
    }

    bool override_active = false;
    ReadDataFilesInSearchPaths(type, override_env_var, relative_path, override_active);

    return XR_SUCCESS;
}
