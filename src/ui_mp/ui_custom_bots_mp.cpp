#ifdef KISAK_MP
#include "ui_main_mp.h"
#include <ui/ui_shared.h>
#include <ui/ui_utils.h>
#include <ui/ui_feeders.h>
#include <ui/ui_main.h>
#include <qcommon/common.h>
#include <database/db_registry.h>
#include <cmath>
#include <list>
#include <vector>

namespace
{
    constexpr int panelLayers = 6;
    constexpr int layersPerRow = 5;

    struct BotRow
    {
        itemDef_s layers[layersPerRow]{};
        textDef_s labels[2]{};
        textDef_s valueText{};
        focusItemDef_s valueFocus{};
    };

    menuDef_t botMenu;
    itemDef_s botPanel[panelLayers];
    itemDef_s botTitle, botHint;
    textDef_s titleText, hintText;
    BotRow botRows[3];
    itemDef_s *botItemPointers[panelLayers + 2 + ARRAY_COUNT(botRows) * layersPerRow];
    multiDef_s botCount, botDifficulty, friendlyCount, enemyCount;
    char numberLabels[18][3];
    GenericEventScript openScript, closeScript, escapeScript, focusScript, changeScript;
    GenericEventHandler openEvent, closeEvent, escapeEvent, focusEvent, changeEvent;

    struct LobbyButton
    {
        itemDef_s item{};
        textDef_s text{};
        focusItemDef_s focus{};
        GenericEventScript script{};
        GenericEventHandler event{};
        // The row looks like the lobby's own: the Server Settings row's other
        // items (backing, highlight box, ...) copied to this row, the highlight
        // ones shown while this button has the focus (UI_IsHiddenBotSettingsHighlight).
        // onFocus takes the lobby's highlight number away from the other rows,
        // and leaveFocus is the Server Settings row's.
        GenericEventScript focusScript{};
        GenericEventHandler focusEvent{};
        GenericEventHandler leaveEvent{};
        std::list<itemDef_s> copies;
        std::list<textDef_s> copyTexts;
        std::vector<itemDef_s *> items;
    };
    // Asset-owned menus retain these pointers across UI restarts. Base and
    // patch assets need separate stable storage, released at process shutdown.
    std::list<LobbyButton> lobbyButtons;

    bool HasFocusEvent(const itemDef_s *item)   // as Item_HasFocusEvent
    {
        for (const GenericEventHandler *event = item->onEvent; event; event = event->next)
            if (!I_stricmp(event->name, "onfocus") || !I_stricmp(event->name, "leavefocus"))
                return true;
        return false;
    }

    void InitBotMenu()
    {
        if (botMenu.window.name)
            return;
        botMenu.window.name = "openblops_bot_settings";
        openScript.action = "activateBlur ; play uin_navigation_menu_lg_open ; setfocus bots_count ;";
        closeScript.action = "deactivateBlur ;";
        escapeScript.action = "play uin_navigation_menu_lg_close ; close self ;";
        focusScript.action = "play uin_navigation_over ;";
        changeScript.action = "play uin_mouse_click_t ;";
        openEvent.name = "onOpen";
        openEvent.eventScript = &openScript;
        openEvent.next = &escapeEvent;
        escapeEvent.name = "onESC";
        escapeEvent.eventScript = &escapeScript;
        escapeEvent.next = &closeEvent;
        closeEvent.name = "onClose";
        closeEvent.eventScript = &closeScript;
        focusEvent.name = "onFocus";
        focusEvent.eventScript = &focusScript;
        focusEvent.next = &changeEvent;
        changeEvent.name = "action";
        changeEvent.eventScript = &changeScript;

        botCount.count = ARRAY_COUNT(numberLabels);
        friendlyCount.count = 9;
        enemyCount.count = 10;
        for (int i = 0; i < ARRAY_COUNT(numberLabels); ++i)
        {
            Com_sprintf(numberLabels[i], sizeof(numberLabels[i]), "%d", i);
            botCount.dvarList[i] = friendlyCount.dvarList[i] = enemyCount.dvarList[i] = numberLabels[i];
            botCount.dvarValue[i] = friendlyCount.dvarValue[i] = enemyCount.dvarValue[i] = static_cast<float>(i);
        }
        botCount.dvarList[0] = "@MENU_OFF_CAPS";

        botDifficulty.count = 4;
        botDifficulty.strDef = 1;
        const char *values[] = {"easy", "normal", "hard", "fu"};
        const char *labels[] = {"@MENU_RECRUIT_CAPS", "@MENU_REGULAR_CAPS", "@MENU_HARDENED_CAPS", "@MENU_VETERAN_CAPS"};
        for (int i = 0; i < botDifficulty.count; ++i)
        {
            botDifficulty.dvarStr[i] = values[i];
            botDifficulty.dvarList[i] = labels[i];
        }
    }

