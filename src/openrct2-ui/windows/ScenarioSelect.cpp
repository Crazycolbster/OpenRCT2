/*****************************************************************************
 * Copyright (c) 2014-2023 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "../interface/Theme.h"

#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Window.h>
#include <openrct2/Context.h>
#include <openrct2/audio/audio.h>
#include <openrct2/config/Config.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/localisation/Date.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/localisation/Localisation.h>
#include <openrct2/localisation/LocalisationService.h>
#include <openrct2/ride/RideData.h>
#include <openrct2/scenario/Scenario.h>
#include <openrct2/scenario/ScenarioRepository.h>
#include <openrct2/scenario/ScenarioSources.h>
#include <openrct2/sprites.h>
#include <openrct2/util/Util.h>
#include <vector>

static constexpr StringId WINDOW_TITLE = STR_SELECT_SCENARIO;
static constexpr int32_t WW = 734;
static constexpr int32_t WH = 384;
static constexpr int32_t SidebarWidth = 180;
static constexpr int32_t TabWidth = 92;
static constexpr int32_t TabHeight = 34;
static constexpr int32_t TrueFontSize = 24;
static constexpr int32_t WidgetsStart = 17;
static constexpr int32_t TabsStart = WidgetsStart;
static constexpr int32_t InitialNumUnlockedScenarios = 5;
constexpr uint8_t NumTabs = 10;

enum class ListItemType : uint8_t
{
    Heading,
    Scenario,
};

struct ScenarioListItem
{
    ListItemType type;
    union
    {
        struct
        {
            StringId string_id;
        } heading;
        struct
        {
            const ScenarioIndexEntry* scenario;
            bool is_locked;
        } scenario;
    };
};

enum
{
    WIDX_BACKGROUND,
    WIDX_TITLEBAR,
    WIDX_CLOSE,
    WIDX_TABCONTENT,
    WIDX_TAB1,
    WIDX_TAB2,
    WIDX_TAB3,
    WIDX_TAB4,
    WIDX_TAB5,
    WIDX_TAB6,
    WIDX_TAB7,
    WIDX_TAB8,
    WIDX_TAB9,
    WIDX_TAB10,
    WIDX_SCENARIOLIST
};

static constexpr StringId kScenarioOriginStringIds[] = {
    STR_SCENARIO_CATEGORY_RCT1,        STR_SCENARIO_CATEGORY_RCT1_AA,    STR_SCENARIO_CATEGORY_RCT1_LL,
    STR_SCENARIO_CATEGORY_RCT2,        STR_SCENARIO_CATEGORY_RCT2_WW,    STR_SCENARIO_CATEGORY_RCT2_TT,
    STR_SCENARIO_CATEGORY_UCES,        STR_SCENARIO_CATEGORY_REAL_PARKS, STR_SCENARIO_CATEGORY_EXTRAS_PARKS,
    STR_SCENARIO_CATEGORY_OTHER_PARKS,
};

// clang-format off
static Widget _scenarioSelectWidgets[] = {
    WINDOW_SHIM(WINDOW_TITLE, WW, WH),
    MakeWidget({ TabWidth + 1, WidgetsStart }, { WW, 284 }, WindowWidgetType::Resize, WindowColour::Secondary), // tab content panel
    MakeRemapWidget({ 3, TabsStart + (TabHeight * 0) }, { TabWidth, TabHeight}, WindowWidgetType::Tab, WindowColour::Secondary, SPR_G2_SIDEWAYS_TAB), // tab 01
    MakeRemapWidget({ 3, TabsStart + (TabHeight * 1) }, { TabWidth, TabHeight}, WindowWidgetType::Tab, WindowColour::Secondary, SPR_G2_SIDEWAYS_TAB), // tab 02
    MakeRemapWidget({ 3, TabsStart + (TabHeight * 2) }, { TabWidth, TabHeight}, WindowWidgetType::Tab, WindowColour::Secondary, SPR_G2_SIDEWAYS_TAB), // tab 03
    MakeRemapWidget({ 3, TabsStart + (TabHeight * 3) }, { TabWidth, TabHeight}, WindowWidgetType::Tab, WindowColour::Secondary, SPR_G2_SIDEWAYS_TAB), // tab 04
    MakeRemapWidget({ 3, TabsStart + (TabHeight * 4) }, { TabWidth, TabHeight}, WindowWidgetType::Tab, WindowColour::Secondary, SPR_G2_SIDEWAYS_TAB), // tab 05
    MakeRemapWidget({ 3, TabsStart + (TabHeight * 5) }, { TabWidth, TabHeight}, WindowWidgetType::Tab, WindowColour::Secondary, SPR_G2_SIDEWAYS_TAB), // tab 06
    MakeRemapWidget({ 3, TabsStart + (TabHeight * 6) }, { TabWidth, TabHeight}, WindowWidgetType::Tab, WindowColour::Secondary, SPR_G2_SIDEWAYS_TAB), // tab 07
    MakeRemapWidget({ 3, TabsStart + (TabHeight * 7) }, { TabWidth, TabHeight}, WindowWidgetType::Tab, WindowColour::Secondary, SPR_G2_SIDEWAYS_TAB), // tab 08
    MakeRemapWidget({ 3, TabsStart + (TabHeight * 8) }, { TabWidth, TabHeight}, WindowWidgetType::Tab, WindowColour::Secondary, SPR_G2_SIDEWAYS_TAB), // tab 09
    MakeRemapWidget({ 3, TabsStart + (TabHeight * 8) }, { TabWidth, TabHeight}, WindowWidgetType::Tab, WindowColour::Secondary, SPR_G2_SIDEWAYS_TAB), // tab 10
    MakeWidget({ TabWidth + 3, WidgetsStart + 1 }, { WW - SidebarWidth, 362 }, WindowWidgetType::Scroll, WindowColour::Secondary, SCROLL_VERTICAL), // level list
    WIDGETS_END,
};
// clang-format on

class ScenarioSelectWindow final : public Window
{
private:
    bool _showLockedInformation = false;
    std::function<void(std::string_view)> _callback;
    std::vector<ScenarioListItem> _listItems;
    const ScenarioIndexEntry* _highlightedScenario = nullptr;

public:
    ScenarioSelectWindow(std::function<void(std::string_view)> callback)
        : _callback(callback)
    {
    }

    void OnOpen() override
    {
        // Load scenario list
        ScenarioRepositoryScan();

        widgets = _scenarioSelectWidgets;
        _highlightedScenario = nullptr;
        InitTabs();
        InitialiseListItems();
        InitScrollWidgets();
    }

    void OnMouseUp(WidgetIndex widgetIndex) override
    {
        if (widgetIndex == WIDX_CLOSE)
        {
            Close();
        }
    }

    void OnMouseDown(WidgetIndex widgetIndex) override
    {
        if (widgetIndex >= WIDX_TAB1 && widgetIndex <= WIDX_TAB10)
        {
            selected_tab = widgetIndex - 4;
            _highlightedScenario = nullptr;
            gConfigInterface.ScenarioselectLastTab = selected_tab;
            ConfigSaveDefault();
            InitialiseListItems();
            Invalidate();
            OnResize();
            OnPrepareDraw();
            InitScrollWidgets();
            Invalidate();
        }
    }

    void OnDraw(DrawPixelInfo& dpi) override
    {
        int32_t format;
        const ScenarioIndexEntry* scenario;

        DrawWidgets(dpi);

        format = ScenarioSelectUseSmallFont() ? STR_SMALL_WINDOW_COLOUR_2_STRINGID : STR_WINDOW_COLOUR_2_STRINGID;
        FontStyle fontStyle = ScenarioSelectUseSmallFont() ? FontStyle::Small : FontStyle::Medium;

        // Text for each tab
        for (uint32_t i = 0; i < std::size(kScenarioOriginStringIds); i++)
        {
            const Widget& widget = widgets[WIDX_TAB1 + i];
            if (widget.type == WindowWidgetType::Empty)
                continue;

            auto ft = Formatter();
            if (gConfigGeneral.ScenarioSelectMode == SCENARIO_SELECT_MODE_ORIGIN)
            {
                ft.Add<StringId>(kScenarioOriginStringIds[i]);
            }
            else
            { // old-style
                ft.Add<StringId>(ScenarioCategoryStringIds[i]);
            }

            auto stringCoords = windowPos + ScreenCoordsXY{ widget.midX(), widget.midY() - 3 };
            DrawTextWrapped(dpi, stringCoords, 87, format, ft, { COLOUR_AQUAMARINE, fontStyle, TextAlignment::CENTRE });
        }

        // Return if no scenario highlighted
        scenario = _highlightedScenario;
        if (scenario == nullptr)
        {
            if (_showLockedInformation)
            {
                // Show locked information
                auto screenPos = windowPos
                    + ScreenCoordsXY{ widgets[WIDX_SCENARIOLIST].right + 4, widgets[WIDX_TABCONTENT].top + 5 };
                DrawTextEllipsised(
                    dpi, screenPos + ScreenCoordsXY{ 85, 0 }, 170, STR_SCENARIO_LOCKED, {}, { TextAlignment::CENTRE });

                DrawTextWrapped(dpi, screenPos + ScreenCoordsXY{ 0, 15 }, 170, STR_SCENARIO_LOCKED_DESC);
            }
            else
            {
                // Show general information about how to start.
                auto screenPos = windowPos
                    + ScreenCoordsXY{ widgets[WIDX_SCENARIOLIST].right + 4, widgets[WIDX_TABCONTENT].top + 5 };

                DrawTextWrapped(dpi, screenPos + ScreenCoordsXY{ 0, 15 }, 170, STR_SCENARIO_HOVER_HINT);
            }
            return;
        }

        // Scenario path
        if (gConfigGeneral.DebuggingTools)
        {
            utf8 path[MAX_PATH];

            ShortenPath(path, sizeof(path), scenario->Path, width - 6 - TabWidth, FontStyle::Medium);

            const utf8* pathPtr = path;
            auto ft = Formatter();
            ft.Add<const char*>(pathPtr);
            DrawTextBasic(dpi, windowPos + ScreenCoordsXY{ TabWidth + 3, height - 3 - 11 }, STR_STRING, ft, { colours[1] });
        }

        // Scenario name
        auto screenPos = windowPos + ScreenCoordsXY{ widgets[WIDX_SCENARIOLIST].right + 4, widgets[WIDX_TABCONTENT].top + 5 };
        auto ft = Formatter();
        ft.Add<StringId>(STR_STRING);
        ft.Add<const char*>(scenario->Name);
        DrawTextEllipsised(
            dpi, screenPos + ScreenCoordsXY{ 85, 0 }, 170, STR_WINDOW_COLOUR_2_STRINGID, ft, { TextAlignment::CENTRE });
        screenPos.y += 15;

        // Scenario details
        ft = Formatter();
        ft.Add<StringId>(STR_STRING);
        ft.Add<const char*>(scenario->Details);
        screenPos.y += DrawTextWrapped(dpi, screenPos, 170, STR_BLACK_STRING, ft) + 5;

        // Scenario objective
        ft = Formatter();
        ft.Add<StringId>(ObjectiveNames[scenario->ObjectiveType]);
        if (scenario->ObjectiveType == OBJECTIVE_BUILD_THE_BEST)
        {
            StringId rideTypeString = STR_NONE;
            auto rideTypeId = scenario->ObjectiveArg3;
            if (rideTypeId != RIDE_TYPE_NULL && rideTypeId < RIDE_TYPE_COUNT)
            {
                rideTypeString = GetRideTypeDescriptor(rideTypeId).Naming.Name;
            }
            ft.Add<StringId>(rideTypeString);
        }
        else
        {
            ft.Add<int16_t>(scenario->ObjectiveArg3);
            ft.Add<int16_t>(DateGetTotalMonths(MONTH_OCTOBER, scenario->ObjectiveArg1));
            if (scenario->ObjectiveType == OBJECTIVE_FINISH_5_ROLLERCOASTERS)
                ft.Add<uint16_t>(scenario->ObjectiveArg2);
            else
                ft.Add<money64>(scenario->ObjectiveArg2);
        }
        screenPos.y += DrawTextWrapped(dpi, screenPos, 170, STR_OBJECTIVE, ft) + 5;

        // Scenario score
        if (scenario->Highscore != nullptr)
        {
            // TODO: Should probably be translatable
            u8string completedByName = "???";
            if (!scenario->Highscore->name.empty())
            {
                completedByName = scenario->Highscore->name;
            }
            ft = Formatter();
            ft.Add<StringId>(STR_STRING);
            ft.Add<const char*>(completedByName.c_str());
            ft.Add<money64>(scenario->Highscore->company_value);
            screenPos.y += DrawTextWrapped(dpi, screenPos, 170, STR_COMPLETED_BY_WITH_COMPANY_VALUE, ft);
        }
    }

    void OnPrepareDraw() override
    {
        pressed_widgets &= ~(
            (1uLL << WIDX_CLOSE) | (1uLL << WIDX_TAB1) | (1uLL << WIDX_TAB2) | (1uLL << WIDX_TAB3) | (1uLL << WIDX_TAB4)
            | (1uLL << WIDX_TAB5) | (1uLL << WIDX_TAB6) | (1uLL << WIDX_TAB7) | (1uLL << WIDX_TAB8) | (1uLL << WIDX_TAB9)
            | (1uLL << WIDX_TAB10));

        pressed_widgets |= 1LL << (selected_tab + WIDX_TAB1);

        ResizeFrameWithPage();
        const int32_t bottomMargin = gConfigGeneral.DebuggingTools ? 17 : 5;
        widgets[WIDX_SCENARIOLIST].right = width - 179;
        widgets[WIDX_SCENARIOLIST].bottom = height - bottomMargin;
    }

    ScreenSize OnScrollGetSize(int32_t scrollIndex) override
    {
        const int32_t scenarioItemHeight = GetScenarioListItemSize();

        int32_t y = 0;
        for (const auto& listItem : _listItems)
        {
            switch (listItem.type)
            {
                case ListItemType::Heading:
                    y += 18;
                    break;
                case ListItemType::Scenario:
                    y += scenarioItemHeight;
                    break;
            }
        }

        return { WW, y };
    }

    void OnScrollMouseOver(int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
    {
        const int32_t scenarioItemHeight = GetScenarioListItemSize();

        bool originalShowLockedInformation = _showLockedInformation;
        _showLockedInformation = false;
        const ScenarioIndexEntry* selected = nullptr;
        auto mutableScreenCoords = screenCoords;
        for (const auto& listItem : _listItems)
        {
            switch (listItem.type)
            {
                case ListItemType::Heading:
                    mutableScreenCoords.y -= 18;
                    break;
                case ListItemType::Scenario:
                    mutableScreenCoords.y -= scenarioItemHeight;
                    if (mutableScreenCoords.y < 0)
                    {
                        if (listItem.scenario.is_locked)
                        {
                            _showLockedInformation = true;
                        }
                        else
                        {
                            selected = listItem.scenario.scenario;
                        }
                    }
                    break;
            }
            if (mutableScreenCoords.y < 0)
            {
                break;
            }
        }

        if (_highlightedScenario != selected)
        {
            _highlightedScenario = selected;
            Invalidate();
        }
        else if (_showLockedInformation != originalShowLockedInformation)
        {
            Invalidate();
        }
    }

    void OnScrollMouseDown(int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
    {
        const int32_t scenarioItemHeight = GetScenarioListItemSize();

        auto mutableScreenCoords = screenCoords;
        for (const auto& listItem : _listItems)
        {
            switch (listItem.type)
            {
                case ListItemType::Heading:
                    mutableScreenCoords.y -= 18;
                    break;
                case ListItemType::Scenario:
                    mutableScreenCoords.y -= scenarioItemHeight;
                    if (mutableScreenCoords.y < 0 && !listItem.scenario.is_locked)
                    {
                        OpenRCT2::Audio::Play(OpenRCT2::Audio::SoundId::Click1, 0, windowPos.x + (width / 2));
                        gFirstTimeSaving = true;
                        // Callback will likely close this window! So should always return after it.
                        _callback(listItem.scenario.scenario->Path);
                        return;
                    }
                    break;
            }
            if (mutableScreenCoords.y < 0)
            {
                break;
            }
        }
    }

    void OnScrollDraw(int32_t scrollIndex, DrawPixelInfo& dpi) override
    {
        uint8_t paletteIndex = ColourMapA[colours[1]].mid_light;
        GfxClear(&dpi, paletteIndex);

        StringId highlighted_format = ScenarioSelectUseSmallFont() ? STR_WHITE_STRING : STR_WINDOW_COLOUR_2_STRINGID;
        StringId unhighlighted_format = ScenarioSelectUseSmallFont() ? STR_WHITE_STRING : STR_BLACK_STRING;

        const auto& listWidget = widgets[WIDX_SCENARIOLIST];
        int32_t listWidth = listWidget.width() - 12;

        const int32_t scenarioItemHeight = GetScenarioListItemSize();

        // Scenario title
        int32_t scenarioTitleHeight = FontGetLineHeight(FontStyle::Medium);

        int32_t y = 0;
        for (const auto& listItem : _listItems)
        {
            if (y > dpi.y + dpi.height)
            {
                continue;
            }

            switch (listItem.type)
            {
                case ListItemType::Heading:
                {
                    const int32_t horizontalRuleMargin = 4;
                    DrawCategoryHeading(
                        dpi, horizontalRuleMargin, listWidth - horizontalRuleMargin, y + 2, listItem.heading.string_id);
                    y += 18;
                    break;
                }
                case ListItemType::Scenario:
                {
                    // Draw hover highlight
                    const ScenarioIndexEntry* scenario = listItem.scenario.scenario;
                    bool isHighlighted = _highlightedScenario == scenario;
                    if (isHighlighted)
                    {
                        GfxFilterRect(dpi, { 0, y, width, y + scenarioItemHeight - 1 }, FilterPaletteID::PaletteDarken1);
                    }

                    bool isCompleted = scenario->Highscore != nullptr;
                    bool isDisabled = listItem.scenario.is_locked;

                    // Draw scenario name
                    char buffer[64];
                    SafeStrCpy(buffer, scenario->Name, sizeof(buffer));
                    StringId format = isDisabled ? static_cast<StringId>(STR_STRINGID)
                                                 : (isHighlighted ? highlighted_format : unhighlighted_format);
                    auto ft = Formatter();
                    ft.Add<StringId>(STR_STRING);
                    ft.Add<char*>(buffer);
                    colour_t colour = isDisabled ? colours[1] | COLOUR_FLAG_INSET : COLOUR_BLACK;
                    auto darkness = isDisabled ? TextDarkness::Dark : TextDarkness::Regular;
                    const auto scrollCentre = widgets[WIDX_SCENARIOLIST].width() / 2;

                    DrawTextBasic(
                        dpi, { scrollCentre, y + 1 }, format, ft,
                        { colour, FontStyle::Medium, TextAlignment::CENTRE, darkness });

                    // Check if scenario is completed
                    if (isCompleted)
                    {
                        // Draw completion tick
                        GfxDrawSprite(dpi, ImageId(SPR_MENU_CHECKMARK), { widgets[WIDX_SCENARIOLIST].width() - 45, y + 1 });

                        // Draw completion score
                        u8string completedByName = "???";
                        if (!scenario->Highscore->name.empty())
                        {
                            completedByName = scenario->Highscore->name;
                        }
                        ft = Formatter();
                        ft.Add<StringId>(STR_COMPLETED_BY);
                        ft.Add<StringId>(STR_STRING);
                        ft.Add<const char*>(completedByName.c_str());
                        DrawTextBasic(
                            dpi, { scrollCentre, y + scenarioTitleHeight + 1 }, format, ft,
                            { FontStyle::Small, TextAlignment::CENTRE });
                    }

                    y += scenarioItemHeight;
                    break;
                }
            }
        }
    }

private:
    void DrawCategoryHeading(DrawPixelInfo& dpi, int32_t left, int32_t right, int32_t y, StringId stringId) const
    {
        colour_t baseColour = colours[1];
        colour_t lightColour = ColourMapA[baseColour].lighter;
        colour_t darkColour = ColourMapA[baseColour].mid_dark;

        // Draw string
        int32_t centreX = (left + right) / 2;
        DrawTextBasic(dpi, { centreX, y }, stringId, {}, { baseColour, TextAlignment::CENTRE });

        // Get string dimensions
        utf8 buffer[CommonTextBufferSize];
        auto bufferPtr = buffer;
        OpenRCT2::FormatStringLegacy(bufferPtr, sizeof(buffer), stringId, nullptr);
        int32_t categoryStringHalfWidth = (GfxGetStringWidth(bufferPtr, FontStyle::Medium) / 2) + 4;
        int32_t strLeft = centreX - categoryStringHalfWidth;
        int32_t strRight = centreX + categoryStringHalfWidth;

        // Draw light horizontal rule
        int32_t lineY = y + 4;
        auto lightLineLeftTop1 = ScreenCoordsXY{ left, lineY };
        auto lightLineRightBottom1 = ScreenCoordsXY{ strLeft, lineY };
        GfxDrawLine(dpi, { lightLineLeftTop1, lightLineRightBottom1 }, lightColour);

        auto lightLineLeftTop2 = ScreenCoordsXY{ strRight, lineY };
        auto lightLineRightBottom2 = ScreenCoordsXY{ right, lineY };
        GfxDrawLine(dpi, { lightLineLeftTop2, lightLineRightBottom2 }, lightColour);

        // Draw dark horizontal rule
        lineY++;
        auto darkLineLeftTop1 = ScreenCoordsXY{ left, lineY };
        auto darkLineRightBottom1 = ScreenCoordsXY{ strLeft, lineY };
        GfxDrawLine(dpi, { darkLineLeftTop1, darkLineRightBottom1 }, darkColour);

        auto darkLineLeftTop2 = ScreenCoordsXY{ strRight, lineY };
        auto darkLineRightBottom2 = ScreenCoordsXY{ right, lineY };
        GfxDrawLine(dpi, { darkLineLeftTop2, darkLineRightBottom2 }, darkColour);
    }

    void InitialiseListItems()
    {
        size_t numScenarios = ScenarioRepositoryGetCount();
        _listItems.clear();

        // Mega park unlock
        const uint32_t rct1RequiredCompletedScenarios = (1 << SC_MEGA_PARK) - 1;
        uint32_t rct1CompletedScenarios = 0;
        std::optional<size_t> megaParkListItemIndex = std::nullopt;

        int32_t numUnlocks = InitialNumUnlockedScenarios;
        uint8_t currentHeading = UINT8_MAX;
        for (size_t i = 0; i < numScenarios; i++)
        {
            const ScenarioIndexEntry* scenario = ScenarioRepositoryGetByIndex(i);

            if (!IsScenarioVisible(*scenario))
                continue;

            // Category heading
            StringId headingStringId = STR_NONE;
            if (gConfigGeneral.ScenarioSelectMode == SCENARIO_SELECT_MODE_ORIGIN)
            {
                if (selected_tab != static_cast<uint8_t>(ScenarioSource::Real) && currentHeading != scenario->Category)
                {
                    currentHeading = scenario->Category;
                    headingStringId = ScenarioCategoryStringIds[currentHeading];
                }
            }
            else
            {
                if (selected_tab <= SCENARIO_CATEGORY_EXPERT)
                {
                    if (currentHeading != static_cast<uint8_t>(scenario->SourceGame))
                    {
                        currentHeading = static_cast<uint8_t>(scenario->SourceGame);
                        headingStringId = kScenarioOriginStringIds[currentHeading];
                    }
                }
                else if (selected_tab == SCENARIO_CATEGORY_OTHER)
                {
                    int32_t category = scenario->Category;
                    if (category <= SCENARIO_CATEGORY_REAL)
                    {
                        category = SCENARIO_CATEGORY_OTHER;
                    }
                    if (currentHeading != category)
                    {
                        currentHeading = category;
                        headingStringId = ScenarioCategoryStringIds[category];
                    }
                }
            }

            if (headingStringId != STR_NONE)
            {
                ScenarioListItem headerItem;
                headerItem.type = ListItemType::Heading;
                headerItem.heading.string_id = headingStringId;
                _listItems.push_back(std::move(headerItem));
            }

            // Scenario
            ScenarioListItem scenarioItem;
            scenarioItem.type = ListItemType::Scenario;
            scenarioItem.scenario.scenario = scenario;
            if (IsLockingEnabled())
            {
                scenarioItem.scenario.is_locked = numUnlocks <= 0;
                if (scenario->Highscore == nullptr)
                {
                    numUnlocks--;
                }
                else
                {
                    // Mark RCT1 scenario as completed
                    if (scenario->ScenarioId < SC_MEGA_PARK)
                    {
                        rct1CompletedScenarios |= 1 << scenario->ScenarioId;
                    }
                }

                // If scenario is Mega Park, keep a reference to it
                if (scenario->ScenarioId == SC_MEGA_PARK)
                {
                    megaParkListItemIndex = _listItems.size() - 1;
                }
            }
            else
            {
                scenarioItem.scenario.is_locked = false;
            }
            _listItems.push_back(std::move(scenarioItem));
        }

        // Mega park handling
        if (megaParkListItemIndex.has_value())
        {
            bool megaParkLocked = (rct1CompletedScenarios & rct1RequiredCompletedScenarios) != rct1RequiredCompletedScenarios;
            _listItems[megaParkListItemIndex.value()].scenario.is_locked = megaParkLocked;
            if (megaParkLocked && gConfigGeneral.ScenarioHideMegaPark)
            {
                // Remove mega park
                _listItems.pop_back();

                // Remove empty headings
                for (auto it = _listItems.begin(); it != _listItems.end();)
                {
                    const auto& listItem = *it;
                    if (listItem.type == ListItemType::Heading)
                    {
                        auto nextIt = std::next(it);
                        if (nextIt == _listItems.end() || nextIt->type == ListItemType::Heading)
                        {
                            it = _listItems.erase(it);
                            continue;
                        }
                    }
                    ++it;
                }
            }
        }
    }

    bool IsScenarioVisible(const ScenarioIndexEntry& scenario) const
    {
        if (gConfigGeneral.ScenarioSelectMode == SCENARIO_SELECT_MODE_ORIGIN)
        {
            if (static_cast<uint8_t>(scenario.SourceGame) != selected_tab)
            {
                return false;
            }
        }
        else
        {
            int32_t category = scenario.Category;
            if (category > SCENARIO_CATEGORY_OTHER)
            {
                category = SCENARIO_CATEGORY_OTHER;
            }
            if (category != selected_tab)
            {
                return false;
            }
        }
        return true;
    }

    bool IsLockingEnabled() const
    {
        if (gConfigGeneral.ScenarioSelectMode != SCENARIO_SELECT_MODE_ORIGIN)
            return false;
        if (!gConfigGeneral.ScenarioUnlockingEnabled)
            return false;
        if (selected_tab >= 6)
            return false;

        return true;
    }

    void InitTabs()
    {
        int32_t showPages = 0;
        size_t numScenarios = ScenarioRepositoryGetCount();
        for (size_t i = 0; i < numScenarios; i++)
        {
            const ScenarioIndexEntry* scenario = ScenarioRepositoryGetByIndex(i);
            if (gConfigGeneral.ScenarioSelectMode == SCENARIO_SELECT_MODE_ORIGIN)
            {
                showPages |= 1 << static_cast<uint8_t>(scenario->SourceGame);
            }
            else
            {
                int32_t category = scenario->Category;
                if (category > SCENARIO_CATEGORY_OTHER)
                {
                    category = SCENARIO_CATEGORY_OTHER;
                }
                showPages |= 1 << category;
            }
        }

        if (showPages & (1 << gConfigInterface.ScenarioselectLastTab))
        {
            selected_tab = gConfigInterface.ScenarioselectLastTab;
        }
        else
        {
            int32_t firstPage = UtilBitScanForward(showPages);
            if (firstPage != -1)
            {
                selected_tab = firstPage;
            }
        }

        int32_t y = TabsStart;
        for (int32_t i = 0; i < NumTabs; i++)
        {
            auto& widget = widgets[i + WIDX_TAB1];
            if (!(showPages & (1 << i)))
            {
                widget.type = WindowWidgetType::Empty;
                continue;
            }

            widget.type = WindowWidgetType::Tab;
            widget.top = y;
            widget.bottom = y + (TabHeight - 1);
            y += TabHeight;
        }
    }

    static bool ScenarioSelectUseSmallFont()
    {
        return ThemeGetFlags() & UITHEME_FLAG_USE_ALTERNATIVE_SCENARIO_SELECT_FONT;
    }

    static int32_t GetScenarioListItemSize()
    {
        if (!LocalisationService_UseTrueTypeFont())
            return TrueFontSize;

        // Scenario title
        int32_t lineHeight = FontGetLineHeight(FontStyle::Medium);

        // 'Completed by' line
        lineHeight += FontGetLineHeight(FontStyle::Small);

        return lineHeight;
    }
};

WindowBase* WindowScenarioselectOpen(scenarioselect_callback callback)
{
    return WindowScenarioselectOpen([callback](std::string_view scenario) { callback(std::string(scenario).c_str()); });
}

WindowBase* WindowScenarioselectOpen(std::function<void(std::string_view)> callback)
{
    auto* window = static_cast<ScenarioSelectWindow*>(WindowBringToFrontByClass(WindowClass::ScenarioSelect));
    if (window != nullptr)
    {
        return window;
    }

    int32_t screenWidth = ContextGetWidth();
    int32_t screenHeight = ContextGetHeight();
    ScreenCoordsXY screenPos = { (screenWidth - WW) / 2, std::max(TOP_TOOLBAR_HEIGHT + 1, (screenHeight - WH) / 2) };
    window = WindowCreate<ScenarioSelectWindow>(WindowClass::ScenarioSelect, screenPos, WW, WH, 0, callback);
    return window;
}
