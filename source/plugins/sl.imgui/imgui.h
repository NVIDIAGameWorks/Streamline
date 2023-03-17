#pragma once

#include "source/plugins/sl.imgui/types.h"
#include "source/plugins/sl.imgui/input.h"
#include "source/plugins/sl.imgui/imguiTypes.h"

namespace sl
{

namespace imgui
{

struct ImGUI;
using RenderCallback = std::function<void(ImGUI* ui, bool finalFrame)>;

using namespace type;

struct ImGUI
{
 
    Context* (*createContext)(const ContextDesc& desc);
    void (*destroyContext)(Context* ctx);
    void (*setCurrentContext)(Context* ctx);
    uint8_t* (*getFontAtlasPixels)(int32_t& width, int32_t& height);
    void (*newFrame)(float elapsedTime);
    void (*render)(void* commandList, void* backBuffer, uint32_t index);
    const DrawData& (*getDrawData)();

    void (*triggerRenderWindowCallbacks)(bool finalFrame);
    void (*triggerRenderAnywhereCallbacks)(bool finalFrame);
    void (*registerRenderCallbacks)(RenderCallback window, RenderCallback anywhere);

    void(*plotGraph)(const Graph& graph, const std::vector<GraphValues>& values);

    /**
     * Sets the display size.
     *
     * @param size The display size to be set.
     */
    void(* setDisplaySize)(type::Float2 size);

    /**
     * Gets the display size.
     *
     * @return The display size.
     */
    type::Float2(*getDisplaySize)();

    /**
     * Gets the style struct.
     *
     * @return The style struct.
     */
    Style* (*getStyle)();

    /**
     * Shows a demo window of all features supported.
     *
     * @param open Is window opened, can be changed after the call.
     */
    void(*showDemoWindow)(bool* open);

    /**
     * Create metrics window.
     *
     * Display ImGui internals: draw commands (with individual draw calls and vertices), window list, basic internal
     * state, etc.
     *
     * @param open Is window opened, can be changed after the call.
     */
    void(* showMetricsWindow)(bool* open);

    /**
     * Add style editor block (not a window).
     *
     * You can pass in a reference ImGuiStyle structure to compare to, revert to and save to (else it uses the default
     * style)
     *
     * @param style Style to edit. Pass nullptr to edit the default one.
     */
    void(* showStyleEditor)(Style* style);

    /**
     * Add style selector block (not a window).
     *
     * Essentially a combo listing the default styles.
     *
     * @param label Label.
     */
    bool(* showStyleSelector)(const char* label);

    /**
     * Add font selector block (not a window).
     *
     * Essentially a combo listing the loaded fonts.
     *
     * @param label Label.
     */
    void(* showFontSelector)(const char* label);

    /**
     * Add basic help/info block (not a window): how to manipulate ImGui as a end-user (mouse/keyboard controls).
     */
    void(* showUserGuide)();

    /**
     * Get underlying imgui library version string.
     *
     * @return The version string e.g. "1.66".
     */
    const char* (* getImGuiVersion)();

    /**
     * Set style colors from one of the predefined presets:
     *
     * @param style Style to change. Pass nullptr to change the default one.
     * @param preset Colors preset.
     */
    void(* setStyleColors)(Style* style, StyleColorsPreset preset);

    /**
     * Begins defining a new immediate mode gui (window).
     *
     * @param label The window label.
     * @param open Returns if the window was active.
     * @param windowFlags The window flags to be used.
     * @return return false to indicate the window is collapsed or fully clipped,
     *  so you may early out and omit submitting anything to the window.
     */
    bool(* begin)(const char* label, bool* open, WindowFlags flags);

    /**
     * Ends defining a immediate mode.
     */
    void(* end)();

    /**
     * Begins a scrolling child region.
     *
     * @param strId The string identifier for the child region.
     * @param size The size of the region.
     *  size==0.0f: use remaining window size, size<0.0f: use remaining window.
     * @param border Draw border.
     * @param flags Sets any WindowFlags for the dialog.
     * @return true if the region change, false if not.
     */
    bool(* beginChild)(const char* strId, type::Float2 size, bool border, WindowFlags flags);

    /**
     * Begins a scrolling child region.
     *
     * @param id The identifier for the child region.
     * @param size The size of the region.
     *  size==0.0f: use remaining window size, size<0.0f: use remaining window.
     * @param border Draw border.
     * @param flags Sets any WindowFlags for the dialog.
     * @return true if the region change, false if not.
     */
    bool(* beginChildId)(uint32_t id, type::Float2 size, bool border, WindowFlags flags);

    /**
     * Ends a child region.
     */
    void(* endChild)();

    /**
     * Is window appearing?
     *
     * @return true if window is appearing, false if not.
     */
    bool(* isWindowAppearing)();

    /**
     * Is window collapsed?
     *
     * @return true if window is collapsed, false is not.
     */
    bool(* isWindowCollapsed)();

    /**
     * Is current window focused? or its root/child, depending on flags. see flags for options.
     *
     * @param flags The focused flags to use.
     * @return true if window is focused, false if not.
     */
    bool(* isWindowFocused)(FocusedFlags flags);

    /**
     * Is current window hovered (and typically: not blocked by a popup/modal)? see flags for options.
     *
     * If you are trying to check whether your mouse should be dispatched to imgui or to your app, you should use
     * the 'io.WantCaptureMouse' boolean for that! Please read the FAQ!
     *
     * @param flags The hovered flags to use.
     * @return true if window is currently hovered over, flase if not.
     */
    bool(* isWindowHovered)(HoveredFlags flags);

    /**
     * Get the draw list associated to the window, to append your own drawing primitives.
     *
     * @return The draw list associated to the window, to append your own drawing primitives.
     */
    DrawList* (* getWindowDrawList)();

    /**
     * Gets the DPI scale currently associated to the current window's viewport.
     *
     * @return The DPI scale currently associated to the current window's viewport.
     */
    float(* getWindowDpiScale)();

    /**
     * Get current window position in screen space
     *
     * This is useful if you want to do your own drawing via the DrawList API.
     *
     * @return The current window position in screen space.
     */
    type::Float2(* getWindowPos)();

    /**
     * Gets the current window size.
     *
     * @return The current window size
     */
    type::Float2(* getWindowSize)();

    /**
     * Gets the current window width.
     *
     * @return The current window width.
     */
    float(* getWindowWidth)();

    /**
     * Gets the current window height.
     *
     * @return The current window height.
     */
    float(* getWindowHeight)();

    /**
     * Gets the current content boundaries.
     *
     * This is typically window boundaries including scrolling,
     * or current column boundaries, in windows coordinates.
     *
     * @return The current content boundaries.
     */
    type::Float2(* getContentRegionMax)();

    /**
     * Gets the current content region available.
     *
     * This is: getContentRegionMax() - getCursorPos()
     *
     * @return The current content region available.
     */
    type::Float2(* getContentRegionAvail)();

    /**
     * Gets the width of the current content region available.
     *
     * @return The width of the current content region available.
     */
    float(* ContentRegionAvailWidth)();

    /**
     * Content boundaries min (roughly (0,0)-Scroll), in window coordinates
     */
    type::Float2(* getWindowContentRegionMin)();

    /**
     * Gets the maximum content boundaries.
     *
     * This is roughly (0,0)+Size-Scroll) where Size can be override with SetNextWindowContentSize(),
     * in window coordinates.
     *
     * @return The maximum content boundaries.
     */
    type::Float2(* getWindowContentRegionMax)();

    /**
     * Content region width.
     */
    float(* getWindowContentRegionWidth)();

    /**
     * Sets the next window position.
     *
     * Call before begin(). use pivot=(0.5f,0.5f) to center on given point, etc.
     *
     * @param position The position to set.
     * @param pivot The offset pivot.
     */
    void(* setNextWindowPos)(Float2 position, Condition cond, Float2 pivot);

    /**
     * Set next window size.
     *
     * Set axis to 0.0f to force an auto-fit on this axis. call before begin()
     *
     * @param size The next window size.
     */
    void(* setNextWindowSize)(Float2 size, Condition cond);

    /**
     * Set next window size limits. use -1,-1 on either X/Y axis to preserve the current size. Use callback to apply
     * non-trivial programmatic constraints.
     */
    void(* setNextWindowSizeConstraints)(const Float2& sizeMin, const Float2& sizeMax);

    /**
     * Set next window content size (~ enforce the range of scrollbars). not including window decorations (title bar,
     * menu bar, etc.). set an axis to 0.0f to leave it automatic. call before Begin()
     */
    void(* setNextWindowContentSize)(const Float2& size);

    /**
     * Set next window collapsed state. call before Begin()
     */
    void(* setNextWindowCollapsed)(bool collapsed, Condition cond);

    /**
     * Set next window to be focused / front-most. call before Begin()
     */
    void(* setNextWindowFocus)();

    /**
     * Set next window background color alpha. helper to easily modify ImGuiCol_WindowBg/ChildBg/PopupBg.
     */
    void(* setNextWindowBgAlpha)(float alpha);

    /**
     * Set font scale. Adjust IO.FontGlobalScale if you want to scale all windows
     */
    void(* setWindowFontScale)(float scale);

    /**
     * Set named window position.
     */
    void(* setWindowPos)(const char* name, const Float2& pos, Condition cond);