    void CopyBotItem(itemDef_s &item, const itemDef_s *source)
    {
        item = *source;
        item.parent = &botMenu;
        item.window.rect = item.window.rectClient;
        item.window.dynamicFlags[0] = (source->window.dynamicFlags[0] & 0x10000) | 4;
        item.onEvent = nullptr;
        item.animInfo = nullptr;
        item.ui3dWindowId = -1;
    }

    void UngateBotItem(itemDef_s &item)
    {
        item.visibleExp = {};
        item.forecolorAExp = {};
        item.showBits = 0;
        item.hideBits = ~0ULL;
        item.dvar = item.dvarTest = item.enableDvar = nullptr;
        item.dvarFlags = 0;
        item.rectExpData = nullptr;
    }

    void SetBotText(itemDef_s &item, textDef_s &text, const char *label)
    {
        text = *item.typeData.textDef;
        memset(text.textRect, 0, sizeof(text.textRect));
        text.text = label;
        text.textExpData = nullptr;
        text.textTypeData.focusItemDef = nullptr;
        item.typeData.textDef = &text;
    }

    void AddBotRow(BotRow &row, itemDef_s *const *templates, float y, const char *name,
        const char *label, const char *dvar, multiDef_s *values)
    {
        for (int i = 0; i < layersPerRow; ++i)
        {
            itemDef_s &part = row.layers[i];
            CopyBotItem(part, templates[i]);
            UngateBotItem(part);
            part.window.name = nullptr;
            part.window.rect.y = part.window.rectClient.y = y;
            if (i < layersPerRow - 1)
                part.window.staticFlags |= 0x100000;
            botItemPointers[botMenu.itemCount++] = &part;
        }
        SetBotText(row.layers[2], row.labels[0], label);
        SetBotText(row.layers[3], row.labels[1], label);

        itemDef_s &choice = row.layers[layersPerRow - 1];
        SetBotText(choice, row.valueText, nullptr);
        const float labelWidth = row.valueText.textalignx - 18.0f;
        for (int i = 2; i < layersPerRow - 1; ++i)
            row.layers[i].window.rect.w = row.layers[i].window.rectClient.w = labelWidth;
        memcpy(choice.window.foreColor, row.layers[2].window.foreColor, sizeof(choice.window.foreColor));
        row.valueFocus = {};
        row.valueFocus.focusTypeData.multi = values;
        row.valueText.textTypeData.focusItemDef = &row.valueFocus;
        choice.window.name = name;
        choice.onEvent = &focusEvent;
        choice.dvar = dvar;
    }
}

