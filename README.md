<p align="center">
  <img src="Resources/Icon128.png" alt="TrackEdge logo" width="128" height="128">
</p>

# TrackEdge Unreal Engine Plugin

TrackEdge is an Unreal Engine analytics and replay plugin from DevEdge Studio. It sends gameplay events and replay telemetry to TrackEdge, and includes an editor tool for capturing a level into tiled map images and uploading those tiles.

## Requirements

- Unreal Engine 5.7
- A TrackEdge project with a `Project ID` and `API Key`
- Network access from the editor/game to the configured TrackEdge server
- For map capture: an open level containing an Unreal `Level Bounds` actor

The plugin is currently configured as a source plugin and contains no content assets.

## Installation

### Install from the repository

1. Close Unreal Editor.
2. Copy the complete `TrackEdge` plugin folder into either:
   - `<YourProject>/Plugins/TrackEdge`, for a project-only install; or
   - `<YourEngine>/Plugins/Marketplace/TrackEdge`, for an engine-wide install.
3. Confirm that `TrackEdge.uplugin` is directly inside the copied folder.
4. Open the project. If Unreal asks to rebuild modules, select **Yes**.
5. If no project modules exist yet, generate project files by right-clicking the `.uproject` file and choosing **Generate Visual Studio project files**, then build the project target in Visual Studio.
6. Restart the editor after compilation.

The plugin descriptor contains a runtime module named `TrackEdge` and an editor-only module named `TrackEdgeEditor`. The editor module is not loaded in packaged builds.

## Initial configuration

In Unreal Editor, open **Edit > Project Settings > Plugins > TrackEdge** and set:

| Setting | Description |
| --- | --- |
| `Project ID` | The TrackEdge project identifier used by runtime requests. Required. |
| `API Key` | The project key used by editor map uploads. Required for uploading maps. |
| `Base URL` | TrackEdge server URL. Defaults to `https://trackedge.dev`. Do not add a trailing slash. |
| `Level Mappings` | The TrackEdge map IDs associated with Unreal levels. The editor capture tool can create these automatically. |

These values are stored in the project's `Config/DefaultGame.ini` under `[ /Script/TrackEdge.TrackEdgeSettings ]`. A typical configuration is:

```ini
[/Script/TrackEdge.TrackEdgeSettings]
ProjectId=YOUR_PROJECT_ID
ApiKey=YOUR_API_KEY
BaseUrl=https://trackedge.dev
```

Treat the API key as a secret. Do not commit it to a public repository or expose it in client-distributed configuration unless your TrackEdge deployment is designed for that use.

## Runtime analytics

`UTrackEdgeSubsystem` is a `UGameInstanceSubsystem`, so it is created automatically for each game instance. During initialization it creates or loads a persistent player ID in `Saved/TrackEdge_PlayerId.txt`, generates a device fingerprint, initializes a TrackEdge session, and identifies the player.

The runtime automatically collects platform, OS, build type, engine version, RHI, hardware, display, and FPS information with tracked events. Requests retry up to three times when a request fails.

### Blueprint nodes

The plugin exposes these async Blueprint nodes under the `TrackEdge` categories:

- **Track Event** (`TrackEdge`): sends an event by name. Use the `On Success` and `On Fail` execution paths. The current Blueprint node accepts the event name; custom properties are available through the C++ API below.
- **Start Replay Session** (`TrackEdge | Replay`): explicitly starts a replay session.
- **Capture Replay Event** (`TrackEdge | Replay`): records a replay event type at the current player position.
- **End Replay Session** (`TrackEdge | Replay`): ends and uploads the current replay session.

Replay event types are:

`Player Start`, `Session Replay`, `Player Death`, `Player Leave`, and `Custom Event`.

### C++ usage

Include `TrackEdgeSubsystem.h` and access the subsystem from a valid world/game instance:

```cpp
#include "TrackEdgeSubsystem.h"

if (UGameInstance* GameInstance = GetGameInstance())
{
    if (UTrackEdgeSubsystem* TrackEdge =
        GameInstance->GetSubsystem<UTrackEdgeSubsystem>())
    {
        TMap<FString, FString> Properties;
        Properties.Add(TEXT("weapon"), TEXT("Rifle"));

        TrackEdge->TrackEvent(
            TEXT("weapon_fired"),
            Properties,
            [](bool bSuccess, const FString& Message)
            {
                UE_LOG(LogTemp, Log, TEXT("TrackEdge: %s (%s)"),
                    bSuccess ? TEXT("success") : TEXT("failed"), *Message);
            });
    }
}
```

