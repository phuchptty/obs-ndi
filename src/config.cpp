/******************************************************************************
	Copyright (C) 2016-2024 DistroAV <contact@distroav.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, see <https://www.gnu.org/licenses/>.
******************************************************************************/

#include "config.h"

#include "plugin-main.h"

#include <util/config-file.h>

#include <QCoreApplication>

#define SECTION_NAME "NDIPlugin"

// User Settings
#define PARAM_MAIN_OUTPUT_ENABLED "MainOutputEnabled"
#define PARAM_MAIN_OUTPUT_NAME "MainOutputName"
#define PARAM_MAIN_OUTPUT_GROUPS "MainOutputGroups"
#define PARAM_PREVIEW_OUTPUT_ENABLED "PreviewOutputEnabled"
#define PARAM_PREVIEW_OUTPUT_NAME "PreviewOutputName"
#define PARAM_PREVIEW_OUTPUT_GROUPS "PreviewOutputGroups"
#define PARAM_TALLY_PROGRAM_ENABLED "TallyProgramEnabled"
#define PARAM_TALLY_PREVIEW_ENABLED "TallyPreviewEnabled"
#define PARAM_SKIP_UPDATE_VERSION "SkipUpdateVersion"

// App Settings
#define PARAM_AUTO_CHECK_FOR_UPDATES "AutoCheckForUpdates"
#define PARAM_LAST_UPDATE_CHECK "LastUpdateCheck"
#define PARAM_MIN_AUTO_UPDATE_CHECK_INTERVAL_SECONDS "MinAutoUpdateCheckIntervalSeconds"

Config *Config::_instance = nullptr;

// Execution parameters (not stored in any config file)
int Config::UpdateForce = 0;
int Config::UpdateLocalPort = 0;
bool Config::UpdateLastCheckIgnore = false;
int Config::DetectObsNdiForce = 0;

enum ObsConfigType { OBS_CONFIG_STRING, OBS_CONFIG_BOOL };

std::map<std::string, enum ObsConfigType> ConfigTypeMap {
	{PARAM_MAIN_OUTPUT_ENABLED, OBS_CONFIG_BOOL},
	{PARAM_MAIN_OUTPUT_NAME, OBS_CONFIG_STRING},
	{PARAM_MAIN_OUTPUT_GROUPS, OBS_CONFIG_STRING},
	{PARAM_PREVIEW_OUTPUT_ENABLED, OBS_CONFIG_BOOL},
	{PARAM_PREVIEW_OUTPUT_NAME, OBS_CONFIG_STRING},
	{PARAM_PREVIEW_OUTPUT_GROUPS, OBS_CONFIG_STRING},
	{PARAM_TALLY_PROGRAM_ENABLED, OBS_CONFIG_BOOL},
	{PARAM_TALLY_PREVIEW_ENABLED, OBS_CONFIG_BOOL},
	{PARAM_SKIP_UPDATE_VERSION, OBS_CONFIG_STRING}
};