bool UI_PrepareCustomBotMenu(UiContext *dc)
{
    constexpr int headingIndex = 7;
    constexpr int firstRowIndex = 8;
    constexpr float rowSpacing = 19.0f;
    const menuDef_t *layout = Menus_FindByName(dc, "basictraining_settings");
    if (!layout)
        layout = DB_FindXAssetHeader(ASSET_TYPE_MENU, (char *)"basictraining_settings", 0, -1).menu;
    if (!layout || layout->itemCount <= firstRowIndex + layersPerRow)
        return false;

    const itemDef_s *heading = layout->items[headingIndex];
    const itemDef_s *hint = layout->items[layout->itemCount - 3];
    itemDef_s *const *row = layout->items + firstRowIndex;
    if (heading->type != 1 || !heading->typeData.textDef || hint->type != 1 || !hint->typeData.textDef
        || row[0]->type != 0 || row[1]->type != 0 || row[2]->type != 1 || row[3]->type != 1 || row[4]->type != 10
        || !row[2]->typeData.textDef || !row[3]->typeData.textDef || !row[4]->typeData.textDef)
        return false;

    const char *gametype = Dvar_GetString("ui_gametype");
    if (!I_strnicmp(gametype, "hc", 2))
        gametype += 2;
    const bool teams = I_stricmp(gametype, "dm") && I_stricmp(gametype, "gun")
        && I_stricmp(gametype, "oic") && I_stricmp(gametype, "hlnd") && I_stricmp(gametype, "shrp");

    const char *name = botMenu.window.name;
    botMenu = *layout;
    botMenu.window.name = name;
    botMenu.window.modal = 1;
    botMenu.window.dynamicFlags[0] = 0;
    botMenu.initialRectInfo = botMenu.window.rect = botMenu.window.rectClient;
    botMenu.ui3dWindowId = -1;
    botMenu.cursorItem[0] = -1;
    botMenu.visibleExp = {};
    botMenu.showBits = 0;
    botMenu.hideBits = ~0ULL;
    botMenu.onEvent = &openEvent;
    botMenu.onKey = nullptr;
    botMenu.itemCount = 0;
    botMenu.items = botItemPointers;

    for (int i = 0; i < panelLayers; ++i)
    {
        CopyBotItem(botPanel[i], layout->items[i]);
        botPanel[i].window.staticFlags |= 0x100000;
        botItemPointers[botMenu.itemCount++] = &botPanel[i];
    }
    CopyBotItem(botTitle, heading);
    UngateBotItem(botTitle);
    botTitle.window.staticFlags |= 0x100000;
    SetBotText(botTitle, titleText, "@OPENBLOPS_BOT_SETTINGS");
    botItemPointers[botMenu.itemCount++] = &botTitle;

    float y = row[4]->window.rectClient.y;
    AddBotRow(botRows[0], row, y, "bots_count",
        teams ? "@OPENBLOPS_BOTS_FRIENDLY" : "@OPENBLOPS_BOTS_COUNT",
        teams ? "scr_num_bots_friendly" : "scr_num_bots", teams ? &friendlyCount : &botCount);
    if (teams)
    {
        y += rowSpacing;
        AddBotRow(botRows[1], row, y, "bots_enemies", "@OPENBLOPS_BOTS_ENEMY", "scr_num_bots_enemy", &enemyCount);
    }
    y += rowSpacing;
    AddBotRow(botRows[2], row, y, "bots_difficulty", "@MENU_BASICTRAINING_DIFFICULTY_CAPS",
        "scr_bot_difficulty", &botDifficulty);

    CopyBotItem(botHint, hint);
    UngateBotItem(botHint);
    botHint.window.rectClient.x = row[4]->window.rectClient.x;
    botHint.window.rectClient.y = y + 2.0f * rowSpacing;
    botHint.window.rect = botHint.window.rectClient;
    botHint.window.staticFlags |= 0x900000;
    SetBotText(botHint, hintText, teams ? "@OPENBLOPS_BOTS_TEAM_HINT" : "@OPENBLOPS_BOTS_SOLO_HINT");
    botItemPointers[botMenu.itemCount++] = &botHint;
    return true;
}

bool UI_IsCustomBotMenu(const menuDef_t *menu)
{
    return menu == &botMenu;
}

// Item_IsVisible (ui_utils.cpp): the BOT SETTINGS row's highlight items show
// only while its button has the focus.
bool UI_IsHiddenBotSettingsHighlight(int contextIndex, const itemDef_s *item)
{
    if (item->parent == &botMenu)
    {
        for (const auto &row : botRows)
            for (int i = 0; i < layersPerRow - 1; ++i)
                if (item == &row.layers[i])
                    return Window_HasFocus(contextIndex, &row.layers[layersPerRow - 1].window) != (i % 2 != 0);
        return false;
    }
    if (!item->window.name || I_stricmp(item->window.name, "openblops_bot_settings_hl") || !item->parent)
        return false;
    const menuDef_t *menu = item->parent;
    for (int i = 0; i < menu->itemCount; ++i)
        if (menu->items[i]->window.name && !I_stricmp(menu->items[i]->window.name, "openblops_bot_settings_button"))
            return !Window_HasFocus(contextIndex, &menu->items[i]->window);
    return true;
}