    /**
     * Set named window size. set axis to 0.0f to force an auto-fit on this axis.
     */
    void(* setWindowSize)(const char* name, const Float2& size, Condition cond);

    /**
     * Set named window collapsed state
     */
    void(* setWindowCollapsed)(const char* name, bool collapsed, Condition cond);

    /**
     * Set named window to be focused / front-most. use NULL to remove focus.
     */
    void(* setWindowFocus)(const char* name);

    /**
     * Get scrolling amount [0..GetScrollMaxX()]
     */
    float(* getScrollX)();

    /**
     * Get scrolling amount [0..GetScrollMaxY()]
     */
    float(* getScrollY)();

    /**
     * Get maximum scrolling amount ~~ ContentSize.X - WindowSize.X
     */
    float(* getScrollMaxX)();

    /**
     * Get maximum scrolling amount ~~ ContentSize.Y - WindowSize.Y
     */
    float(* getScrollMaxY)();

    /**
     * Set scrolling amount [0..GetScrollMaxX()]
     */
    void(* setScrollX)(float scrollX);

    /**
     * Set scrolling amount [0..GetScrollMaxY()]
     */
    void(* setScrollY)(float scrollY);

    /**
     * Adjust scrolling amount to make current cursor position visible.
     *
     * @param centerYRatio Center y ratio: 0.0: top, 0.5: center, 1.0: bottom.
     */
    void(* setScrollHereY)(float centerYRatio);

    /**
     * Adjust scrolling amount to make given position valid. use getCursorPos() or getCursorStartPos()+offset to get
     * valid positions. default: centerYRatio = 0.5f
     */
    void(* setScrollFromPosY)(float posY, float centerYRatio);

    /**
     * Use NULL as a shortcut to push default font
     */
    void(* pushFont)(Font* font);

    /**
     * Pop font from the stack
     */
    void(* popFont)();

    /**
     * Pushes and applies a style color for the current widget.
     *
     * @param styleColorIndex The style color index.
     * @param color The color to be applied for the style color being pushed.
     */
    void(* pushStyleColor)(StyleColor styleColorIndex, Float4 color);

    /**
     * Pops off and stops applying the style color for the current widget.
     */
    void(* popStyleColor)();

    /**
     * Pushes a style variable(property) with a float value.
     *
     * @param styleVarIndex The style variable(property) index.
     * @value The value to be applied.
     */
    void(* pushStyleVarFloat)(StyleVar styleVarIndex, float value);

    /**
     * Pushes a style variable(property) with a Float2 value.
     *
     * @param styleVarIndex The style variable(property) index.
     * @value The value to be applied.
     */
    void(* pushStyleVarFloat2)(StyleVar styleVarIndex, Float2 value);

    /**
     * Pops off and stops applying the style variable(property) for the current widget.
     */
    void(* popStyleVar)();

    /**
     * Retrieve style color as stored in ImGuiStyle structure. use to feed back into PushStyleColor(), otherwhise use
     * GetColorU32() to get style color with style alpha baked in.
     */
    const Float4& (* getStyleColorVec4)(StyleColor colorIndex);

    /**
     * Get current font
     */
    Font* (* getFont)();

    /**
     * Get current font size (= height in pixels) of current font with current scale applied
     */
    float(* getFontSize)();

    /**
     * Get UV coordinate for a while pixel, useful to draw custom shapes via the ImDrawList API
     */
    Float2(* getFontTexUvWhitePixel)();

    /**
     * Retrieve given style color with style alpha applied and optional extra alpha multiplier
     */
    uint32_t(* getColorU32StyleColor)(StyleColor colorIndex, float alphaMul);

    /**
     * Retrieve given color with style alpha applied
     */
    uint32_t(* getColorU32Vec4)(Float4 color);

    /**
     * Retrieve given color with style alpha applied
     */
    uint32_t(* getColorU32)(uint32_t color);

    /**
     * Push an item width for next widgets.
     * In pixels. 0.0f = default to ~2/3 of windows width, >0.0f: width in pixels, <0.0f align xx pixels to the right of
     * window (so -1.0f always align width to the right side).
     *
     * @param width The width.
     */
    void(* pushItemWidth)(float width);

    /**
     * Pops an item width.
     */
    void(* popItemWidth)();

    /**
     * Size of item given pushed settings and current cursor position
     * NOTE: This is not the same as calcItemWidth
     */
    type::Float2(* calcItemSize)(type::Float2 size, float defaultX, float defaultY);

    /**
     * Width of item given pushed settings and current cursor position
     */
    float(* calcItemWidth)();

    void(* pushItemFlag)(ItemFlags option, bool enabled);

    void(* popItemFlag)();

    /**
     * Word-wrapping for Text*() commands. < 0.0f: no wrapping; 0.0f: wrap to end of window (or column); > 0.0f: wrap at
     * 'wrapPosX' position in window local space
     */
    void(* pushTextWrapPos)(float wrapPosX);

    /**
     * Pop text wrap pos form the stack
     */
    void(* popTextWrapPos)();

    /**
     * Allow focusing using TAB/Shift-TAB, enabled by default but you can disable it for certain widgets
     */
    void(* pushAllowKeyboardFocus)(bool allow);

    /**
     * Pop allow keyboard focus
     */
    void(* popAllowKeyboardFocus)();

    /**
     * In 'repeat' mode, Button*() functions return repeated true in a typematic manner (using
     * io.KeyRepeatDelay/io.KeyRepeatRate setting). Note that you can call IsItemActive() after any Button() to tell if
     * the button is held in the current frame.
     */
    void(* pushButtonRepeat)(bool repeat);

    /**
     * Pop button repeat
     */
    void(* popButtonRepeat)();

    /**
     * Adds a widget separator.
     */
    void(* separator)();

    /**
     * Tell the widget to stay on the same line.
     */
    void sameLine()
    {
        sameLineEx(0, -1.0f);
    }

    /**
     * Tell the widget to stay on the same line with parameters.
     */
    void(* sameLineEx)(float posX, float spacingW);

    /**
     * Undo sameLine()
     */
    void(* newLine)();

    /**
     * Adds widget spacing.
     */
    void(* spacing)();

    /**
     * Adds a dummy element of a given size
     *
     * @param size The size of a dummy element.
     */
    void(* dummy)(Float2 size);

    /**
     * Indents with width indent spacing.
     */
    void(* indent)(float indentWidth);

    /**
     * Undo indent.
     */
    void(* unindent)(float indentWidth);

    /**
     * Lock horizontal starting position + capture group bounding box into one "item" (so you can use IsItemHovered() or
     * layout primitives such as () on whole group, etc.)
     */
    void(* beginGroup)();

    /**
     * End group
     */
    void(* endGroup)();

    /**
     * Cursor position is relative to window position
     */
    Float2(* getCursorPos)();

    /**
     *
     */
    float(* getCursorPosX)();

    /**
     *
     */
    float(* getCursorPosY)();

    /**
     *
     */
    void(* setCursorPos)(const Float2& localPos);

    /**
     *
     */
    void(* setCursorPosX)(float x);

    /**
     *
     */
    void(* setCursorPosY)(float y);

    /**
     * Initial cursor position
     */
    Float2(* getCursorStartPos)();

    /**
     * Cursor position in absolute screen coordinates [0..io.DisplaySize] (useful to work with ImDrawList API)
     */
    Float2(* getCursorScreenPos)();

    /**
     * Cursor position in absolute screen coordinates [0..io.DisplaySize]
     */
    void(* setCursorScreenPos)(const Float2& pos);

    /**
     * Vertically align upcoming text baseline to FramePadding.y so that it will align properly to regularly framed
     * items (call if you have text on a line before a framed item)
     */
    void(* alignTextToFramePadding)();

    /**
     * ~ FontSize
     */
    float(* getTextLineHeight)();

    /**
     * ~ FontSize + style.ItemSpacing.y (distance in pixels between 2 consecutive lines of text)
     */
    float(* getTextLineHeightWithSpacing)();

    /**
     * ~ FontSize + style.FramePadding.y * 2
     */
    float(* getFrameHeight)();

    /**
     * ~ FontSize + style.FramePadding.y * 2 + style.ItemSpacing.y (distance in pixels between 2 consecutive lines of
     * framed widgets)
     */
    float(* getFrameHeightWithSpacing)();

    /**
     * Push a string id for next widgets.
     *
     * If you are creating widgets in a loop you most likely want to push a unique identifier (e.g. object pointer, loop
     * index) so ImGui can differentiate them. popId() must be called later.
     * @param id The string id.
     */
    void(* pushIdString)(const char* id);

    /**
     * Push a string id for next widgets.
     *
     * If you are creating widgets in a loop you most likely want to push a unique identifier (e.g. object pointer, loop
     * index) so ImGui can differentiate them. popId() must be called later.
     * @param id The string id.
     */
    void(* pushIdStringBeginEnd)(const char* idBegin, const char* idEnd);

    /**
     * Push an integer id for next widgets.
     *
     * If you are creating widgets in a loop you most likely want to push a unique identifier (e.g. object pointer, loop
     * index) so ImGui can differentiate them. popId() must be called later.
     * @param id The integer id.
     */
    void(* pushIdInt)(int id);

    /**
     * Push pointer id for next widgets.
     */
    void(* pushIdPtr)(const void* id);

