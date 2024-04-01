# Debugging with Streamline ImGUI
> **NOTE 1:**
> This document applies to non-production, development builds only. `sl.imgui` won't load in production builds.
> Additionally, you will need to turn off any checks for signed libraries when loading Streamline libraries in order to be able to load the non-production libraries.

## What SL ImGUI Does

At a high level, the `sl.imgui` plugin uses [`imgui`](https://github.com/ocornut/imgui) to show certain metrics/information about specific SL plugins that can be useful for validating and debugging your app integration.

The `sl.imgui` plugin is a wrapper around `imgui`. On plugin load, `sl.imgui` creates its own context and exposes functions for other plugins to:
 * Build their UI
 * Render their UI (via callbacks, or directly by calling the `sl::imgui::render()` function)

## Using SL ImGUI to debug existing plugins
### Summary

When running a non-`Production` build of SL, you should see the `imgui` pop-ups on the app screen.

*Note 1: plugin may **NOT** load their UI if they are not engaged/turned on from the app-side.*

*Note 2: you can toggle the `imgui` pop-ups with `Ctrl + Shift + Home` hotkey. Hotkey mappings can change in the future. In general, refer to the hotkey shortcuts at the bottom of the screen, or next to the UI control, for ground-truth hotkeys.*

Plugin | Debug information | Reference Image
---|---|---
Overall Streamline  |  - Bottom of screen: `imgui` debug menu keyboard shortcuts and warnings <br>  - Right side of screen: `imgui` debug menu (Each plugin that builds a UI will have its UI show up here) <br> *Note: some apps won't let the mouse interact with the `imgui` menus. For those apps, it's best to change the controls to be hotkey-controllable* | <blockquote><details><summary>Overall UI</summary><img width="100%" src="./media/sl_imgui_collapsed_view_captions.png"></details></blockquote>
`sl.interposer` | - SDK build date <br> - SL SDK version | <blockquote><details><summary>`sl.interposer` UI</summary><img max-width="100%" height="auto" src="./media/sl_imgui_interposer.png"></details></blockquote>
`sl.common` | - System (OS, driver, GPU, etc.) <br> - Graphics API <br> - VRAM usage | <blockquote><details><summary>`sl.common` UI</summary><img max-width="100%" height="auto" src="./media/sl_imgui_common.png"></details></blockquote>
`sl.reflex` | - Mode/FPS cap <br> - Marker usage <br> - Stats on sleep time | <blockquote><details><summary>`sl.reflex` UI</summary><img max-width="100%" height="auto" src="./media/sl_imgui_reflex.png"></details></blockquote>
`sl.dlss` | - Version <br> - Mode <br> - Performance stats | <blockquote><details><summary>`sl.dlss` UI</summary><img max-width="100%" height="auto" src="./media/sl_imgui_dlss.png"></details></blockquote>
`sl.dlss_g` | - Version <br> - Mode <br> - FPS boost stats (i.e., `Scaling`) <br> - VRAM consumption <br> - Constants passed in through `sl.common` | <blockquote><details><summary>`sl.dlss_g` UI</summary><img max-width="100%" height="auto" src="./media/sl_imgui_dlssg.png"></details></blockquote>
`sl.nis` | - Mode <br> - Viewport dimensions <br> - Execution time on GPU | <blockquote><details><summary>`sl.nis` UI</summary><img max-width="100%" height="auto" src="./media/sl_imgui_nis.png"></details></blockquote>
`reflex-sync` | - **Ignore, NVIDIA Internal Only** | <blockquote><details><summary>`reflex-sync` UI</summary><img max-width="100%" height="auto" src="./media/sl_imgui_reflex_sync.png"></details></blockquote>

### ImGUI Buffer Visualizer
For certain plugins, debugging some GPU buffers can be done through `sl.imgui`. **For now, only `sl.dlss_g` supports this feature.**

#### Debugging buffers for `sl.dlssg`
*Note: debug hotkey mappings can change in the future. In general, refer to the hotkey shortcuts at the bottom of the screen, or next to the UI control, for ground-truth hotkeys.*

1. Turn on `dlssg` from the app-side, and verify that the `sl.imgui` pop-up shows that `dlssg` is **On**
2. Use the visualizer:
   * *Turn on visualizer*: `Ctrl + Shift + Insert`
   * *Cycle views*: `Ctrl + Shift + End`.
   * *Turn off visualizer*: `Ctrl + Shift + Insert`

In addition to the `sl.dlssg` input buffers (e.g. depth, motion vectors, etc.), the visualizer should help you view the debug buffers:

Buffer | What it means | Correctness Interpretation | Reference Image
---|---|---|---
Alignment | Visualizes the alignment of depth, motion vectors, and color buffers using a Sobel filter | - Image should be mostly blue (color data) <br> - You should see yellow/green edges around moving objects, including when the camera is moving (mvec data) <br> - You should see red edges everywhere else (depth data) | <img max-width="100%" height="auto" src="./media/sl_imgui_dlssg_buffer_alignment.png">
Dynamic Objects | Visualizes pixels that have non-zero motion vector values. This excludes motion caused by camera movement | - Only (parts) of dynamic objects should be colored red (not due to camera movement!) <br> - All other pixels should be black/zero | <img max-width="100%" height="auto" src="./media/sl_imgui_dlssg_dynamic_objs.png">

## Adding SL ImGUI to new plugins
The `sl.common` plugin's usage of `sl.imgui` is an easy to follow example on how to add `sl.common` UI and render it. Implementing something similar is advised.

```
#ifndef SL_PRODUCTION
// 1. Check for UI and register our callback
imgui::ImGUI* ui{};
param::getPointerParam(api::getContext()->parameters, param::imgui::kInterface, &ui);

if (ui)
{
    // 2. Define the UI building callback
    auto renderUI = [](imgui::ImGUI* ui, bool finalFrame)->void
    {
        // Use `ui` to build buttons/text/sliders/etc.
    };

    // 3. Register the callback so sl::imgui can render it
    ui->registerRenderCallbacks(renderUI, nullptr);
}
#endif
```