void UI_AddCustomBotSettings(int localClientNum, UiContext *dc, menuDef_t *menu)
{
    if (I_stricmp(menu->window.name, "menu_xboxlive_privatelobby"))
        return;
    _Dvar_RegisterInt("scr_num_bots", 0, 0, 17, 1u, "Number of bots in custom matches (zero disables bots)");
    _Dvar_RegisterInt("scr_num_bots_friendly", 0, 0, 8, 1u, "Friendly bots in team custom matches");
    _Dvar_RegisterInt("scr_num_bots_enemy", 0, 0, 9, 1u, "Enemy bots in team custom matches");
    if (Menu_FindItemByName(menu, "openblops_bot_settings_button"))
    {
        UI_AddMenu(localClientNum, dc, &botMenu, 1);
        return;
    }
    for (int i = 0; i < menu->itemCount; ++i)
    {
        const itemDef_s *source = menu->items[i];
        if (source->type != 3 || !source->typeData.textDef)
            continue;
        for (GenericEventHandler *event = source->onEvent; event; event = event->next)
        {
            if (I_stricmp(event->name, "action"))
                continue;
            for (GenericEventScript *script = event->eventScript; script; script = script->next)
            {
                if (!script->action || !strstr(script->action, "open server_settings"))
                    continue;
                InitBotMenu();
                lobbyButtons.emplace_back();
                LobbyButton &entry = lobbyButtons.back();
                entry.item = *source;
                entry.item.window.name = "openblops_bot_settings_button";
                // Retail leaves this row between Killstreaks and Playercard.
                entry.item.window.rect.y = entry.item.window.rectClient.y = 174.0f;
                entry.item.rectExpData = nullptr;
                entry.item.animInfo = nullptr;
                const float dy = entry.item.window.rect.y - source->window.rect.y;
                entry.text = *source->typeData.textDef;
                entry.text.text = "@OPENBLOPS_BOT_SETTINGS";
                entry.text.textExpData = nullptr;
                entry.text.textTypeData.focusItemDef = &entry.focus;
                entry.item.typeData.textDef = &entry.text;
                entry.script.action = "play uin_navigation_click ; open openblops_bot_settings ;";
                entry.event.name = "action";
                entry.event.eventScript = &entry.script;
                // No lobby row uses highlight number 0; the other rows' highlights go off.
                entry.focusScript.action = "play uin_navigation_over ; setLocalVarInt ui_highlight 0 ;"
                                           " setLocalVarString ui_choicegroup menu_xboxlive_privatelobby ;"
                                           " execNow set ui_hint_text \"\" ;";
                entry.focusEvent.name = "onFocus";
                entry.focusEvent.eventScript = &entry.focusScript;
                entry.event.next = &entry.focusEvent;
                for (const GenericEventHandler *handler = source->onEvent; handler; handler = handler->next)
                {
                    if (handler->name && !I_stricmp(handler->name, "leavefocus"))
                    {
                        entry.leaveEvent = *handler;
                        entry.leaveEvent.next = nullptr;
                        entry.focusEvent.next = &entry.leaveEvent;
                        break;
                    }
                }
                entry.item.onEvent = &entry.event;
                entry.items.assign(menu->items, menu->items + menu->itemCount);
                // The row's other items: those on the button's rect, in draw order
                // around the button (before it if they were before the source).
                std::vector<itemDef_s *> before, after;
                bool pastSource = false;
                for (int k = 0; k < menu->itemCount; ++k)
                {
                    itemDef_s *other = menu->items[k];
                    if (other == source)
                    {
                        pastSource = true;
                        continue;
                    }
                    if (other->window.rect.x != source->window.rect.x || other->window.rect.y != source->window.rect.y
                        || other->window.rect.w != source->window.rect.w || other->window.rect.h != source->window.rect.h)
                        continue;
                    entry.copies.push_back(*other);
                    itemDef_s &copy = entry.copies.back();
                    const bool highlight = other->visibleExp.numRpn != source->visibleExp.numRpn;
                    copy.window.name = highlight ? "openblops_bot_settings_hl" : "openblops_bot_settings_row";
                    copy.window.rect.y += dy;
                    copy.window.rectClient.y += dy;
                    copy.rectExpData = nullptr;
                    copy.animInfo = nullptr;
                    copy.onEvent = nullptr;
                    copy.parent = menu;
                    if (highlight)
                        copy.visibleExp = source->visibleExp;   // the row's condition; the focus test is in code
                    if (Item_IsTextDefType(other) && other->typeData.textDef)
                    {
                        entry.copyTexts.push_back(*other->typeData.textDef);
                        textDef_s &text = entry.copyTexts.back();
                        text.textRect[0].y += dy;
                        text.text = "@OPENBLOPS_BOT_SETTINGS";
                        text.textExpData = nullptr;
                        copy.typeData.textDef = &text;
                    }
                    (pastSource ? after : before).push_back(&copy);
                }
                // Up/down step through the items in array order
                // (Menu_SetNextCursorItem), so the row goes before the first
                // focusable row below it in this column (Playercard), not last.
                size_t at = entry.items.size();
                for (size_t k = 0; k < entry.items.size(); ++k)
                {
                    const itemDef_s *other = entry.items[k];
                    if (other->window.rect.y <= entry.item.window.rect.y
                        || fabsf(other->window.rect.x - source->window.rect.x) > 1.0f
                        || !HasFocusEvent(other))
                        continue;
                    if (at == entry.items.size() || other->window.rect.y < entry.items[at]->window.rect.y)
                        at = k;
                }
                std::vector<itemDef_s *> row(before);
                row.push_back(&entry.item);
                row.insert(row.end(), after.begin(), after.end());
                entry.items.insert(entry.items.begin() + at, row.begin(), row.end());
                menu->items = entry.items.data();
                menu->itemCount += int(row.size());
                UI_AddMenu(localClientNum, dc, &botMenu, 1);
                return;
            }
        }
    }
    Com_PrintWarning(13, "Custom bots: private lobby has no Server Settings button to extend.\n");
}

#endif