    /**
     * Pops an id.
     */
    void(* popId)();

    /**
     * Calculate unique ID (hash of whole ID stack + given parameter). e.g. if you want to query into ImGuiStorage
     * yourself.
     */
    uint32_t(* getIdString)(const char* id);

    /**
     * Calculate unique ID (hash of whole ID stack + given parameter). e.g. if you want to query into ImGuiStorage
     * yourself.
     */
    uint32_t(* getIdStringBeginEnd)(const char* idBegin, const char* idEnd);

    /**
     */
    uint32_t(* getIdPtr)(const void* id);

    /**
     * Shows a text widget, without text formatting. Faster version, use for big texts.
     *
     * @param text The null terminated text string.
     */
    void(* textUnformatted)(const char* text);


    /**
     * Shows a text widget.
     *
     * @param fmt The formated label for the text.
     * @param ... The variable arguments for the label.
     */
    void(* text)(const char* fmt, ...);

    /**
     * Shows a colored text widget.
     *
     * @param fmt The formated label for the text.
     * @param ... The variable arguments for the label.
     */
    void(* textColored)(const Float4& color, const char* fmt, ...);

    /**
     * Shows a colored label widget.
     *
     * @param fmt The formated label for the text.
     * @param ... The variable arguments for the label.
     */
    void(*labelColored)(const Float4& color, const char* label, const char* fmt, ...);

    /**
     * Shows a disabled text widget.
     *
     * @param fmt The formated label for the text.
     * @param ... The variable arguments for the label.
     */
    void(* textDisabled)(const char* fmt, ...);

    /**
     * Shows a wrapped text widget.
     *
     * @param fmt The formated label for the text.
     * @param ... The variable arguments for the label.
     */
    void(* textWrapped)(const char* fmt, ...);

    /**
     * Display text+label aligned the same way as value+label widgets.
     */
    void(* labelText)(const char* label, const char* fmt, ...);

    /**
     * Shortcut for Bullet()+Text()
     */
    void(* bulletText)(const char* fmt, ...);

    /**
     * Shows a button widget.
     *
     * @param label The label for the button.
     * @return true if the button was pressed, false if not.
     */
    bool(* buttonEx)(const char* label, const Float2& size);

    bool button(const char* label)
    {
        return buttonEx(label, { 0, 0 });
    }
    
    /**
     * Shows a smallButton widget.
     *
     * @param label The label for the button.
     * @return true if the button was pressed, false if not.
     */
    bool(* smallButton)(const char* label);

    /**
     * Button behavior without the visuals.
     *
     * Useful to build custom behaviors using the public api (along with isItemActive, isItemHovered, etc.)
     */
    bool(* invisibleButton)(const char* id, const Float2& size);

    /**
     * Arrow-like button with specified direciton
     */
    bool(* arrowButton)(const char* id, Direction dir);

    /**
     * Image with user texture id
     * defaults:
     *      uv0 = Float2(0,0)
     *      uv1 = Float2(1,1)
     *      tintColor = Float4(1,1,1,1)
     *      borderColor = Float4(0,0,0,0)
     */
    void(* image)(TextureId userTextureId,
        const Float2& size,
        const Float2& uv0,
        const Float2& uv1,
        const Float4& tintColor,
        const Float4& borderColor);

    /**
     * Image as a button. <0 framePadding uses default frame padding settings. 0 for no padding
     * defaults:
     *     uv0 = Float2(0,0)
     *     uv1 = Float2(1,1)
     *     framePadding = -1
     *     bgColor = Float4(0,0,0,0)
     *     tintColor = Float4(1,1,1,1)
     */
    bool(* imageButton)(TextureId userTextureId,
        const Float2& size,
        const Float2& uv0,
        const Float2& uv1,
        int framePadding,
        const Float4& bgColor,
        const Float4& tintColor);

    /**
     * Adds a checkbox widget.
     *
     * @param label The checkbox label.
     * @param value The current value of the checkbox
     *
     * @return true if the checkbox was pressed, false if not.
     */
    bool(* checkbox)(const char* label, bool* value);

    /**
     * Flags checkbox
     */
    bool(* checkboxFlags)(const char* label, uint32_t* flags, uint32_t flagsValue);

    /**
     * Radio button
     */
    bool(* radioButton)(const char* label, bool active);

    /**
     * Radio button
     */
    bool(* radioButtonEx)(const char* label, int* v, int vButton);

    /**
     * Adds a progress bar widget.
     *
     * @param fraction The progress value (0-1).
     * @param size The widget size.
     * @param overlay The text overlay, if nullptr the default with percents is displayed.
     */
    void(* progressBar)(float fraction, Float2 size, const char* overlay);

    /**
     * Draws a small circle.
     */
    void(* bullet)();

    /**
     * The new beginCombo()/endCombo() api allows you to manage your contents and selection state however you want it.
     * The old Combo() api are helpers over beginCombo()/endCombo() which are kept available for convenience purpose.
     */
    bool(* beginCombo)(const char* label, const char* previewValue, ComboFlags flags);

    /**
     * only call endCombo() if beginCombo() returns true!
     */
    void(* endCombo)();