void ProcessCommandLine()
{
	auto arguments = QCoreApplication::arguments();
	for (int i = 0; i < arguments.size(); i++) {
		auto argument = arguments.at(i).toLower();

		//
		// Logging
		//
		if (argument == "--distroav-debug") {
			obs_log(LOG_INFO, "config: DistroAV log level set to `debug`");
			LOG_LEVEL = LOG_DEBUG;
			continue;
		}
		if (argument == "--distroav-verbose") {
			obs_log(LOG_INFO, "config: DistroAV log level set to `verbose`");
			LOG_LEVEL = LOG_VERBOSE;
			continue;
		}
		if (argument.startsWith("--distroav-log")) {
			auto parts = argument.split("=");
			if (parts.size() > 1) {
				auto level = parts.at(1).toLower();
				if (level == "error") {
					obs_log(LOG_INFO, "config: DistroAV log level set to `error`");
					LOG_LEVEL = LOG_ERROR;
				} else if (level == "warning") {
					obs_log(LOG_INFO, "config: DistroAV log level set to `warning`");
					LOG_LEVEL = LOG_WARNING;
				} else if (level == "info") {
					obs_log(LOG_INFO, "config: DistroAV log level set to `info`");
					LOG_LEVEL = LOG_INFO;
				} else if (level == "debug") {
					obs_log(LOG_INFO, "config: DistroAV log level set to `debug`");
					LOG_LEVEL = LOG_DEBUG;
				} else if (level == "verbose") {
					obs_log(LOG_INFO, "config: DistroAV log level set to `verbose`");
					LOG_LEVEL = LOG_VERBOSE;
				}
			}
			obs_log(LOG_DEBUG, "config: DistroAV log level set to %d", LOG_LEVEL);
			continue;
		}

		//
		// Update
		//
		if (argument.startsWith("--distroav-update-force")) {
			auto parts = argument.split("=");
			if (parts.size() > 1) {
				Config::UpdateForce = parts.at(1).toInt();
				obs_log(LOG_INFO, "config: DistroAV update force set to %d", Config::UpdateForce);
			}
			continue;
		}
		if (argument == "--distroav-update-last-check-ignore") {
			obs_log(LOG_INFO, "config: DistroAV update last check ignore enabled");
			Config::UpdateLastCheckIgnore = true;
		}
		if (argument.startsWith("--distroav-update-local")) {
			obs_log(LOG_INFO, "config: DistroAV update host set to Local");
			Config::UpdateLocalPort = DEFAULT_UPDATE_LOCAL_PORT;
			auto parts = argument.split("=");
			if (parts.size() > 1) {
				auto port = parts.at(1).toInt();
				if (port > 0 && port < 65536) {
					Config::UpdateLocalPort = port;
				}
			}
			if (Config::UpdateLocalPort != DEFAULT_UPDATE_LOCAL_PORT) {
				obs_log(LOG_INFO, "config: DistroAV update port set to %d", Config::UpdateLocalPort);
			} else {
				obs_log(LOG_INFO, "config: DistroAV update port using default %d",
					Config::UpdateLocalPort);
			}
			continue;
		}

		//
		// OBS-NDI Detection
		//
		if (argument.startsWith("--distroav-detect-obsndi-force")) {
			auto parts = argument.split("=");
			if (parts.size() > 1) {
				auto force = parts.at(1).toLower();
				if (force == "off") {
					obs_log(LOG_INFO, "config: DistroAV detect OBS-NDI force off");
					Config::DetectObsNdiForce = -1;
				} else if (force == "on") {
					obs_log(LOG_INFO, "config: DistroAV detect OBS-NDI force on");
					Config::DetectObsNdiForce = 1;
				}
			}
		}
	}
}

void MigrateSetting(config_t *from, config_t *to, const char *section, const char *name)
{
	if (!config_has_user_value(from, section, name))
		return;

	if (ConfigTypeMap.count(name) == 0)
		return;

	const enum ObsConfigType type = ConfigTypeMap[name];

	switch (type) {
	case OBS_CONFIG_STRING:
		config_set_string(to, section, name, config_get_string(from, section, name));

		break;
	case OBS_CONFIG_BOOL:
		config_set_bool(to, section, name, config_get_bool(from, section, name));
		break;
	}
	config_remove_value(from, section, name);
	obs_log(LOG_INFO, "config: migrated configuration setting %s", name);
}

Config::Config()
	: OutputEnabled(false),
	  OutputName("OBS PGM"),
	  OutputGroups(""),
	  PreviewOutputEnabled(false),
	  PreviewOutputName("OBS Preview"),
	  PreviewOutputGroups(""),
	  TallyProgramEnabled(true),
	  TallyPreviewEnabled(true)
{
	ProcessCommandLine();
	SetDefaultsToUserStore();
	if (obs_get_version() >= MAKE_SEMANTIC_VERSION(31, 0, 0))
		GlobalToUserMigration();
}

