// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov

/** @file MenuStyle.h
 *  @brief Self-contained menu appearance for the Wt application.
 */
#pragma once

#include <Wt/WCssStyleSheet.h>

namespace itsv {

// These rules are sent with the application, without an external theme or
// stylesheet dependency. Popup menus are global Wt widgets, so style the class
// on each popup itself rather than relying on a parent menu-bar selector.
/**
 * @brief Install self-contained CSS for ITSV menu bar and popup menus.
 * @param sheet Wt stylesheet to populate.
 */
inline void applyMenuStyle(Wt::WCssStyleSheet& sheet)
{
  sheet.addRule(".itsv-menu-bar",
    "box-sizing:border-box;background:linear-gradient(#fafbfc,#eef1f4);"
    "border-bottom:1px solid #bcc4ce;color:#20252b;"
    "font:var(--itsv-menu-font-size,14px)/1.4 system-ui,-apple-system,Segoe UI,Arial,sans-serif;");

  sheet.addRule(".itsv-menu-bar button.itsv-menu-button",
    "box-sizing:border-box;appearance:none;-webkit-appearance:none;"
    "margin:0;padding:3px 12px;border:1px solid transparent;border-radius:3px;"
    "background:transparent;color:inherit;font:inherit;line-height:21px;"
    "white-space:nowrap;box-shadow:none;text-shadow:none;cursor:default;"
    "user-select:none;");

  sheet.addRule(".itsv-menu-bar button.itsv-menu-button:hover",
    "background:#e5edf8;border-color:#bdcde2;");
  sheet.addRule(".itsv-menu-bar button.itsv-menu-button.active,"
                ".itsv-menu-bar button.itsv-menu-button:active",
    "background:#d9e7f8;border-color:#a8bfdc;color:#142e4d;");
  sheet.addRule(".itsv-menu-bar button.itsv-menu-button:focus-visible",
    "outline:2px solid #2864ad;outline-offset:1px;");
  sheet.addRule(".dv-status",
    "font:var(--itsv-menu-font-size,14px)/1.4 system-ui,-apple-system,Segoe UI,Arial,sans-serif;");

  sheet.addRule(".itsv-menu-bar button.itsv-menu-button:disabled,"
                ".itsv-menu-bar button.itsv-menu-button.Wt-disabled",
    "background:transparent;border-color:transparent;color:#939ba6;"
    "cursor:default;");

  // Leave positioning, visibility, z-index and dismissal under Wt's control.
  sheet.addRule("ul.itsv-popup-menu",
    "box-sizing:border-box;margin:0;padding:5px;"
    "min-width:min(220px,calc(100vw - 16px));max-width:calc(100vw - 16px);"
    "max-height:calc(100vh - 60px);overflow-x:hidden;overflow-y:auto;"
    "list-style:none;border:1px solid #aeb8c5;border-radius:5px;"
    "background:#fff;color:#20252b;box-shadow:0 6px 18px rgba(0,0,0,.18);"
    "font:var(--itsv-menu-font-size,14px)/1.4 system-ui,-apple-system,Segoe UI,Arial,sans-serif;"
    "text-align:left;cursor:default;user-select:none;");

  sheet.addRule("ul.itsv-popup-menu > li",
    "box-sizing:border-box;list-style:none;margin:0;padding:0;border:0;"
    "background-color:transparent;color:inherit;");
  sheet.addRule("ul.itsv-popup-menu > li > a,"
                "ul.itsv-popup-menu > li > a:link,"
                "ul.itsv-popup-menu > li > a:visited",
    "display:block;box-sizing:border-box;min-height:30px;padding:5px 13px;"
    "border-radius:3px;background-color:transparent;color:inherit;"
    "font:inherit;line-height:19px;text-decoration:none;white-space:nowrap;"
    "cursor:default;");
  sheet.addRule("ul.itsv-popup-menu > li > a *",
    "color:inherit;text-decoration:none;cursor:inherit;");

  // Wt uses .active on the hovered menu item and on the open title button.
  sheet.addRule("ul.itsv-popup-menu > li.active,"
                "ul.itsv-popup-menu > li:not(.Wt-disabled):not(.Wt-separator):not(.Wt-sectheader):hover",
    "border-radius:3px;background-color:#2864ad;color:#fff;");
  sheet.addRule("ul.itsv-popup-menu > li > a:focus-visible",
    "background-color:#2864ad;color:#fff;outline:2px solid #16457f;"
    "outline-offset:-2px;");

  sheet.addRule("ul.itsv-popup-menu > li.Wt-separator",
    "height:1px;min-height:0;margin:5px 7px;padding:0;"
    "border:0;border-top:1px solid #d7dce4;background-color:transparent;"
    "pointer-events:none;");
  sheet.addRule("ul.itsv-popup-menu > li.Wt-sectheader,"
                "ul.itsv-popup-menu > li.Wt-sectheader.active",
    "padding:5px 13px;background-color:transparent;color:#687381;"
    "font-weight:600;cursor:default;");
  sheet.addRule("ul.itsv-popup-menu > li.Wt-disabled,"
                "ul.itsv-popup-menu > li.Wt-disabled.active,"
                "ul.itsv-popup-menu > li.Wt-disabled > a,"
                "ul.itsv-popup-menu > li > a.Wt-disabled",
    "background-color:transparent;color:#939ba6;cursor:default;");
}

} // namespace itsv
