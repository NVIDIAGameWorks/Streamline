# Debugging
> **NOTE:**
> This document applies to non-production, development builds only which can be found in `bin/x64/development` directory.  JSON configuration is disabled in production builds.
> Additionally, you will need to turn off any checks for signed libraries when loading Streamline libraries in order to be able to load the non-production libraries.

## How to toggle SL on/off

Place the `sl.interposer.json` file (located in `./scripts/`) in the game's working directory. Edit the following line(s):

```json
{
	"enableInterposer": false,	
}
```

When the game starts, if the flag is set to off, interposing will be completely disabled. This can be used to check for added CPU overhead in games.

## How to override plugin location

Place the `sl.interposer.json` file (located in `./scripts/`) in the game's working directory. Edit the following line(s):

```json
{
	"pathToPlugins": "N:/My/Plugin/Path"
}
```

By default SL looks for plugins next to the executable or in the paths provided by the host application (see sl::Preferences). If `pathToPlugins` is provided in JSON it overrides all these settings.
> **NOTE:**
> The `sl.interposer.dll` still needs to reside in the game's working directory in order to be found and loaded properly.  All other SL plugin dlls should reside in the path referenced in the `pathToPlugins` setting.

## How to override logging settings

Place the `sl.interposer.json` file (located in `./scripts/`) in the game's working directory. Edit the following line(s):

```json
{
	"showConsole": true,
	"logLevel": 2,
	"logPath": "N:/My/Log/Path"
}
```

To modify NGX logging, place `sl.common.json` file (located in `./scripts/`) in the game's working directory. Edit the following line(s):

```json
{
	"logLevelNGX": 2,
}
```

Log levels are `off` (0), `on` (1) and `verbose` (2). Default values come from the `sl::Preferences` structure set by the app.

> **NOTE:**
> NGX logging gets redirected to SL so NGX log files will NOT be generated.

## How to override feature allow list

Place the `sl.interposer.json` file (located in `./scripts/`) in the game's working directory. Edit the following line(s):

```json
{
	"enableAllFeatures": true,
}
```

> **NOTE:**
> This entry tells the interposer to load all features when set to true.  When set to false or not specified, only the features specified in the `Preferences1::featuresToEnable` member will be loaded.  Any features that are loaded are also enabled by default.
This entry will be renamed in a future release of the SDK to clarify this distinction.

## How to override existing or add new hot-keys

Place the `sl.common.json` file (located in `./scripts/`) in the game's working directory. Edit the following line(s):

```json
{
	"keys": [
		{
			"alt": false,
			"ctrl": true,
			"shift": true,
			"key": 36,
			"id": "stats"
		},
		{
			"alt": false,
			"ctrl": true,
			"shift": true,
			"key": 45,
			"id": "debug"
		}
	]
}
```