void Config::SetDefaultsToUserStore()
{
	auto obs_config = GetUserConfig();
	if (obs_config) {
		config_set_default_bool(obs_config, SECTION_NAME, PARAM_MAIN_OUTPUT_ENABLED, OutputEnabled);
		config_set_default_string(obs_config, SECTION_NAME, PARAM_MAIN_OUTPUT_NAME, QT_TO_UTF8(OutputName));
		config_set_default_string(obs_config, SECTION_NAME, PARAM_MAIN_OUTPUT_GROUPS, QT_TO_UTF8(OutputGroups));

		config_set_default_bool(obs_config, SECTION_NAME, PARAM_PREVIEW_OUTPUT_ENABLED, PreviewOutputEnabled);
		config_set_default_string(obs_config, SECTION_NAME, PARAM_PREVIEW_OUTPUT_NAME,
					  QT_TO_UTF8(PreviewOutputName));
		config_set_default_string(obs_config, SECTION_NAME, PARAM_PREVIEW_OUTPUT_GROUPS,
					  QT_TO_UTF8(PreviewOutputGroups));

		config_set_default_bool(obs_config, SECTION_NAME, PARAM_TALLY_PROGRAM_ENABLED, TallyProgramEnabled);
		config_set_default_bool(obs_config, SECTION_NAME, PARAM_TALLY_PREVIEW_ENABLED, TallyPreviewEnabled);
	}
}

void Config::GlobalToUserMigration()
{
	auto app_config = GetAppConfig();
	auto user_config = GetUserConfig();

	if (app_config && user_config) {
		MigrateSetting(app_config, user_config, SECTION_NAME, PARAM_MAIN_OUTPUT_ENABLED);
		MigrateSetting(app_config, user_config, SECTION_NAME, PARAM_MAIN_OUTPUT_NAME);
		MigrateSetting(app_config, user_config, SECTION_NAME, PARAM_MAIN_OUTPUT_GROUPS);

		MigrateSetting(app_config, user_config, SECTION_NAME, PARAM_PREVIEW_OUTPUT_ENABLED);
		MigrateSetting(app_config, user_config, SECTION_NAME, PARAM_PREVIEW_OUTPUT_NAME);
		MigrateSetting(app_config, user_config, SECTION_NAME, PARAM_PREVIEW_OUTPUT_GROUPS);

		MigrateSetting(app_config, user_config, SECTION_NAME, PARAM_TALLY_PROGRAM_ENABLED);
		MigrateSetting(app_config, user_config, SECTION_NAME, PARAM_TALLY_PREVIEW_ENABLED);

		MigrateSetting(app_config, user_config, SECTION_NAME, PARAM_SKIP_UPDATE_VERSION);

		config_save(app_config);
		config_save(user_config);
	}
}

void Config::Load()
{
	auto obs_config = GetUserConfig();
	if (obs_config) {
		OutputEnabled = config_get_bool(obs_config, SECTION_NAME, PARAM_MAIN_OUTPUT_ENABLED);
		OutputName = config_get_string(obs_config, SECTION_NAME, PARAM_MAIN_OUTPUT_NAME);
		OutputGroups = config_get_string(obs_config, SECTION_NAME, PARAM_MAIN_OUTPUT_GROUPS);

		PreviewOutputEnabled = config_get_bool(obs_config, SECTION_NAME, PARAM_PREVIEW_OUTPUT_ENABLED);
		PreviewOutputName = config_get_string(obs_config, SECTION_NAME, PARAM_PREVIEW_OUTPUT_NAME);
		PreviewOutputGroups = config_get_string(obs_config, SECTION_NAME, PARAM_PREVIEW_OUTPUT_GROUPS);

		TallyProgramEnabled = config_get_bool(obs_config, SECTION_NAME, PARAM_TALLY_PROGRAM_ENABLED);
		TallyPreviewEnabled = config_get_bool(obs_config, SECTION_NAME, PARAM_TALLY_PREVIEW_ENABLED);
	}
}

