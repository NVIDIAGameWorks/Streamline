# Debugging
> **NOTE:**
> This document applies to non-production, development builds only.  JSON configuration is disabled in production builds.
> Additionally, you will need to turn off any checks for signed libraries when loading Streamline libraries in order to be able to load the non-production libraries.

## JSON Config File(s)

### Location of the JSON

Note that the `sl.interposer.json` file is loaded by finding the first copy in the following ordered list of paths:
1. The directory containing the application's executable.
2. The application's current working directory at the point at which the app calls `slInit`.

### "Commenting-out" Lines
Note that the example configuration JSON files (located in `./scripts/`) include some tags that are disabled, but visible as a form of "comment"; this is done by prefixing the correct/expected tag name with underscore (_):

Functional:
```json
{
	"enableInterposer": false,	
}
```

Non-functional "comment":
```json
{
	"_enableInterposer": false,	
}
```

## How to toggle SL on/off

Place the `sl.interposer.json` file (located in `./scripts/`) in the game's working directory. Edit the following line(s):

```json
{
	"enableInterposer": false,	
}
```

When the game starts, if the flag is set to off, interposing will be completely disabled. This can be used to check for added CPU overhead in games.

## How to force use of proxies

Place the `sl.interposer.json` file (located in `./scripts/`) in the game's working directory. Edit the following line(s):

```json
{
	"forceProxies": true
}
```

> NOTE:
> This effectively forces `slGetNativeInterface` to return proxies all the time. Useful for debugging and redirecting/changing behavior by intercepting all APIs for command queues, lists, devices etc.
## How to track engine D3D12 allocations

Place the `sl.interposer.json` file (located in `./scripts/`) in the game's working directory. Edit the following line(s):

```json
{
	"trackEngineAllocations": true
}
```
> NOTE:
> This only works for D3D12 at the moment.
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
	"loadAllFeatures": true,
	"loadSpecificFeatures": [0,1],
}
```

> **NOTE:**
> This entry tells the interposer to load all features or a specific subset using the unique Ids from `sl.h`. `loadAllFeatures` supersedes `loadSpecificFeatures` if set to true.

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

Note that `"key"` lines specify the *decimal* number of the Windows Virtual Key Code (`VK_*`) for the desired key.

## How to override DLSS-G settings

Place the `sl.dlss_g.json` file (located in `./scripts/`) in the game's working directory. Edit the following line(s):

```json
{
	"_comment_compute" : "use compute queue or not, if game crashes set this back to false since some drivers might be buggy",
	"useCompute": true,
	"_comment_frames" : "optimal defaults - 1 @4K, 2 @1440p, 3 @1080p",
	"numFramesToGenerate":  1,
	"_comment_mode" : "possible modes cur, prev, auto - async flush current or previous frame or decide automatically",
	"mode" : "auto",
	"showDebugText" : true
}
```