    /**
     * Adds a combo box widget.
     *
     * @param label The label for the combo box.
     * @param currentItem The current (selected) element index.
     * @param items The array of items.
     * @param itemCount The number of items.
     *
     * @return true if the selected item value has changed, false if not.
     */
    bool(* combo)(const char* label, int* currentItem, const char* const* items, int itemCount);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). If vMin >= vMax we have no bound. For all the Float2/Float3/Float4/Int2/Int3/Int4 versions of
     * every functions, note that a 'float v[X]' function argument is the same as 'float* v', the array syntax is just a
     * way to document the number of elements that are expected to be accessible. You can pass address of your first
     * element out of a contiguous set, e.g. &myvector.x. Speed are per-pixel of mouse movement (vSpeed=0.2f: mouse
     * needs to move by 5 pixels to increase value by 1). For gamepad/keyboard navigation, minimum speed is Max(vSpeed,
     * minimum_step_at_given_precision). Defaults: float vSpeed = 1.0f, float vMin = 0.0f, float vMax = 0.0f, const
     * char* displayFormat = "%.3f", float power = 1.0f)
     */
    bool(* dragFloat)(
        const char* label, float* v, float vSpeed, float vMin, float vMax, const char* displayFormat, float power);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds) Defaults: float vSpeed = 1.0f, float vMin = 0.0f, float vMax = 0.0f, const char* displayFormat =
     * "%.3f", float power = 1.0f)
     */
    bool(* dragFloat2)(
        const char* label, float v[2], float vSpeed, float vMin, float vMax, const char* displayFormat, float power);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds) Defaults: float vSpeed = 1.0f, float vMin = 0.0f, float vMax = 0.0f, const char* displayFormat =
     * "%.3f", float power = 1.0f)
     */
    bool(* dragFloat3)(
        const char* label, float v[3], float vSpeed, float vMin, float vMax, const char* displayFormat, float power);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds) Defaults: float vSpeed = 1.0f, float vMin = 0.0f, float vMax = 0.0f, const char* displayFormat =
     * "%.3f", float power = 1.0f)
     */
    bool(* dragFloat4)(
        const char* label, float v[4], float vSpeed, float vMin, float vMax, const char* displayFormat, float power);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: float vSpeed = 1.0f, float vMin = 0.0f, float vMax = 0.0f, const char* displayFormat =
     * "%.3f", const char* displayFormatMax = NULL, float power = 1.0f
     */
    bool(* dragFloatRange2)(const char* label,
        float* vCurrentMin,
        float* vCurrentMax,
        float vSpeed,
        float vMin,
        float vMax,
        const char* displayFormat,
        const char* displayFormatMax,
        float power);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). If vMin >= vMax we have no bound.. Defaults: float vSpeed = 1.0f, int vMin = 0, int vMax = 0,
     * const char* displayFormat = "%.0f"
     */
    bool(* dragInt)(const char* label, int* v, float vSpeed, int vMin, int vMax, const char* displayFormat);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: float vSpeed = 1.0f, int vMin = 0, int vMax = 0, const char* displayFormat = "%.0f"
     */
    bool(* dragInt2)(const char* label, int v[2], float vSpeed, int vMin, int vMax, const char* displayFormat);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: float vSpeed = 1.0f, int vMin = 0, int vMax = 0, const char* displayFormat = "%.0f"
     */
    bool(* dragInt3)(const char* label, int v[3], float vSpeed, int vMin, int vMax, const char* displayFormat);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: float vSpeed = 1.0f, int vMin = 0, int vMax = 0, const char* displayFormat = "%.0f"
     */
    bool(* dragInt4)(const char* label, int v[4], float vSpeed, int vMin, int vMax, const char* displayFormat);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: float vSpeed = 1.0f, int vMin = 0, int vMax = 0, const char* displayFormat = "%.0f",
     * const char* displayFormatMax = NULL
     */
    bool(* dragIntRange2)(const char* label,
        int* vCurrentMin,
        int* vCurrentMax,
        float vSpeed,
        int vMin,
        int vMax,
        const char* displayFormat,
        const char* displayFormatMax);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). If vMin >= vMax we have no bound.. Defaults: float vSpeed = 1.0f, int vMin = 0, int vMax = 0,
     * const char* displayFormat = "%.0f", power = 1.0f
     */
    bool(* dragScalar)(const char* label,
        DataType dataType,
        void* v,
        float vSpeed,
        const void* vMin,
        const void* vMax,
        const char* displayFormat,
        float power);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). If vMin >= vMax we have no bound.. Defaults: float vSpeed = 1.0f, int vMin = 0, int vMax = 0,
     * const char* displayFormat = "%.0f", power = 1.0f
     */
    bool(* dragScalarN)(const char* label,
        DataType dataType,
        void* v,
        int components,
        float vSpeed,
        const void* vMin,
        const void* vMax,
        const char* displayFormat,
        float power);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Adjust displayFormat to decorate the value with a prefix or a suffix for in-slider labels or unit
     * display. Use power!=1.0 for logarithmic sliders . Defaults: const char* displayFormat = "%.3f", float power
     * = 1.0f
     */
    bool(* sliderFloat)(const char* label, float* v, float vMin, float vMax, const char* displayFormat, float power);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.3f", float power = 1.0f
     */
    bool(* sliderFloat2)(
        const char* label, float v[2], float vMin, float vMax, const char* displayFormat, float power);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.3f", float power = 1.0f
     */
    bool(* sliderFloat3)(
        const char* label, float v[3], float vMin, float vMax, const char* displayFormat, float power);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.3f", float power = 1.0f
     */
    bool(* sliderFloat4)(
        const char* label, float v[4], float vMin, float vMax, const char* displayFormat, float power);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: float vDegreesMin = -360.0f, float vDegreesMax = +360.0f
     */
    bool(* sliderAngle)(const char* label, float* vRad, float vDegreesMin, float vDegreesMax);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.0f"
     */
    bool(* sliderInt)(const char* label, int* v, int vMin, int vMax, const char* displayFormat);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.0f"
     */
    bool(* sliderInt2)(const char* label, int v[2], int vMin, int vMax, const char* displayFormat);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.0f"
     */
    bool(* sliderInt3)(const char* label, int v[3], int vMin, int vMax, const char* displayFormat);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.0f"
     */
    bool(* sliderInt4)(const char* label, int v[4], int vMin, int vMax, const char* displayFormat);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.0f", power = 1.0f
     */
    bool(* sliderScalar)(const char* label,
        DataType dataType,
        void* v,
        const void* vMin,
        const void* vMax,
        const char* displayFormat,
        float power);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.0f", power = 1.0f
     */
    bool(* sliderScalarN)(const char* label,
        DataType dataType,
        void* v,
        int components,
        const void* vMin,
        const void* vMax,
        const char* displayFormat,
        float power);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.3f", float power = 1.0f
     */
    bool(* vSliderFloat)(
        const char* label, const Float2& size, float* v, float vMin, float vMax, const char* displayFormat, float power);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.0f"
     */
    bool(* vSliderInt)(const char* label, const Float2& size, int* v, int vMin, int vMax, const char* displayFormat);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.0f", power = 1.0f
     */
    bool(* vSliderScalar)(const char* label,
        const Float2& size,
        DataType dataType,
        void* v,
        const void* vMin,
        const void* vMax,
        const char* displayFormat,
        float power);

    /**
     * Widgets: Input with Keyboard
     */
    bool(* inputText)(
        const char* label, char* buf, size_t bufSize, InputTextFlags flags, TextEditCallback callback, void* userData);

    /**
     * Widgets: Input with Keyboard
     */
    bool(* inputTextWithHint)(const char* label,
        const char* hint,
        char* buf,
        size_t bufSize,
        InputTextFlags flags,
        TextEditCallback callback,
        void* userData);

    /**
     * Widgets: Input with Keyboard
     */
    bool(* inputTextMultiline)(const char* label,
        char* buf,
        size_t bufSize,
        const Float2& size,
        InputTextFlags flags,
        TextEditCallback callback,
        void* userData);

    /**
     * Widgets: Input with Keyboard. Defaults: float step = 0.0f, float stepFast = 0.0f, int decimalPrecision = -1,
     * InputTextFlags extraFlags = 0
     */
    bool(* inputFloat)(
        const char* label, float* v, float step, float stepFast, int decimalPrecision, InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard. Defaults: int decimalPrecision = -1, InputTextFlags extraFlags = 0
     */
    bool(* inputFloat2)(const char* label, float v[2], int decimalPrecision, InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard. Defaults: int decimalPrecision = -1, InputTextFlags extraFlags = 0
     */
    bool(* inputFloat3)(const char* label, float v[3], int decimalPrecision, InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard. Defaults: int decimalPrecision = -1, InputTextFlags extraFlags = 0
     */
    bool(* inputFloat4)(const char* label, float v[4], int decimalPrecision, InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard. Defaults: int step = 1, int stepFast = 100, InputTextFlags extraFlags = 0
     */
    bool(* inputInt)(const char* label, int* v, int step, int stepFast, InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard
     */
    bool(* inputInt2)(const char* label, int v[2], InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard
     */
    bool(* inputInt3)(const char* label, int v[3], InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard
     */
    bool(* inputInt4)(const char* label, int v[4], InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard. Defaults: double step = 0.0f, double stepFast = 0.0f, const char* displayFormat =
     * "%.6f", InputTextFlags extraFlags = 0
     */
    bool(* inputDouble)(
        const char* label, double* v, double step, double stepFast, const char* displayFormat, InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard. Defaults: double step = 0.0f, double stepFast = 0.0f, const char* displayFormat =
     * "%.6f", InputTextFlags extraFlags = 0
     */
    bool(* inputScalar)(const char* label,
        DataType dataType,
        void* v,
        const void* step,
        const void* stepFast,
        const char* displayFormat,
        InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard. Defaults: double step = 0.0f, double stepFast = 0.0f, const char* displayFormat =
     * "%.6f", InputTextFlags extraFlags = 0
     */
    bool(* inputScalarN)(const char* label,
        DataType dataType,
        void* v,
        int components,
        const void* step,
        const void* stepFast,
        const char* displayFormat,
        InputTextFlags extraFlags);

    /**
     * Widgets: Color Editor/Picker (tip: the ColorEdit* functions have a little colored preview square that can be
     * left-clicked to open a picker, and right-clicked to open an option menu.)
     */
    bool(* colorEdit3)(const char* label, float col[3], ColorEditFlags flags);

    /**
     * Widgets: Color Editor/Picker (tip: the ColorEdit* functions have a little colored preview square that can be
     * left-clicked to open a picker, and right-clicked to open an option menu.)
     */
    bool(* colorEdit4)(const char* label, float col[4], ColorEditFlags flags);

    /**
     * Widgets: Color Editor/Picker (tip: the ColorEdit* functions have a little colored preview square that can be
     * left-clicked to open a picker, and right-clicked to open an option menu.)
     */
    bool(* colorPicker3)(const char* label, float col[3], ColorEditFlags flags);

    /**
     * Widgets: Color Editor/Picker (tip: the ColorEdit* functions have a little colored preview square that can be
     * left-clicked to open a picker, and right-clicked to open an option menu.)
     */
    bool(* colorPicker4)(const char* label, float col[4], ColorEditFlags flags, const float* refCol);

    /**
     * display a colored square/button, hover for details, return true when pressed.
     */
    bool(* colorButton)(const char* descId, const Float4& col, ColorEditFlags flags, Float2 size);

    /**
     * initialize current options (generally on application startup) if you want to select a default format, picker
     * type, etc. User will be able to change many settings, unless you pass the _NoOptions flag to your calls.
     */
    void(* setColorEditOptions)(ColorEditFlags flags);

    /**
     * Tree node. if returning 'true' the node is open and the tree id is pushed into the id stack. user is responsible
     * for calling TreePop().
     */
    bool(* treeNode)(const char* label);

    /**
     * Tree node with string id. read the FAQ about why and how to use ID. to align arbitrary text at the same level as
     * a TreeNode() you can use Bullet().
     */
    bool(* treeNodeString)(const char* strId, const char* fmt, ...);

    /**
     * Tree node with ptr id.
     */
    bool(* treeNodePtr)(const void* ptrId, const char* fmt, ...);

    /**
     * Tree node with flags.
     */
    bool(* treeNodeEx)(const char* label, TreeNodeFlags flags);

    /**
     * Tree node with flags and string id.
     */
    bool(* treeNodeStringEx)(const char* strId, TreeNodeFlags flags, const char* fmt, ...);

    /**
     * Tree node with flags and ptr id.
     */
    bool(* treeNodePtrEx)(const void* ptrId, TreeNodeFlags flags, const char* fmt, ...);

    /**
     * ~ Indent()+PushId(). Already called by TreeNode() when returning true, but you can call Push/Pop yourself for
     * layout purpose
     */
    void(* treePushString)(const char* strId);

    /**
     *
     */
    void(* treePushPtr)(const void* ptrId);

    /**
     * ~ Unindent()+PopId()
     */
    void(* treePop)();

    /**
     * Advance cursor x position by GetTreeNodeToLabelSpacing()
     */
    void(* treeAdvanceToLabelPos)();

    /**
     * Horizontal distance preceding label when using TreeNode*() or Bullet() == (g.FontSize + style.FramePadding.x*2)
     * for a regular unframed TreeNode
     */
    float(* getTreeNodeToLabelSpacing)();

    /**
     * Set next TreeNode/CollapsingHeader open state.
     */
    void(* setNextTreeNodeOpen)(bool isOpen, Condition cond);

    /**
     * If returning 'true' the header is open. doesn't indent nor push on ID stack. user doesn't have to call TreePop().
     */
    bool(* collapsingHeader)(const char* label, TreeNodeFlags flags);

    /**
     * When 'open' isn't NULL, display an additional small close button on upper right of the header
     */
    bool(* collapsingHeaderEx)(const char* label, bool* open, TreeNodeFlags flags);

    /**
     * Selectable. "bool selected" carry the selection state (read-only). Selectable() is clicked is returns true so you
     * can modify your selection state. size.x==0.0: use remaining width, size.x>0.0: specify width. size.y==0.0: use
     * label height, size.y>0.0: specify height.
     */
    bool(* selectable)(const char* label,
        bool selected /* = false*/,
        SelectableFlags flags /* = 0*/,
        const Float2& size /* = Float2(0,0)*/);

    /**
     * Selectable. "bool* selected" point to the selection state (read-write), as a convenient helper.
     */
    bool(* selectableEx)(const char* label,
        bool* selected,
        SelectableFlags flags /* = 0*/,
        const Float2& size /* = Float2(0,0)*/);

    /**
     * ListBox.
     */
    bool(* listBox)(
        const char* label, int* currentItem, const char* const items[], int itemCount, int heightInItems /* = -1*/);

    /**
     * ListBox.
     */
    bool(* listBoxEx)(const char* label,
        int* currentItem,
        bool (*itemsGetterFn)(void* data, int idx, const char** out_text),
        void* data,
        int itemCount,
        int heightInItems /* = -1*/);

    /**
     * ListBox Header. use if you want to reimplement ListBox() will custom data or interactions. make sure to call
     * ListBoxFooter() afterwards.
     */
    bool(* listBoxHeader)(const char* label, const Float2& size /* = Float2(0,0)*/);

    /**
     * ListBox Header.
     */
    bool(* listBoxHeaderEx)(const char* label, int itemCount, int heightInItems /* = -1*/);

    /**
     * Terminate the scrolling region
     */
    void(* listBoxFooter)();

    /**
     * Plot
     * defaults:
     *      valuesOffset = 0
     *      overlayText = nullptr
     *      scaleMin = FLT_MAX
     *      scaleMax = FLT_MAX
     *      graphSize = Float2(0,0)
     *      stride = sizeof(float)
     */
    void(* plotLines)(const char* label,
        const float* values,
        int valuesCount,
        int valuesOffset,
        const char* overlayText,
        float scaleMin,
        float scaleMax,
        Float2 graphSize,
        int stride);

    /**
     * Plot
     * defaults:
     *      valuesOffset = 0
     *      overlayText = nullptr
     *      scaleMin = FLT_MAX
     *      scaleMax = FLT_MAX
     *      graphSize = Float2(0,0)
     */
    void(* plotLinesEx)(const char* label,
        float (*valuesGetterFn)(void* data, int idx),
        void* data,
        int valuesCount,
        int valuesOffset,
        const char* overlayText,
        float scaleMin,
        float scaleMax,
        Float2 graphSize);

    /**
     * Histogram
     * defaults:
     *      valuesOffset = 0
     *      overlayText = nullptr
     *      scaleMin = FLT_MAX
     *      scaleMax = FLT_MAX
     *      graphSize = Float2(0,0)
     *      stride = sizeof(float)
     */
    void(* plotHistogram)(const char* label,
        const float* values,
        int valuesCount,
        int valuesOffset,
        const char* overlayText,
        float scaleMin,
        float scaleMax,
        Float2 graphSize,
        int stride);

    /**
     * Histogram
     * defaults:
     *      valuesOffset = 0
     *      overlayText = nullptr
     *      scaleMin = FLT_MAX
     *      scaleMax = FLT_MAX
     *      graphSize = Float2(0,0)
     */
    void(* plotHistogramEx)(const char* label,
        float (*valuesGetterFn)(void* data, int idx),
        void* data,
        int valuesCount,
        int valuesOffset,
        const char* overlayText,
        float scaleMin,
        float scaleMax,
        Float2 graphSize);

    /**
     * Widgets: Value() Helpers. Output single value in "name: value" format.
     */
    void(* valueBool)(const char* prefix, bool b);

    /**
     * Widgets: Value() Helpers. Output single value in "name: value" format.
     */
    void(* valueInt)(const char* prefix, int v);

    /**
     * Widgets: Value() Helpers. Output single value in "name: value" format.
     */
    void(* valueUInt32)(const char* prefix, uint32_t v);

    /**
     * Widgets: Value() Helpers. Output single value in "name: value" format.
     */
    void(* valueFloat)(const char* prefix, float v, const char* floatFormat /* = nullptr*/);

    /**
     * Create and append to a full screen menu-bar.
     */
    bool(* beginMainMenuBar)();

    /**
     * Only call EndMainMenuBar() if BeginMainMenuBar() returns true!
     */
    void(* endMainMenuBar)();

    /**
     * Append to menu-bar of current window (requires WindowFlags_MenuBar flag set on parent window).
     */
    bool(* beginMenuBar)();

    /**
     * Only call EndMenuBar() if BeginMenuBar() returns true!
     */
    void(* endMenuBar)();

    /**
     * Create a sub-menu entry. only call EndMenu() if this returns true!
     */
    bool(* beginMenu)(const char* label, bool enabled /* = true*/);

    /**
     * Only call EndMenu() if BeginMenu() returns true!
     */
    void(* endMenu)();

    /**
     * Return true when activated. shortcuts are displayed for convenience but not processed by ImGui at the moment
     */
    bool(* menuItem)(const char* label,
        const char* shortcut /* = NULL*/,
        bool selected /* = false*/,
        bool enabled /* = true*/);

    /**
     * Return true when activated + toggle (*pSelected) if pSelected != NULL
     */
    bool(* menuItemEx)(const char* label, const char* shortcut, bool* pSelected, bool enabled /* = true*/);

    /**
     * Set text tooltip under mouse-cursor, typically use with ImGui::IsItemHovered(). overidde any previous call to
     * SetTooltip().
     */
    void(* setTooltip)(const char* fmt, ...);

    /**
     * Begin/append a tooltip window. to create full-featured tooltip (with any kind of contents).
     */
    void(* beginTooltip)();

    /**
     * End tooltip
     */
    void(* endTooltip)();

    /**
     * Call to mark popup as open (don't call every frame!). popups are closed when user click outside, or if
     * CloseCurrentPopup() is called within a BeginPopup()/EndPopup() block. By default, Selectable()/MenuItem() are
     * calling CloseCurrentPopup(). Popup identifiers are relative to the current ID-stack (so OpenPopup and BeginPopup
     * needs to be at the same level).
     */
    void(* openPopup)(const char* strId);

    /**
     * Return true if the popup is open, and you can start outputting to it. only call EndPopup() if BeginPopup()
     * returns true!
     */
    bool(* beginPopup)(const char* strId, WindowFlags flags /* = 0*/);

    /**
     * Helper to open and begin popup when clicked on last item. if you can pass a NULL strId only if the previous item
     * had an id. If you want to use that on a non-interactive item such as Text() you need to pass in an explicit ID
     * here. read comments in .cpp!
     */
    bool(* beginPopupContextItem)(const char* strId /* = NULL*/, int mouseButton /* = 1*/);

    /**
     * Helper to open and begin popup when clicked on current window.
     */
    bool(* beginPopupContextWindow)(const char* strId /* = NULL*/,
        int mouseButton /* = 1*/,
        bool alsoOverItems /* = true*/);

    /**
     * Helper to open and begin popup when clicked in void (where there are no imgui windows).
     */
    bool(* beginPopupContextVoid)(const char* strId /* = NULL*/, int mouseButton /* = 1*/);

    /**
     * Modal dialog (regular window with title bar, block interactions behind the modal window, can't close the modal
     * window by clicking outside)
     */
    bool(* beginPopupModal)(const char* name, bool* open /* = NULL*/, WindowFlags flags /* = 0*/);

    /**
     * Only call EndPopup() if BeginPopupXXX() returns true!
     */
    void(* endPopup)();

    /**
     * Helper to open popup when clicked on last item. return true when just opened.
     */
    bool(* openPopupOnItemClick)(const char* strId /* = NULL*/, int mouseButton /* = 1*/);

    /**
     * Return true if the popup is open
     */
    bool(* isPopupOpen)(const char* strId);

    /**
     * Close the popup we have begin-ed into. clicking on a MenuItem or Selectable automatically close the current
     * popup.
     */
    void(* closeCurrentPopup)();

    /**
     * Columns. You can also use SameLine(pos_x) for simplified columns. The columns API is still work-in-progress and
     * rather lacking.
     */
    void(* columns)(int count /* = 1*/, const char* id /* = NULL*/, bool border /* = true*/);

    /**
     * Next column, defaults to current row or next row if the current row is finished
     */
    void(* nextColumn)();

    /**
     * Get current column index
     */
    int(* getColumnIndex)();

    /**
     * Get column width (in pixels). pass -1 to use current column
     */
    float(* getColumnWidth)(int columnIndex /* = -1*/);

    /**
     * Set column width (in pixels). pass -1 to use current column
     */
    void(* setColumnWidth)(int columnIndex, float width);

    /**
     * Get position of column line (in pixels, from the left side of the contents region). pass -1 to use current
     * column, otherwise 0..GetColumnsCount() inclusive. column 0 is typically 0.0f
     */
    float(* getColumnOffset)(int columnIndex /* = -1*/);

    /**
     * Set position of column line (in pixels, from the left side of the contents region). pass -1 to use current column
     */
    void(* setColumnOffset)(int columnIndex, float offsetX);

    /**
     * Columnts count.
     */
    int(* getColumnsCount)();

    /**
     * Create and append into a TabBar.
     * defaults:
     *  flags = 0
     */
    bool(* beginTabBar)(const char* strId, TabBarFlags flags);

    /**
     * End TabBar.
     */
    void(* endTabBar)();

    /**
     * Create a Tab. Returns true if the Tab is selected.
     * defaults:
     *  open = nullptr
     *  flags = 0
     */
    bool(* beginTabItem)(const char* label, bool* open, TabItemFlags flags);

    /**
     * Only call endTabItem() if beginTabItem() returns true!
     */
    void(* endTabItem)();

    /**
     * Notify TabBar or Docking system of a closed tab/window ahead (useful to reduce visual flicker on reorderable
     * tab bars). For tab-bar: call after beginTabBar() and before Tab submissions. Otherwise call with a window name.
     */
    void(* setTabItemClosed)(const char* tabOrDockedWindowLabel);

    /**
     * defaults:
     *  size = Float2(0, 0),
     *  flags = 0,
     *  windowClass = nullptr
     */
    void(* dockSpace)(uint32_t id, const Float2& size, DockNodeFlags flags, const WindowClass* windowClass);

    /**
     * defaults:
     *  viewport = nullptr,
     *  dockspaceFlags = 0,
     *  windowClass = nullptr
     */
    uint32_t(* dockSpaceOverViewport)(Viewport* viewport,
        DockNodeFlags dockspaceFlags,
        const WindowClass* windowClass);

    /**
     * Set next window dock id (FIXME-DOCK).
     */
    void(* setNextWindowDockId)(uint32_t dockId, Condition cond);

    /**
     * Set next window user type (docking filters by same user_type).
     */
    void(* setNextWindowClass)(const WindowClass* windowClass);

    /**
     * Get window dock Id.
     */
    uint32_t(* getWindowDockId)();

    /**
     * Gets the window dock node.
     */
    DockNode* (* getWindowDockNode)();

    /**
     * Return is window Docked.
     */
    bool(* isWindowDocked)();

    /**
     * Call when the current item is active. If this return true, you can call setDragDropPayload() +
     * endDragDropSource()
     */
    bool(* beginDragDropSource)(DragDropFlags flags);

    /**
     * Type is a user defined string of maximum 32 characters. Strings starting with '_' are reserved for dear imgui
     * internal types. Data is copied and held by imgui. Defaults: cond = 0
     */
    bool(* setDragDropPayload)(const char* type, const void* data, size_t size, Condition cond);

    /**
     * Only call endDragDropSource() if beginDragDropSource() returns true!
     */
    void(* endDragDropSource)();

    /**
     * Call after submitting an item that may receive a payload. If this returns true, you can call
     * acceptDragDropPayload() + endDragDropTarget()
     */
    bool(* beginDragDropTarget)();

    /**
     * Accept contents of a given type. If ImGuiDragDropFlags_AcceptBeforeDelivery is set you can peek into the payload
     * before the mouse button is released.
     */
    const Payload* (* acceptDragDropPayload)(const char* type, DragDropFlags flags);

    /**
     * Only call endDragDropTarget() if beginDragDropTarget() returns true!
     */
    void(* endDragDropTarget)();

    /**
     * Peek directly into the current payload from anywhere. may return NULL. use ImGuiPayload::IsDataType() to test for
     * the payload type.
     */
    const Payload* (* getDragDropPayload)();

    /**
     * Clipping.
     */
    void(* pushClipRect)(const Float2& clipRectMin, const Float2& clipRectMax, bool intersectWithCurrentClipRect);

    /**
     * Clipping.
     */
    void(* popClipRect)();

    /**
     * Make last item the default focused item of a window. Please use instead of "if (IsWindowAppearing())
     * SetScrollHere()" to signify "default item".
     */
    void(* setItemDefaultFocus)();

    /**
     * Focus keyboard on the next widget. Use positive 'offset' to access sub components of a multiple component widget.
     * Use -1 to access previous widget.
     */
    void(* setKeyboardFocusHere)(int offset /* = 0*/);

    /**
     * Clears the active element id in the internal state.
     */
    void(* clearActiveId)();

    /**
     * Is the last item hovered? (and usable, aka not blocked by a popup, etc.). See HoveredFlags for more options.
     */
    bool(* isItemHovered)(HoveredFlags flags /* = 0*/);

    /**
     * Is the last item active? (e.g. button being held, text field being edited- items that don't interact will always
     * return false)
     */
    bool(* isItemActive)();

    /**
     * Is the last item focused for keyboard/gamepad navigation?
     */
    bool(* isItemFocused)();

    /**
     * Is the last item clicked? (e.g. button/node just clicked on)
     */
    bool(* isItemClicked)(int mouseButton /* = 0*/);

    /**
     * Is the last item visible? (aka not out of sight due to clipping/scrolling.)
     */
    bool(* isItemVisible)();

    /**
     * Is the last item visible? (items may be out of sight because of clipping/scrolling)
     */
    bool(* isItemEdited)();

    /**
     * Was the last item just made inactive (item was previously active).
     *
     * Useful for Undo/Redo patterns with widgets that requires continuous editing.
     */
    bool(* isItemDeactivated)();

    /**
     * Was the last item just made inactive and made a value change when it was active? (e.g. Slider/Drag moved).
     *
     * Useful for Undo/Redo patterns with widgets that requires continuous editing.
     * Note that you may get false positives (some widgets such as Combo()/ListBox()/Selectable()
     will return true even when clicking an already selected item).
     */
    bool(* isItemDeactivatedAfterEdit)();

    /**
     *
     */
    bool(* isAnyItemHovered)();

    /**
     *
     */
    bool(* isAnyItemActive)();

    /**
     *
     */
    bool(* isAnyItemFocused)();

    /**
     * Get bounding rectangle of last item, in screen space
     */
    Float2(* getItemRectMin)();

    /**
     *
     */
    Float2(* getItemRectMax)();

    /**
     * Get size of last item, in screen space
     */
    Float2(* getItemRectSize)();

    /**
     * Allow last item to be overlapped by a subsequent item. sometimes useful with invisible buttons, selectables, etc.
     * to catch unused area.
     */
    void(* setItemAllowOverlap)();

    /**
     * Test if rectangle (of given size, starting from cursor position) is visible / not clipped.
     */
    bool(* isRectVisible)(const Float2& size);

    /**
     * Test if rectangle (in screen space) is visible / not clipped. to perform coarse clipping on user's side.
     */
    bool(* isRectVisibleEx)(const Float2& rectMin, const Float2& rectMax);

    /**
     * Time.
     */
    float(* getTime)();

    /**
     * Frame Count.
     */
    int(* getFrameCount)();

    /**
     * This draw list will be the last rendered one, useful to quickly draw overlays shapes/text
     */
    DrawList* (* getOverlayDrawList)();

    /**
     *
     */
    const char* (* getStyleColorName)(StyleColor color);

    /**
     *
     */
    Float2(* calcTextSize)(const char* text,
        const char* textEnd /* = nullptr*/,
        bool hideTextAfterDoubleHash /* = false*/,
        float wrap_width /* = -1.0f*/);

    /**
     * Calculate coarse clipping for large list of evenly sized items. Prefer using the ImGuiListClipper higher-level
     * helper if you can.
     */
    void(* calcListClipping)(int itemCount, float itemsHeight, int* outItemsDisplayStart, int* outItemsDisplayEnd);

    /**
     * Helper to create a child window / scrolling region that looks like a normal widget frame
     */
    bool(* beginChildFrame)(uint32_t id, const Float2& size, WindowFlags flags /* = 0*/);

    /**
     * Always call EndChildFrame() regardless of BeginChildFrame() return values (which indicates a collapsed/clipped
     * window)
     */
    void(* endChildFrame)();

    /**
     *
     */
    Float4(* colorConvertU32ToFloat4)(uint32_t in);

    /**
     *
     */
    uint32_t(* colorConvertFloat4ToU32)(const Float4& in);

    /**
     *
     */
    void(* colorConvertRGBtoHSV)(float r, float g, float b, float& outH, float& outS, float& outV);

    /**
     *
     */
    void(* colorConvertHSVtoRGB)(float h, float s, float v, float& outR, float& outG, float& outB);

    /**
     * Map ImGuiKey_* values into user's key index. == io.KeyMap[key]
     */
    int(* getKeyIndex)(KeyIndices imguiKeyIndex);

    /**
     * Is key being held. == io.KeysDown[userKeyIndex]. note that imgui doesn't know the semantic of each entry of
     * io.KeyDown[]. Use your own indices/enums according to how your backend/engine stored them into KeyDown[]!
     */
    bool(* isKeyDown)(int userKeyIndex);

    /**
     * Was key pressed (went from !Down to Down). if repeat=true, uses io.KeyRepeatDelay / KeyRepeatRate
     */
    bool(* isKeyPressed)(int userKeyIndex, bool repeat /* = true*/);

    /**
     * Was key released (went from Down to !Down)..
     */
    bool(* isKeyReleased)(int userKeyIndex);

    /**
     * Uses provided repeat rate/delay. return a count, most often 0 or 1 but might be >1 if RepeatRate is small enough
     * that DeltaTime > RepeatRate
     */
    int(* getKeyPressedAmount)(int keyIndex, float repeatDelay, float rate);

    /**
     * Gets the key modifiers for each frame.
     *
     * Shortcut to bitwise modifier from ImGui::GetIO().KeyCtrl + KeyShift + KeyAlt + `KeySuper`
     *
     * @return The key modifiers for each frame.
     */
    KeyModifiers(* getKeyModifiers)();

    /**
     * Is mouse button held
     */
    bool(* isMouseDown)(int button);

    /**
     * Is any mouse button held
     */
    bool(* isAnyMouseDown)();

    /**
     * Did mouse button clicked (went from !Down to Down)
     */
    bool(* isMouseClicked)(int button, bool repeat /* = false*/);

    /**
     * Did mouse button double-clicked. a double-click returns false in IsMouseClicked(). uses io.MouseDoubleClickTime.
     */
    bool(* isMouseDoubleClicked)(int button);

    /**
     * Did mouse button released (went from Down to !Down)
     */
    bool(* isMouseReleased)(int button);

    /**
     * Is mouse dragging. if lockThreshold < -1.0f uses io.MouseDraggingThreshold
     */
    bool(* isMouseDragging)(int button /* = 0*/, float lockThreshold /* = -1.0f*/);

    /**
     * Is mouse hovering given bounding rect (in screen space). clipped by current clipping settings. disregarding of
     * consideration of focus/window ordering/blocked by a popup.
     */
    bool(* isMouseHoveringRect)(const Float2& rMin, const Float2& rMax, bool clip /* = true*/);

    /**
     *
     */
    bool(* isMousePosValid)(const Float2* mousePos /* = nullptr*/);

    /**
     * Shortcut to ImGui::GetIO().MousePos provided by user, to be consistent with other calls
     */
    Float2(* getMousePos)();

    /**
     * Retrieve backup of mouse position at the time of opening popup we have BeginPopup() into
     */
    Float2(* getMousePosOnOpeningCurrentPopup)();

    /**
     * Dragging amount since clicking. if lockThreshold < -1.0f uses io.MouseDraggingThreshold
     */
    Float2(* getMouseDragDelta)(int button /* = 0*/, float lockThreshold /* = -1.0f*/);

    /**
     *
     */
    void(* resetMouseDragDelta)(int button /* = 0*/);

    /**
     * Gets the mouse wheel delta for each frame.
     *
     * Shortcut to ImGui::GetIO().MouseWheel + MouseWheelH.
     *
     * @return The mouse wheel delta for each frame.
     */
    type::Float2(* getMouseWheel)();

    /**
     * Get desired cursor type, reset in ImGui::NewFrame(), this is updated during the frame. valid before Render(). If
     * you use software rendering by setting io.MouseDrawCursor ImGui will render those for you
     */
    MouseCursor(* getMouseCursor)();

    /**
     * Set desired cursor type
     */
    void(* setMouseCursor)(MouseCursor type);

    /**
     * Manually override io.WantCaptureKeyboard flag next frame (said flag is entirely left for your application to
     * handle). e.g. force capture keyboard when your widget is being hovered.
     */
    void(* captureKeyboardFromApp)(bool capture /* = true*/);

    /**
     * Manually override io.WantCaptureMouse flag next frame (said flag is entirely left for your application to
     * handle).
     */
    void(* captureMouseFromApp)(bool capture /* = true*/);

    /**
     * Used to capture text data to the clipboard.
     *
     * @return The text captures from the clipboard
     */
    const char* (* getClipboardText)();

    /**
     * Used to apply text into the clipboard.
     *
     * @param text The text to be set into the clipboard.
     */
    void(* setClipboardText)(const char* text);

    /**
     * Shortcut to ImGui::GetIO().WantSaveIniSettings provided by user, to be consistent with other calls
     */
    bool(* getWantSaveIniSettings)();

    /**
     * Shortcut to ImGui::GetIO().WantSaveIniSettings provided by user, to be consistent with other calls
     */
    void(* setWantSaveIniSettings)(bool wantSaveIniSettings);

    /**
     * Manually load the previously saved setting from memory loaded from an .ini settings file.
     *
     * @param iniData The init data to be loaded.
     * @param initSize The size of the ini data to be loaded.
     */
    void(* loadIniSettingsFromMemory)(const char* iniData, size_t iniSize);

    /**
     * Manually save settings to a ini memory as a string.
     *
     * @param iniSize[out] The ini size of memory to be saved.
     * @return The memory
     */
    const char* (* saveIniSettingsToMemory)(size_t* iniSize);

    /**
     * Main viewport. Same as GetPlatformIO().MainViewport == GetPlatformIO().Viewports[0]
     */
    Viewport* (* getMainViewport)();

    /**
     * Associates a windowName to a dock node id.
     *
     * @param windowName The name of the window.
     * @param nodeId The dock node id.
     */
    void(* dockBuilderDockWindow)(const char* windowName, uint32_t nodeId);

    /**
     * DO NOT HOLD ON ImGuiDockNode* pointer, will be invalided by any split/merge/remove operation.
     */
    DockNode* (* dockBuilderGetNode)(uint32_t nodeId);

    /**
     * Defaults:
     *  flags = 0
     */
    void(* dockBuilderAddNode)(uint32_t nodeId, DockNodeFlags flags);

    /**
     * Remove node and all its child, undock all windows
     */
    void(* dockBuilderRemoveNode)(uint32_t nodeId);

    /**
     * Defaults:
     *  clearPersistentDockingReferences = true
     */
    void(* dockBuilderRemoveNodeDockedWindows)(uint32_t nodeId, bool clearPersistentDockingReferences);

    /**
     * Remove all split/hierarchy. All remaining docked windows will be re-docked to the root.
     */
    void(* dockBuilderRemoveNodeChildNodes)(uint32_t nodeId);

    /**
     * Dock building split node.
     */
    uint32_t(* dockBuilderSplitNode)(
        uint32_t nodeId, Direction splitDir, float sizeRatioForNodeAtDir, uint32_t* outIdDir, uint32_t* outIdOther);

    /**
     * Dock building finished.
     */
    void(* dockBuilderFinish)(uint32_t nodeId);

    Font* (* addFont)(const FontConfig* fontConfig);

    Font* (* addFontDefault)(const FontConfig* fontConfig /* = NULL */);

    Font* (* addFontFromFileTTF)(const char* filename,
        float sizePixels,
        const FontConfig* fontCfg /*= NULL */,
        const Wchar* glyphRanges /*= NULL*/);

    Font* (* addFontFromMemoryTTF)(void* fontData,
        int fontSize,
        float sizePixels,
        const FontConfig* fontCfg /* = NULL */,
        const Wchar* glyphRanges /* = NULL */);

    Font* (* addFontFromMemoryCompressedTTF)(const void* compressedFontData,
        int compressedFontSize,
        float sizePixels,
        const FontConfig* fontCfg /* = NULL */,
        const Wchar* glyphRanges /*= NULL */);

    Font* (* addFontFromMemoryCompressedBase85TTF)(const char* compressedFontDataBase85,
        float sizePixels,
        const FontConfig* fontCfg /* = NULL */,
        const Wchar* glyphRanges /* = NULL */);

    /**
     * Add a custom rect glyph that can be built into the font atlas. Call buildFont after.
     *
     * @param font The font to add to.
     * @param id The unicode point to add for.
     * @param width The width of the glyph.
     * @param height The height of the glyph
     * @param advanceX The advance x for the glyph.
     * @param offset The glyph offset.
     * @return The glpyh index.
     */
    int(* addFontCustomRectGlyph)(
        Font* font, Wchar id, int width, int height, float advanceX, const type::Float2& offset /* (0, 0) */);

    /**
     * Gets the font custom rect by glyph index.
     *
     * @param index The glyph index to get the custom rect information for.
     * @return The font glyph custom rect information.
     */
    const FontCustomRect* (* getFontCustomRectByIndex)(int index);

    /**
     * Builds the font atlas.
     *
     * @return true if the font atlas was built sucessfully.
     */
    bool(* buildFont)();

    /**
     * Determines if changes have been made to font atlas
     *
     * @return true if the font atlas is built.
     */
    bool(* isFontBuilt)();

    /**
     * Gets the font texture data.
     *
     * @param outPixel The pixel texture data in A8 format.
     * @param outWidth The texture width.
     * @param outPixel The texture height.
     */
    void(* getFontTexDataAsAlpha8)(unsigned char** outPixels, int* outWidth, int* outHeight);

    /**
     * Gets the font atlas texture data.
     *
     * @param outPixel The pixel texture data in RGBA32 format.
     * @param outWidth The texture width.
     * @param outPixel The texture height.
     */
    void(* getFontTexDataAsRgba32)(unsigned char** outPixels, int* outWidth, int* outHeight);

    /**
     * Clear input data (all ImFontConfig structures including sizes, TTF data, glyph ranges, etc.) = all the data used
     * to build the texture and fonts.
     */
    void(* clearFontInputData)();

    /**
     * Clear output texture data (CPU side). Saves RAM once the texture has been copied to graphics memory.
     */
    void(* clearFontTexData)();

    /**
     * Clear output font data (glyphs storage, UV coordinates).
     */
    void(* clearFonts)();

    /**
     * Clear all input and output.
     */
    void(* clearFontInputOutput)();

    /**
     * Basic Latin, Extended Latin
     */
    const Wchar* (* getFontGlyphRangesDefault)();

    /**
     * Default + Korean characters
     */
    const Wchar* (* getFontGlyphRangesKorean)();

    /**
     * Default + Hiragana, Katakana, Half-Width, Selection of 1946 Ideographs
     */
    const Wchar* (* getFontGlyphRangesJapanese)();

    /**
     * Default + Half-Width + Japanese Hiragana/Katakana + full set of about 21000 CJK Unified Ideographs
     */
    const Wchar* (* getFontGlyphRangesChineseFull)();

    /**
     * Default + Half-Width + Japanese Hiragana/Katakana + set
     * of 2500 CJK Unified Ideographs for common simplified Chinese
     */
    const Wchar* (* getGlyphRangesChineseSimplifiedCommon)();

    /**
     * Default + about 400 Cyrillic characters
     */
    const Wchar* (* getFontGlyphRangesCyrillic)();

    /**
     * Default + Thai characters
     */
    const Wchar* (* getFontGlyphRangesThai)();

    /**
     * set Global Font Scale
     */
    void(* setFontGlobalScale)(float scale);

    /**
     * Shortcut for getWindowDrawList() + DrawList::AddCallback()
     */
    void(* addWindowDrawCallback)(DrawCallback callback, void* userData);

    /**
     * Adds a line to the draw list.
     */
    void(* addLine)(DrawList* drawList, const type::Float2& a, const type::Float2& b, uint32_t col, float thickness);

    /**
     * Adds a rect to the draw list.
     *
     * @param a Upper-left.
     * @param b Lower-right.
     * @param col color.
     * @param rounding Default = 0.f;
     * @param roundingCornersFlags 4-bits corresponding to which corner to round. Default = kDrawCornerFlagAll
     * @param thickness Default = 1.0f
     */
    void(* addRect)(DrawList* drawList,
        const type::Float2& a,
        const type::Float2& b,
        uint32_t col,
        float rounding,
        DrawCornerFlags roundingCornersFlags,
        float thickness);

    /**
     * Adds a filled rect to the draw list.
     *
     * @param a Upper-left.
     * @param b Lower-right.
     * @param col color.
     * @param rounding Default = 0.f;
     * @param roundingCornersFlags 4-bits corresponding to which corner to round. Default = kDrawCornerFlagAll
     */
    void(* addRectFilled)(DrawList* drawList,
        const type::Float2& a,
        const type::Float2& b,
        uint32_t col,
        float rounding,
        DrawCornerFlags roundingCornersFlags);

    /**
     * Adds a filled multi-color rect to the draw list.
     */
    void(* addRectFilledMultiColor)(DrawList* drawList,
        const type::Float2& a,
        const type::Float2& b,
        uint32_t colUprLeft,
        uint32_t colUprRight,
        uint32_t colBotRight,
        uint32_t colBotLeft);
    /**
     * Adds a quad to the draw list.
     * Default: thickness = 1.0f.
     */
    void(* addQuad)(DrawList* drawList,
        const type::Float2& a,
        const type::Float2& b,
        const type::Float2& c,
        const type::Float2& d,
        uint32_t col,
        float thickness);

    /**
     * Adds a filled quad to the draw list.
     */
    void(* addQuadFilled)(DrawList* drawList,
        const type::Float2& a,
        const type::Float2& b,
        const type::Float2& c,
        const type::Float2& d,
        uint32_t col);

    /**
     * Adds a triangle to the draw list.
     * Defaults: thickness = 1.0f.
     */
    void(* addTriangle)(DrawList* drawList,
        const type::Float2& a,
        const type::Float2& b,
        const type::Float2& c,
        uint32_t col,
        float thickness);

    /**
     * Adds a filled triangle to the draw list.
     */
    void(* addTriangleFilled)(
        DrawList* drawList, const type::Float2& a, const type::Float2& b, const type::Float2& c, uint32_t col);

    /**
     * Adds a circle to the draw list.
     * Defaults: numSegments = 12, thickness = 1.0f.
     */
    void(* addCircle)(
        DrawList* drawList, const type::Float2& centre, float radius, uint32_t col, int32_t numSegments, float thickness);

    /**
     * Adds a filled circle to the draw list.
     * Defaults: numSegments = 12, thickness = 1.0f.
     */
    void(* addCircleFilled)(
        DrawList* drawList, const type::Float2& centre, float radius, uint32_t col, int32_t numSegments);

    /**
     * Adds text to the draw list.
     */
    void(* addText)(
        DrawList* drawList, const type::Float2& pos, uint32_t col, const char* textBegin, const char* textEnd);

    /**
     * Adds text to the draw list.
     * Defaults: textEnd = nullptr, wrapWidth = 0.f, cpuFineClipRect = nullptr.
     */
    void(* addTextEx)(DrawList* drawList,
        const Font* font,
        float fontSize,
        const type::Float2& pos,
        uint32_t col,
        const char* textBegin,
        const char* textEnd,
        float wrapWidth,
        const type::Float4* cpuFineClipRect);

    /**
     * Adds an image to the draw list.
     */
    void(* addImage)(DrawList* drawList,
        TextureId textureId,
        const type::Float2& a,
        const type::Float2& b,
        const type::Float2& uvA,
        const type::Float2& uvB,
        uint32_t col);

    /**
     * Adds an image quad to the draw list.
     * defaults: uvA = (0, 0), uvB = (1, 0), uvC = (1, 1), uvD = (0, 1), col = 0xFFFFFFFF.
     */
    void(* addImageQuad)(DrawList* drawList,
        TextureId textureId,
        const type::Float2& a,
        const type::Float2& b,
        const type::Float2& c,
        const type::Float2& d,
        const type::Float2& uvA,
        const type::Float2& uvB,
        const type::Float2& uvC,
        const type::Float2& uvD,
        uint32_t col);

    /**
     * Adds an rounded image to the draw list.
     * defaults: roundingCorners = kDrawCornerFlagAll
     */
    void(* addImageRounded)(DrawList* drawList,
        TextureId textureId,
        const type::Float2& a,
        const type::Float2& b,
        const type::Float2& uvA,
        const type::Float2& uvB,
        uint32_t col,
        float rounding,
        DrawCornerFlags roundingCorners);

    /**
     * Adds a polygon line to the draw list.
     */
    void(* addPolyline)(DrawList* drawList,
        const type::Float2* points,
        const int32_t numPoints,
        uint32_t col,
        bool closed,
        float thickness);

    /**
     * Adds a filled convex polygon to draw list.
     * Note: Anti-aliased filling requires points to be in clockwise order.
     */
    void(* addConvexPolyFilled)(DrawList* drawList,
        const type::Float2* points,
        const int32_t numPoints,
        uint32_t col);

    /**
     * Adds a bezier curve to draw list.
     * defaults: numSegments = 0.
     */
    void(* addBezierCurve)(DrawList* drawList,
        const type::Float2& pos0,
        const type::Float2& cp0,
        const type::Float2& cp1,
        const type::Float2& pos1,
        uint32_t col,
        float thickness,
        int32_t numSegments);

    /**
     * Creates a ListClipper to clip large list of items.
     *
     * @param itemsCount Number of items to clip. Use INT_MAX if you don't know how many items you have (in which case
     * the cursor won't be advanced in the final step)
     * @param float itemsHeight Use -1.0f to be calculated automatically on first step. Otherwise pass in the distance
     * between your items, typically getTextLineHeightWithSpacing() or getFrameHeightWithSpacing().
     *
     * @return returns the created ListClipper instance.
     */
    ListClipper* (* createListClipper)(int32_t itemsCount, float itemsHeight);

    /**
     * Call until it returns false. The displayStart/displayEnd fields will be set and you can process/draw those items.
     *
     * @param listClipper The listClipper instance to advance.
     */
    bool(* stepListClipper)(ListClipper* listClipper);

    /**
     * Destroys a listClipper instance.
     *
     * @param listClipper The listClipper instance to destroy.
     */
    void(* destroyListClipper)(ListClipper* listClipper);

    /**
     * Feed the keyboard event into the imgui.
     *
     * @param ctx The context to be fed input event to.
     * @param event Keyboard event description.
     */
    bool(* feedKeyboardEvent)(Context* ctx, const input::KeyboardEvent& event);

    /**
     * Feed the mouse event into the imgui.
     *
     * @param ctx The context to be fed input event to.
     * @param event Mouse event description.
     */
    bool(* feedMouseEvent)(Context* ctx, const input::MouseEvent& event);

    /**
     * Return true if a modal popup is open
     */
    bool(* isModalPopupOpen)();
};

}
}