void Config::Save()
{
	auto obs_config = GetUserConfig();
	if (obs_config) {
		config_set_bool(obs_config, SECTION_NAME, PARAM_MAIN_OUTPUT_ENABLED, OutputEnabled);
		config_set_string(obs_config, SECTION_NAME, PARAM_MAIN_OUTPUT_NAME, QT_TO_UTF8(OutputName));
		config_set_string(obs_config, SECTION_NAME, PARAM_MAIN_OUTPUT_GROUPS, QT_TO_UTF8(OutputGroups));

		config_set_bool(obs_config, SECTION_NAME, PARAM_PREVIEW_OUTPUT_ENABLED, PreviewOutputEnabled);
		config_set_string(obs_config, SECTION_NAME, PARAM_PREVIEW_OUTPUT_NAME, QT_TO_UTF8(PreviewOutputName));
		config_set_string(obs_config, SECTION_NAME, PARAM_PREVIEW_OUTPUT_GROUPS,
				  QT_TO_UTF8(PreviewOutputGroups));

		config_set_bool(obs_config, SECTION_NAME, PARAM_TALLY_PROGRAM_ENABLED, TallyProgramEnabled);
		config_set_bool(obs_config, SECTION_NAME, PARAM_TALLY_PREVIEW_ENABLED, TallyPreviewEnabled);

		config_save(obs_config);
	}
}

bool Config::AutoCheckForUpdates()
{
	auto obs_config = GetAppConfig();
	if (obs_config) {
		return config_get_bool(obs_config, SECTION_NAME, PARAM_AUTO_CHECK_FOR_UPDATES);
	}
	return false;
}

void Config::AutoCheckForUpdates(bool value)
{
	auto obs_config = GetAppConfig();
	if (obs_config) {
		config_set_bool(obs_config, SECTION_NAME, PARAM_AUTO_CHECK_FOR_UPDATES, value);
		config_save(obs_config);
	}
}

void Config::SkipUpdateVersion(const QVersionNumber &version)
{
	auto obs_config = GetUserConfig();
	if (obs_config) {
		config_set_string(obs_config, SECTION_NAME, PARAM_SKIP_UPDATE_VERSION, QT_TO_UTF8(version.toString()));
		config_save(obs_config);
	}
}

QVersionNumber Config::SkipUpdateVersion()
{
	auto obs_config = GetUserConfig();
	if (obs_config) {
		auto version = config_get_string(obs_config, SECTION_NAME, PARAM_SKIP_UPDATE_VERSION);
		if (version) {
			return QVersionNumber::fromString(version);
		}
	}
	return QVersionNumber();
}

QDateTime Config::LastUpdateCheck()
{
	auto obs_config = GetAppConfig();
	if (obs_config) {
		auto lastCheck = config_get_int(obs_config, SECTION_NAME, PARAM_LAST_UPDATE_CHECK);
		return QDateTime::fromSecsSinceEpoch(lastCheck);
	}
	return QDateTime();
}

void Config::LastUpdateCheck(const QDateTime &dateTime)
{
	auto obs_config = GetAppConfig();
	if (obs_config) {
		config_set_int(obs_config, SECTION_NAME, PARAM_LAST_UPDATE_CHECK, dateTime.toSecsSinceEpoch());
		config_save(obs_config);
	}
}

int Config::MinAutoUpdateCheckIntervalSeconds()
{
	auto obs_config = GetAppConfig();
	if (obs_config) {
		return (int)config_get_int(obs_config, SECTION_NAME, PARAM_MIN_AUTO_UPDATE_CHECK_INTERVAL_SECONDS);
	}
	return 0;
}

void Config::MinAutoUpdateCheckIntervalSeconds(int seconds)
{
	auto obs_config = GetAppConfig();
	if (obs_config) {
		config_set_int(obs_config, SECTION_NAME, PARAM_MIN_AUTO_UPDATE_CHECK_INTERVAL_SECONDS, seconds);
		config_save(obs_config);
	}
}

void Config::Initialize()
{
	if (!_instance) {
		_instance = new Config();
		_instance->Load();
	}
}

Config *Config::Current(bool load)
{
	if (!_instance) {
		Initialize();
	} else if (load) {
		_instance->Load();
	}
	return _instance;
}

void Config::Destroy()
{
	if (_instance) {
		delete _instance;
		_instance = nullptr;
	}
}