Replay sessions can be controlled with `StartReplaySession`, `SetReplayEventType`, and `EndReplaySession`. Replay capture requires a matching level mapping. When a mapping has **Automatic Replay** enabled, the subsystem starts replay tracking after the level loads and ends it when the level changes.

## Level mappings and replay settings

Each mapping is matched against the loaded Unreal level name (`UWorld::GetMapName()`). A mapping contains:

| Field | Description |
| --- | --- |
| `Level Name` | The exact Unreal level name. |
| `Map Set ID` | TrackEdge map-set identifier. |
| `Map ID` | TrackEdge map identifier for the level. |
| `Replay Sample Interval` | Seconds between replay samples. Default: `0.2`. |
| `Replay Chunk Size` | Number of points uploaded per chunk. Default: `25`. |
| `Automatic Replay` | Enables automatic replay capture for the level. Default: enabled. |

If the current level has no mapping, replay capture is skipped and explicit replay start requests return a `No Level Mapping` failure.

## Editor map capture and upload

The editor module adds **Capture Map** to **Window > TrackEdge**.

1. Open the level you want to map.
2. Add a `Level Bounds` actor if the level does not already have one: **Place Actors > Level Bounds**.
3. Open **Window > TrackEdge > Capture Map**.
4. Enter a map name and optional description.
5. Choose `LEVEL` or `UI`.
6. Set grid rows and columns. The valid range is 1–100 for each; the default is 3 × 3.
7. Set the camera height. The valid range is 100–100,000 Unreal units; the default is 1,000.
8. Choose `Lit` or `Unlit` capture mode.
9. Click **Create**. The tool captures the level bounds as 1024 × 1024 PNG tiles.
10. After capture completes, use **Open Folder** to inspect the generated tiles or click **Upload** to create the TrackEdge map set and upload them.

After a successful upload, the editor adds a level mapping to TrackEdge project settings and saves it to configuration. If you change the level name or upload a replacement map, review the entries under **Project Settings > Plugins > TrackEdge > Level Mappings** and remove stale mappings if necessary.

The capture tool requires editor-only rendering and HTTP functionality. It should be run while the target level is open and not while PIE or another capture operation is active.

## Packaging and shipping

The runtime module is intended to compile into game targets. `TrackEdgeEditor` is an editor module and is excluded from packaged game builds by Unreal's module type. Before shipping, verify that:

- `Project ID` and `Base URL` are correct for the target environment.
- Every level that should produce replay data has a valid mapping.
- Replay sample interval and chunk size are appropriate for bandwidth and storage costs.
- Network failures are handled by the game's normal offline behavior; TrackEdge requests are asynchronous.

## Troubleshooting

**The plugin does not appear in Project Settings**

- Confirm `TrackEdge.uplugin` is at the plugin root.
- Confirm the project is using Unreal Engine 5.7.
- Close the editor, delete only the project's generated `Binaries` and `Intermediate` folders if they are stale, regenerate project files, and rebuild.

**Events are not being sent**

- Check `Project ID`, `Base URL`, and network access.
- Inspect the Output Log for `TrackEdge` messages.
- Make sure requests are not being triggered before the game instance has initialized.

**Replay reports `No Level Mapping`**

- Confirm the mapping's `Level Name` exactly matches the loaded level name.
- Confirm both `Map Set ID` and `Map ID` are populated.
- Enable `Automatic Replay` if the level should start replay capture on load.

**Map capture fails with “No LevelBounds actor found”**

- Add a `Level Bounds` actor to the level and ensure it encloses the playable area.
- Reopen the capture window and try again.

**Map upload fails**

- Verify the API key and base URL.
- Ensure the editor has internet access and the project has permission to create map sets.
- Review the Output Log for the HTTP response and retry after the service is available.

## Project layout

```text
TrackEdge.uplugin
Resources/Icon128.png
Source/TrackEdge/                 Runtime analytics, settings, replay subsystem, Blueprint nodes
Source/TrackEdgeEditor/           Editor menu, map capture, tile upload, mapping creation
```

## Support

- Documentation: https://trackedge.devedge.studio/docs
- Website: https://trackedge.dev/
- Support: https://discord.gg/hf35ZVYqj4

## Contributing and license

TrackEdge is released under the [MIT License](LICENSE). You may use, modify,
redistribute, sublicense, and sell software containing this code, including
competing products, provided that the copyright and license notices are
preserved. See [`CONTRIBUTING.md`](CONTRIBUTING.md) for contribution rules.

TrackEdge and DevEdge Studio names, logos, and branding remain subject to
applicable trademark rights and are not granted by the MIT License.
