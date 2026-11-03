// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
#include <Wt/WApplication.h>
#include <Wt/WBorder.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WCssDecorationStyle.h>
#include <Wt/WDialog.h>
#include <Wt/WFileUpload.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WJavaScript.h>
#include <Wt/WMenuItem.h>
#include <Wt/WPopupMenu.h>
#include <Wt/WPushButton.h>
#include <Wt/WSpinBox.h>
#include <Wt/WTable.h>
#include <Wt/WString.h>
#include <Wt/WText.h>
#include <Wt/WVBoxLayout.h>

#include <algorithm>
#include <array>
#include <exception>
#include <memory>
#include <string>
#include <utility>

#include "AmplitudeMenuModes.h"
#include "ChannelSelectionDialog.h"
#include "MenuStyle.h"
#include "MiniSeedReader.h"
#include "WaveformDisplay.h"
#include "WaveformPreview.h"

namespace itsv {

/**
 * @brief Wt application coordinating file import, menus, main canvas, overview and saved layout.
 */
class ItsvApplication final : public Wt::WApplication {
public:
  /**
   * @brief Construct the application UI and connect browser/server layout signals.
   * @param environment Wt request/session environment.
   */
  explicit ItsvApplication(const Wt::WEnvironment& environment)
    : Wt::WApplication(environment),
      restoreLayout_(this, "restoreLayout"),
      previewHeightChanged_(this, "previewHeightChanged"),
      mainViewportResized_(this, "mainViewportResized")
  {
    setTitle("Interactive Time Series Viewer");
    applyMenuStyle(styleSheet());

    rootLayout_ = root()->setLayout(std::make_unique<Wt::WVBoxLayout>());
    rootLayout_->setContentsMargins(6, 6, 6, 6);

    buildMenuBar(rootLayout_);

    // A native scroll container keeps the waveform canvas scrollable while the
    // preview and status bar remain fixed. WPaintedWidget is NOT managed by
    // a Wt layout inside this container: it needs explicit pixel dimensions.
    mainScroll_ = rootLayout_->addWidget(std::make_unique<Wt::WContainerWidget>(), 1);
    mainScroll_->setAttributeValue("data-itsv-waveform-scroll", "1");
    mainScroll_->setAttributeValue("style",
        "min-height:0;min-width:0;overflow-y:auto;overflow-x:hidden;"
        "overscroll-behavior:contain;position:relative;background:#e4e8ec;");
    display_ = mainScroll_->addWidget(std::make_unique<WaveformDisplayWidget>());
    // WPaintedWidget needs explicit initial dimensions until the browser
    // reports the waveform viewport's actual size.
    display_->resize(Wt::WLength(mainViewportWidth_), Wt::WLength(mainViewportHeight_));
    // A small custom drag target uses only client-side pointer movement until
    // release; the expensive Wt painting work runs once at the final height.
    // Unlike WBoxLayout::setResizable(), this preserves flex layout sizing.
    previewSplitter_ = rootLayout_->addWidget(std::make_unique<Wt::WContainerWidget>(), 0);
    previewSplitter_->setMinimumSize(Wt::WLength::Auto, Wt::WLength(8));
    previewSplitter_->setMaximumSize(Wt::WLength::Auto, Wt::WLength(8));
    previewSplitter_->setAttributeValue("style",
        "box-sizing:border-box;height:8px;cursor:row-resize;"
        "border-top:1px solid #bbc2c9;border-bottom:1px solid #bbc2c9;"
        "background:#edf0f4;touch-action:none;user-select:none;");
    previewSplitter_->setToolTip("Drag vertically to resize the full-waveform preview");
    preview_ = rootLayout_->addWidget(std::make_unique<WaveformPreviewWidget>(*display_), 0);
    preview_->setMinimumSize(Wt::WLength::Auto, Wt::WLength(80));
    preview_->setMaximumSize(Wt::WLength::Auto, Wt::WLength(300));
    // Auto width is essential: specifying both dimensions to WPaintedWidget
    // disables its layout-size-aware backing canvas, causing stretched art.
    preview_->resize(Wt::WLength::Auto, Wt::WLength(145));
    preview_->decorationStyle().setBorder(
        Wt::WBorder(Wt::BorderStyle::Solid, Wt::WLength(1), Wt::WColor(119, 119, 119)));

    status_ = rootLayout_->addWidget(std::make_unique<Wt::WText>("Ready"), 0);
    status_->setStyleClass("dv-status");

    display_->statusText().connect([this](const std::string& text) { status_->setText(text); });
    display_->viewChanged().connect([this] { preview_->update(); });
    display_->dataChanged().connect([this] {
      preview_->invalidateCache();
      updateMainCanvasSize();
    });
    display_->rowHeightChanged().connect([this](int height) {
      // Programmatic viewport fitting must not be mistaken for a manual
      // divider drag and must not overwrite the saved fixed row height.
      if (fittingRows_) return; // updateMainCanvasSize() is already running.
      fitRowsToViewport_ = false;
      syncFitRowsMenu();
      savePreference("itsv.layout.autoFitRows", 0);
      savePreference("itsv.layout.rowHeight", height);
      updateMainCanvasSize();
    });
    display_->labelPanelWidthChanged().connect([this](int width) {
      savePreference("itsv.layout.labelWidth", width);
    });

    // Keep the flex layout. WBoxLayout::setResizable() uses a different
    // sizing path that can leave WPaintedWidget with a zero-size canvas.
    // Preview height is controlled by the dedicated splitter.

    restoreLayout_.connect([this](int fontSize, int labelWidth,
                                  int previewHeight, int rowHeight, int autoFit) {
      applyFontSize(fontSize, false);
      display_->setLabelPanelWidth(labelWidth);
      setPreviewHeight(previewHeight, false);
      fitRowsToViewport_ = autoFit != 0;
      syncFitRowsMenu();
      if (!fitRowsToViewport_) display_->setRowHeight(rowHeight);
      updateMainCanvasSize();
    });
    previewHeightChanged_.connect([this](int height) {
      setPreviewHeight(height, false);
    });
    mainViewportResized_.connect([this](int width, int height) {
      // The scrollbar changes clientWidth and the flex layout changes height.
      // Paint both dimensions at the size the browser actually allocated.
      if (width < 200 || height < 120) return;
      const int nextWidth = std::clamp(width, 200, 16384);
      const int nextHeight = std::clamp(height, 120, 10000);
      if (mainViewportWidth_ != nextWidth || mainViewportHeight_ != nextHeight) {
        mainViewportWidth_ = nextWidth;
        mainViewportHeight_ = nextHeight;
        updateMainCanvasSize();
      }
    });
    installPreviewSplitter();

    buildMenus();
    display_->amplitudeModeChanged().connect([this](AmplitudeMode mode) {
      syncAmplitudeMenu(mode);
    });
    syncAmplitudeMenu(display_->viewState().amplitudeMode());
    installLayoutPersistence();
    installMainViewportObserver();
    updateMainCanvasSize();
  }

private:
  /// Vertical application layout: menus, scrolling waveforms, preview, and status.
  Wt::WVBoxLayout* rootLayout_{nullptr};
  /// File-opening and channel-selection commands.
  Wt::WPopupMenu* fileMenu_{nullptr};
  /// Time navigation, font size and layout commands.
  Wt::WPopupMenu* viewMenu_{nullptr};
  /// Checkable automatic waveform-height menu item.
  Wt::WMenuItem* fitRowsMenuItem_{nullptr};
  /// Bulk channel marking and visibility commands.
  Wt::WPopupMenu* tracesMenu_{nullptr};
  /// Vertical scale and clipping commands.
  Wt::WPopupMenu* amplitudeMenu_{nullptr};
  // Wt checkable entries for the five mutually exclusive scaling modes.
  /// One checkable menu entry per exclusive amplitude mode.
  std::array<Wt::WMenuItem*, amplitudeMenuModes.size()> amplitudeModeItems_{};
  /// Mouse-control and application information commands.
  Wt::WPopupMenu* helpMenu_{nullptr};
  /// Scrollable parent of the main waveform painting surface.
  Wt::WContainerWidget* mainScroll_{nullptr};
  /// Main interactive waveform canvas, owned by the Wt widget tree.
  WaveformDisplayWidget* display_{nullptr};
  /// Full-recording overview, owned by the Wt widget tree.
  WaveformPreviewWidget* preview_{nullptr};
  /// Bottom-line text reporting UI and load status.
  Wt::WText* status_{nullptr};
  /// Draggable divider controlling the overview height.
  Wt::WContainerWidget* previewSplitter_{nullptr};
  /// Currently open channel dialog, or null if closed.
  ChannelSelectionDialog* channelDialog_{nullptr};
  /// Sequence number used to give popup menus distinct CSS classes.
  int popupIndex_{0};
  /// Client event carrying persisted browser layout preferences.
  Wt::JSignal<int, int, int, int, int> restoreLayout_;
  /// Client event reporting a completed overview-height drag.
  Wt::JSignal<int> previewHeightChanged_;
  /// Client event reporting the waveform scroll viewport size.
  Wt::JSignal<int, int> mainViewportResized_;

  /// Current overview height in pixels.
  int previewHeight_{145};
  /// Current waveform viewport width in pixels.
  int mainViewportWidth_{1024};
  /// Current waveform viewport height in pixels.
  int mainViewportHeight_{420};
  /// Whether uniform rows automatically fill available viewport height.
  bool fitRowsToViewport_{true};
  /// Guard preventing automatic fitting from disabling itself via signals.
  bool fittingRows_{false};

  /// Default UI label font size in pixels.
  static constexpr int kDefaultFontSize = 11;
  /// Default left information-panel width in pixels.
  static constexpr int kDefaultLabelWidth = 150;
  /// Default overview panel height in pixels.
  static constexpr int kDefaultPreviewHeight = 145;
  /// Default row height when automatic fitting is disabled.
  static constexpr int kDefaultRowHeight = uniform_rows::defaultHeight;

  /**
   * @brief Create the File, View, Traces, Ampl, and Help menu controls.
   * @param rootLayout Vertical layout receiving the menu bar.
   */
  void buildMenuBar(Wt::WVBoxLayout* rootLayout)
  {
    auto* bar = rootLayout->addWidget(std::make_unique<Wt::WContainerWidget>(), 0);
    bar->addStyleClass("itsv-menu-bar");
    bar->setMinimumSize(Wt::WLength::Auto, Wt::WLength(36));
    bar->setMaximumSize(Wt::WLength::Auto, Wt::WLength(36));
    auto* row = bar->setLayout(std::make_unique<Wt::WHBoxLayout>());
    row->setContentsMargins(4, 2, 4, 2);
    row->setSpacing(2);

    auto file = std::make_unique<Wt::WPopupMenu>();
    auto view = std::make_unique<Wt::WPopupMenu>();
    auto traces = std::make_unique<Wt::WPopupMenu>();
    auto amplitude = std::make_unique<Wt::WPopupMenu>();
    auto help = std::make_unique<Wt::WPopupMenu>();
    fileMenu_ = file.get();
    viewMenu_ = view.get();
    tracesMenu_ = traces.get();
    amplitudeMenu_ = amplitude.get();
    helpMenu_ = help.get();

    addMenuButton(row, "File", std::move(file));
    addMenuButton(row, "View", std::move(view));
    addMenuButton(row, "Traces", std::move(traces));
    addMenuButton(row, "Ampl", std::move(amplitude));
    addMenuButton(row, "Help", std::move(help));
    row->addStretch(1);
  }

  /**
   * @brief Create a styled top-level menu button and attach a popup.
   * @param row Horizontal menu-bar layout.
   * @param title Text visible on the menu button.
   * @param popup Menu whose ownership is transferred to the button.
   */
  void addMenuButton(Wt::WHBoxLayout* row, const std::string& title,
                     std::unique_ptr<Wt::WPopupMenu> popup)
  {
    auto* button = row->addWidget(std::make_unique<Wt::WPushButton>(title));
    popup->addStyleClass("itsv-popup-menu");
    const std::string popupClass = "itsv-popup-menu-" + std::to_string(popupIndex_++);
    popup->addStyleClass(popupClass);
    popup->setAdjust(Wt::Orientation::Vertical);
    button->setMenu(std::move(popup));

    const std::string buttonJs = button->jsRef();
    // Position the popup after Wt makes it visible. The observer follows
    // the actual menu state instead of relying on animation-frame timing.
    button->clicked().connect(
        "function(o,e){var b=" + buttonJs + ";if(!b)return;"
        "var place=function(){var m=document.querySelector('ul." + popupClass + "');"
        "if(!m||getComputedStyle(m).display==='none')return false;"
        "var r=b.getBoundingClientRect(),mw=m.offsetWidth||220,mh=m.offsetHeight||0;"
        "var x=Math.max(4,Math.min(r.left,innerWidth-mw-4));"
        "var y=(mh&&r.bottom+mh>innerHeight-4&&r.top-mh>=4)?r.top-mh:r.bottom;"
        "m.style.position='absolute';m.style.left=Math.round(x+scrollX)+'px';"
        "m.style.right='auto';m.style.top=Math.round(y+scrollY)+'px';"
        "m.style.bottom='auto';m.style.transform='none';"
        "m.style.visibility='visible';return true;};"
        "if(place())return;"
        "var observer=new MutationObserver(function(){if(place())observer.disconnect();});"
        "observer.observe(document.body,{attributes:true,childList:true,subtree:true,"
        "attributeFilter:['style','class']});"
        "setTimeout(function(){observer.disconnect();},5000);"
        "}");

    button->setMargin(0, Wt::Side::Left | Wt::Side::Right);
    button->setMinimumSize(Wt::WLength(52), Wt::WLength(30));
    button->setMaximumSize(Wt::WLength::Auto, Wt::WLength(30));
    button->addStyleClass("itsv-menu-button");
  }

  /**
   * @brief Populate menu commands and connect handlers to the viewer.
   */
  void buildMenus()
  {
    fileMenu_->addItem("Open MiniSEED...")->triggered().connect([this] { showOpenDialog(); });
    fileMenu_->addItem("Select channels...")->triggered().connect([this] { openChannelDialog(); });
    fileMenu_->addSeparator();
    fileMenu_->addItem("Quit")->triggered().connect([this] { quit(); });

    viewMenu_->addItem("Fit time to data")->triggered().connect([this] { display_->fitToData(); });
    viewMenu_->addSeparator();
    viewMenu_->addItem("Expand horizontal")->triggered().connect([this] { display_->zoomTimeIn(); });
    viewMenu_->addItem("Contract horizontal")->triggered().connect([this] { display_->zoomTimeOut(); });
    viewMenu_->addItem("Expand vertical")->triggered().connect([this] { display_->expandAmplitude(); });
    viewMenu_->addItem("Contract vertical")->triggered().connect([this] { display_->contractAmplitude(); });
    viewMenu_->addSeparator();
    viewMenu_->addItem("Shift left")->triggered().connect([this] { display_->shiftTimeLeft(); });
    viewMenu_->addItem("Shift right")->triggered().connect([this] { display_->shiftTimeRight(); });
    viewMenu_->addSeparator();
    viewMenu_->addSectionHeader("Font size");
    viewMenu_->addItem("Small (10 px)")->triggered().connect([this] { applyFontSize(10); });
    viewMenu_->addItem("Normal (11 px)")->triggered().connect([this] { applyFontSize(11); });
    viewMenu_->addItem("Large (12 px)")->triggered().connect([this] { applyFontSize(12); });
    viewMenu_->addSeparator();
    fitRowsMenuItem_ = viewMenu_->addItem("Fit waveform heights to window");
    fitRowsMenuItem_->setCheckable(true);
    fitRowsMenuItem_->triggered().connect([this] {
      setFitRowsToViewport(!fitRowsToViewport_);
    });
    syncFitRowsMenu();
    viewMenu_->addItem("Reset waveform heights (110 px)")->triggered().connect([this] {
      setManualRowHeight(kDefaultRowHeight);
    });
    viewMenu_->addItem("Layout settings...")->triggered().connect([this] { showLayoutSettings(); });
    viewMenu_->addSeparator();
    viewMenu_->addItem("Reset layout")->triggered().connect([this] { resetLayout(); });

    tracesMenu_->addItem("Keep marked")->triggered().connect([this] { display_->keepMarkedTraces(); });
    tracesMenu_->addItem("Delete marked")->triggered().connect([this] { display_->deleteMarkedTraces(); });
    tracesMenu_->addSeparator();
    tracesMenu_->addItem("Mark all")->triggered().connect([this] { display_->markAllTraces(); });
    tracesMenu_->addItem("Clear marks")->triggered().connect([this] { display_->clearMarkedTraces(); });
    tracesMenu_->addSeparator();
    tracesMenu_->addItem("Original traces/order")->triggered().connect([this] { display_->restoreTraces(); });

    // The five amplitude modes are mutually exclusive. Keep the menu's
    // native checkmarks synchronized with mouse and keyboard changes.
    auto addAmplitudeMode = [this](AmplitudeMode mode, const char* label,
                                   const char* status,
                                   void (WaveformDisplayWidget::*select)()) {
      auto* item = amplitudeMenu_->addItem(label);
      item->setCheckable(true);
      for (std::size_t i = 0; i < amplitudeMenuModes.size(); ++i) {
        if (amplitudeMenuModes[i] == mode) amplitudeModeItems_[i] = item;
      }
      item->triggered().connect([this, select, status] {
        (display_->*select)();
        status_->setText(std::string("Ampl: ") + status);
      });
    };
    addAmplitudeMode(AmplitudeMode::Auto, "Auto", "Auto",
                     &WaveformDisplayWidget::setAmplitudeAuto);
    addAmplitudeMode(AmplitudeMode::Fixed, "Fixed", "Fixed",
                     &WaveformDisplayWidget::setAmplitudeFixed);
    addAmplitudeMode(AmplitudeMode::ZeroCentered, "Zero-centered (A0)", "A0",
                     &WaveformDisplayWidget::setAmplitudeZeroCentered);
    addAmplitudeMode(AmplitudeMode::FirstTrace, "First trace (A1)", "A1",
                     &WaveformDisplayWidget::setAmplitudeFirstTrace);
    addAmplitudeMode(AmplitudeMode::AllSame, "All same (AA)", "AA",
                     &WaveformDisplayWidget::setAmplitudeAllSame);
    amplitudeMenu_->addSeparator();
    amplitudeMenu_->addItem("Clip to trace panel")->triggered().connect([this] {
      const bool enabled = display_->toggleClip();
      status_->setText(std::string("Clip ") + (enabled ? "ON" : "OFF"));
    });
    amplitudeMenu_->addItem("Invert")->triggered().connect([this] {
      display_->toggleInvert();
      status_->setText("Invert toggled");
    });

    helpMenu_->addItem("Mouse controls")->triggered().connect([this] {
      showHelpDialog("Mouse controls",
        "<p><b>Time and amplitude zoom use two clicks.</b> The modifier is needed only for the first click. Press <b>Esc</b> to cancel.</p>"
        "<table cellpadding=\"6\"><tr><th align=\"left\">First click</th><th align=\"left\">Action</th></tr>"
        "<tr><td>Shift + left</td><td>Time zoom in</td></tr>"
        "<tr><td>Shift + right</td><td>Time zoom out</td></tr>"
        "<tr><td>Ctrl + left</td><td>Amplitude zoom in</td></tr>"
        "<tr><td>Ctrl + right</td><td>Amplitude zoom out</td></tr></table>"
        "<p><b>Ctrl + Shift + left</b> is an alternative for time zoom out.</p>"
        "<p>Plain left/right click shifts time. Middle drag pans time. Channel label click selects one trace; Ctrl-click marks multiple traces.</p>"
        "<p>The lower overview shows the full selected channel; click it to center the visible time window.</p>", 760);
    });
    helpMenu_->addItem("Preview diagnostics (Chrome console)")->triggered().connect([this] {
      showPreviewDiagnostics();
    });
    helpMenu_->addSeparator();
    helpMenu_->addItem("About")->triggered().connect([this] {
      showHelpDialog("About ITSV",
        "<p><b>ITSV — Interactive Time Series Viewer</b></p>"
        "<p>Copyright &copy; 2026 Andrei Akimov.</p>"
        "<p>Original ITSV source code: MIT License (see LICENSE).</p>"
        "<p>Built using <a href=\"https://www.webtoolkit.eu/wt\" "
        "target=\"_blank\" rel=\"noopener noreferrer\">"
        "Wt — C++ Web Toolkit</a> by Emweb, "
        "GPL version 2 only (with OpenSSL exception) or commercial license.</p>"
        "<p>MiniSEED decoding: <a href=\"https://github.com/EarthScope/libmseed\" "
        "target=\"_blank\" rel=\"noopener noreferrer\">"
        "libmseed 2.19.8</a>, LGPL version 3 or later.</p>"
        "<p>Dependency licenses are separate from ITSV's original-source "
        "license. See THIRD_PARTY_NOTICES.md for distribution limitations.</p>", 610);
    });
  }

  /**
   * @brief Check only the menu item corresponding to the current scaling mode.
   * @param active Current amplitude scaling mode.
   */
  void syncAmplitudeMenu(AmplitudeMode active)
  {
    for (std::size_t i = 0; i < amplitudeModeItems_.size(); ++i) {
      if (amplitudeModeItems_[i])
        amplitudeModeItems_[i]->setChecked(amplitudeMenuChecked(amplitudeMenuModes[i], active));
    }
  }

  /**
   * @brief Persist an integer UI preference in browser localStorage.
   * @param key Stable preference storage key.
   * @param value Value to store as an integer.
   */
  void savePreference(const std::string& key, int value)
  {
    doJavaScript("try{localStorage.setItem('" + key + "','" +
                 std::to_string(value) + "');}catch(e){};");
  }

  /**
   * @brief Apply a shared waveform, overview and menu font size.
   * @param pixels Requested waveform text size in pixels.
   * @param persist Whether to save the preference in the browser.
   */
  void applyFontSize(int pixels, bool persist = true)
  {
    const int size = std::clamp(pixels, 10, 12);
    display_->setFontSize(size);
    preview_->setFontSize(size);
    updateMainCanvasSize();
    doJavaScript("document.documentElement.style.setProperty('--itsv-menu-font-size','" +
                 std::to_string(size + 3) + "px');");
    if (persist) savePreference("itsv.layout.fontSize", size);
    status_->setText("Font size: " + std::to_string(size) + " px");
  }

  /**
   * @brief Update the check state of automatic waveform-height fitting.
   */
  void syncFitRowsMenu()
  {
    if (fitRowsMenuItem_) fitRowsMenuItem_->setChecked(fitRowsToViewport_);
  }

  /**
   * @brief Enable or disable equal-height auto-fit of visible trace rows.
   * @param enabled True to fit all rows to the viewport.
   * @param persist True to save the setting.
   */
  void setFitRowsToViewport(bool enabled, bool persist = true)
  {
    fitRowsToViewport_ = enabled;
    syncFitRowsMenu();
    if (persist) savePreference("itsv.layout.autoFitRows", enabled ? 1 : 0);
    if (enabled) updateMainCanvasSize();
    else if (persist) savePreference("itsv.layout.rowHeight", display_->rowHeight());
  }

  /**
   * @brief Switch to fixed row height and persist the requested height.
   * @param height Desired shared waveform row height in pixels.
   */
  void setManualRowHeight(int height)
  {
    setFitRowsToViewport(false);
    display_->setRowHeight(height);
    // Still persist the chosen manual height if it happens to be unchanged.
    savePreference("itsv.layout.rowHeight", display_->rowHeight());
    updateMainCanvasSize();
  }

  /**
   * @brief Resize the overview panel within its permitted bounds.
   * @param pixels Requested overview height in pixels.
   * @param persist Whether to save the setting.
   */
  void setPreviewHeight(int pixels, bool persist = true)
  {
    const int height = std::clamp(pixels, 80, 300);
    previewHeight_ = height;
    preview_->resize(Wt::WLength::Auto, Wt::WLength(previewHeight_));
    preview_->update();
    if (persist) savePreference("itsv.layout.previewHeight", height);
  }

  /**
   * @brief Restore all visual layout parameters to their defaults.
   */
  void resetLayout()
  {
    applyFontSize(kDefaultFontSize, false);
    display_->setLabelPanelWidth(kDefaultLabelWidth);
    setPreviewHeight(kDefaultPreviewHeight, false);
    savePreference("itsv.layout.rowHeight", kDefaultRowHeight);
    setFitRowsToViewport(true);
    savePreference("itsv.layout.fontSize", kDefaultFontSize);
    savePreference("itsv.layout.labelWidth", kDefaultLabelWidth);
    savePreference("itsv.layout.previewHeight", kDefaultPreviewHeight);
    status_->setText("Layout reset to defaults.");
  }

  /**
   * @brief Fit waveform row heights and size the scrollable painted canvas.
   */
  void updateMainCanvasSize()
  {
    if (!display_) return;
    if (fitRowsToViewport_) {
      const int fitted = display_->fittedRowHeight(mainViewportHeight_);
      if (fitted != display_->rowHeight()) {
        // setRowHeight emits rowHeightChanged synchronously. The guard avoids
        // turning auto-fit off or recursively resizing the same canvas.
        fittingRows_ = true;
        display_->setRowHeight(fitted);
        fittingRows_ = false;
      }
    }
    const int contentHeight = display_->minimumContentHeight();
    const int height = std::max(mainViewportHeight_, contentHeight);
    // This painted widget is a direct child of a scroll container, NOT a
    // layout-managed item. Auto width does not give Wt a paint-device width.
    // Use the browser's scroll viewport (excluding the scrollbar) in pixels.
    display_->resize(Wt::WLength(mainViewportWidth_), Wt::WLength(height));
    display_->update();
  }

  /**
   * @brief Observe browser viewport changes and report true canvas dimensions.
   */
  void installMainViewportObserver()
  {
    // The scroll viewport is controlled by the outer flex layout. Its child
    // WPaintedWidget is not in a Wt layout, so both drawing dimensions must be
    // reported by the browser. clientWidth excludes a visible scrollbar.
    const std::string send = mainViewportResized_.createCall({"width", "height"});
    doJavaScript("(function(){var el=" + mainScroll_->jsRef() + ";if(!el)return;"
      "var oldW=0,oldH=0;function report(){"
      "var width=Math.round(el.clientWidth),height=Math.round(el.clientHeight);"
      "if(width<200||height<120||(width===oldW&&height===oldH))return;"
      "oldW=width;oldH=height;" + send + "}"
      "if(window.ResizeObserver){var ro=new ResizeObserver(report);ro.observe(el);"
      "el.__itsvSizeObserver=ro;}else window.addEventListener('resize',report);"
      "report();})();");
  }

  /**
   * @brief Install client-side pointer handling for the overview-height divider.
   */
  void installPreviewSplitter()
  {
    // Pointer events work in Chrome and Firefox and keep the waveform canvas
    // free of layout resizing until the mouse button is released.
    const std::string signal = previewHeightChanged_.createCall({"height"});
    const std::string previewJs = preview_->jsRef();
    previewSplitter_->setAttributeValue("onpointerdown",
      "if(event.button!==0)return;event.preventDefault();event.stopPropagation();"
      "var bar=this,preview=" + previewJs + ";if(!preview)return;"
      "var start=event.clientY;var original=preview.getBoundingClientRect().height;"
      "var proposed=Math.round(original);"
      "var guide=document.createElement('div');"
      "guide.style.cssText='position:fixed;left:0;right:0;height:2px;"
      "background:#2196f3;z-index:2147483000;pointer-events:none';"
      "document.body.appendChild(guide);"
      "var move=function(e){proposed=Math.max(80,Math.min(300,"
      "Math.round(original+start-e.clientY)));"
      "guide.style.top=Math.round(bar.getBoundingClientRect().top+original-proposed)+'px';};"
      "var stop=function(e){document.removeEventListener('pointermove',move,true);"
      "document.removeEventListener('pointerup',stop,true);"
      "document.removeEventListener('pointercancel',cancel,true);guide.remove();"
      "var height=proposed;"
      "try{localStorage.setItem('itsv.layout.previewHeight',String(height));}catch(ex){};"
      + signal + "};"
      "var cancel=function(){document.removeEventListener('pointermove',move,true);"
      "document.removeEventListener('pointerup',stop,true);"
      "document.removeEventListener('pointercancel',cancel,true);guide.remove();};"
      "document.addEventListener('pointermove',move,true);"
      "document.addEventListener('pointerup',stop,true);"
      "document.addEventListener('pointercancel',cancel,true);move(event);");
  }

  /**
   * @brief Restore per-browser saved font, row, panel and preview sizes.
   */
  void installLayoutPersistence()
  {
    const std::string restoreCall = restoreLayout_.createCall(
        {"fontSize", "labelWidth", "previewHeight", "rowHeight", "autoFit"});
    doJavaScript(
      "(function(){"
      "function readInt(k,d,lo,hi){try{var v=parseInt(localStorage.getItem(k),10);"
      "if(!Number.isFinite(v))v=d;return Math.max(lo,Math.min(hi,v));}catch(e){return d;}}"
      "var fontSize=readInt('itsv.layout.fontSize',11,10,12);"
      "var labelWidth=readInt('itsv.layout.labelWidth',150,100,400);"
      "var previewHeight=readInt('itsv.layout.previewHeight',145,80,300);"
      "var rowHeight=readInt('itsv.layout.rowHeight',110,"
      + std::to_string(uniform_rows::minimumHeight) + ","
      + std::to_string(uniform_rows::maximumHeight) + ");"
      "var autoFit=readInt('itsv.layout.autoFitRows',1,0,1);"
      "document.documentElement.style.setProperty('--itsv-menu-font-size',(fontSize+3)+'px');" +
      restoreCall + "})();");
  }

  /**
   * @brief Open a dialog for font and waveform layout settings.
   */
  void showLayoutSettings()
  {
    auto* dialog = root()->addChild(std::make_unique<Wt::WDialog>("Layout settings"));
    dialog->setModal(true);
    dialog->setClosable(true);
    dialog->rejectWhenEscapePressed();
    dialog->resize(Wt::WLength(420), Wt::WLength::Auto);
    auto* table = dialog->contents()->addWidget(std::make_unique<Wt::WTable>());
    table->setAttributeValue("style", "border-spacing:10px;line-height:2;");
    const auto spin = [table](int row, const std::string& name, int low, int high, int value) {
      table->elementAt(row, 0)->addWidget(std::make_unique<Wt::WText>(name));
      auto* field = table->elementAt(row, 1)->addWidget(std::make_unique<Wt::WSpinBox>());
      field->setRange(low, high);
      field->setValue(value);
      return field;
    };
    auto* font = spin(0, "Font size (px)", 10, 12, display_->fontSize());
    auto* panel = spin(1, "Left panel width (px)", 100, 400, display_->labelPanelWidth());
    auto* rowHeight = spin(2, "Waveform heights (px, manual)",
                           uniform_rows::minimumHeight, uniform_rows::maximumHeight,
                           display_->rowHeight());
    auto* preview = spin(3, "Preview height (px)", 80, 300, previewHeight_);
    auto* apply = dialog->footer()->addWidget(std::make_unique<Wt::WPushButton>("Apply"));
    auto* cancel = dialog->footer()->addWidget(std::make_unique<Wt::WPushButton>("Cancel"));
    apply->clicked().connect([this, dialog, font, panel, preview, rowHeight] {
      applyFontSize(font->value());
      display_->setLabelPanelWidth(panel->value());
      // An explicitly changed row height switches from auto-fit to manual.
      if (rowHeight->value() != display_->rowHeight())
        setManualRowHeight(rowHeight->value());
      setPreviewHeight(preview->value());
      dialog->accept();
    });
    cancel->clicked().connect([dialog] { dialog->reject(); });
    dialog->finished().connect([this, dialog](Wt::DialogCode) { root()->removeChild(dialog); });
    dialog->show();
  }

  /**
   * @brief Write overview canvas dimensions to Chrome developer tools.
   */
  void showPreviewDiagnostics()
  {
    doJavaScript("(function(){var p=" + preview_->jsRef() + ";if(!p)return;"
      "var outer=p.getBoundingClientRect();"
      "var elements=Array.from(p.querySelectorAll('canvas,svg'));"
      "var results=elements.map(function(el){var r=el.getBoundingClientRect();return{"
      "tag:el.tagName,cssWidth:r.width,cssHeight:r.height,"
      "bitmapWidth:el.width&&el.width.baseVal?el.width.baseVal.value:el.width,"
      "bitmapHeight:el.height&&el.height.baseVal?el.height.baseVal.value:el.height,"
      "parentWidth:outer.width,parentHeight:outer.height};});"
      "console.table(results);if(!results.length)console.warn('No preview canvas/svg found');"
      "})();");
    status_->setText("Preview canvas dimensions sent to Chrome DevTools console (F12).");
  }

  /**
   * @brief Display a modal HTML help or About dialog.
   * @param title Dialog title.
   * @param text XHTML body text to display.
   * @param width Preferred dialog width in pixels.
   */
  void showHelpDialog(const Wt::WString& title, const Wt::WString& text, int width)
  {
    auto* dialog = root()->addChild(std::make_unique<Wt::WDialog>(title));
    dialog->setModal(true);
    dialog->setClosable(true);
    dialog->rejectWhenEscapePressed();
    dialog->resize(Wt::WLength(width), Wt::WLength::Auto);
    dialog->contents()->setPadding(Wt::WLength(12));
    auto* body = dialog->contents()->addWidget(std::make_unique<Wt::WText>(text, Wt::TextFormat::XHTML));
    body->setAttributeValue("style", "line-height:1.45;");
    auto* ok = dialog->footer()->addWidget(std::make_unique<Wt::WPushButton>("OK"));
    ok->clicked().connect([dialog] { dialog->accept(); });
    dialog->finished().connect([this, dialog](Wt::DialogCode) { root()->removeChild(dialog); });
    dialog->show();
  }

  /**
   * @brief Open the MiniSEED upload dialog and import selected waveform data.
   */
  void showOpenDialog()
  {
    auto* dialog = root()->addChild(std::make_unique<Wt::WDialog>("Open MiniSEED file"));
    dialog->setModal(true);
    dialog->setClosable(true);
    dialog->rejectWhenEscapePressed();
    dialog->resize(Wt::WLength(600), Wt::WLength::Auto);

    auto* body = dialog->contents()->addWidget(std::make_unique<Wt::WContainerWidget>());
    auto* layout = body->setLayout(std::make_unique<Wt::WVBoxLayout>());
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);
    layout->addWidget(std::make_unique<Wt::WText>(
        "Select a MiniSEED file. The file is uploaded to the local ITSV server and decoded with libmseed 2.19.8."));
    auto* upload = layout->addWidget(std::make_unique<Wt::WFileUpload>());
    upload->setMultiple(false);
    upload->setFileTextSize(80);
    auto* message = layout->addWidget(std::make_unique<Wt::WText>(""));
    message->setInline(false);

    auto* footer = dialog->footer()->addWidget(std::make_unique<Wt::WContainerWidget>());
    auto* buttons = footer->setLayout(std::make_unique<Wt::WHBoxLayout>());
    buttons->setSpacing(8);
    auto* load = buttons->addWidget(std::make_unique<Wt::WPushButton>("Upload && Load"));
    auto* cancel = buttons->addWidget(std::make_unique<Wt::WPushButton>("Cancel"));
    load->setEnabled(false);

    upload->changed().connect([=] {
      load->setEnabled(true);
      const auto name = upload->clientFileName();
      message->setText(name.empty() ? "File selected." : "Selected: " + name);
    });
    load->clicked().connect([=] {
      load->setEnabled(false);
      message->setText("Uploading...");
      upload->upload();
    });
    upload->fileTooLarge().connect([=] {
      message->setText("File too large for the configured Wt request limit.");
      load->setEnabled(true);
    });
    upload->uploaded().connect([=] {
      const std::string path = upload->spoolFileName();
      if (path.empty()) {
        message->setText("Upload completed but Wt did not provide a spool file path.");
        load->setEnabled(true);
        return;
      }
      message->setText("Uploaded. Decoding MiniSEED...");
      try {
        auto result = MiniSeedReader::read(path);
        display_->setRecording(std::move(result.recording));
        if (result.warnings.empty())
          status_->setText("Loaded: " + upload->clientFileName());
        else
          status_->setText("Loaded with " + std::to_string(result.warnings.size()) + " warning(s)");
        dialog->accept();
      } catch (const std::exception& error) {
        message->setText("Load failed: " + std::string(error.what()));
        load->setEnabled(true);
      }
    });
    cancel->clicked().connect([dialog] { dialog->reject(); });
    dialog->finished().connect([this, dialog](Wt::DialogCode code) {
      root()->removeChild(dialog);
      if (code == Wt::DialogCode::Accepted) openChannelDialog();
    });
    dialog->show();
  }

  /**
   * @brief Open the channel tree picker for the loaded recording.
   */
  void openChannelDialog()
  {
    if (channelDialog_) return;
    if (!display_->recording()) {
      status_->setText("Open a MiniSEED file first.");
      return;
    }
    auto* dialog = root()->addChild(std::make_unique<ChannelSelectionDialog>(*display_));
    channelDialog_ = dialog;
    dialog->finished().connect([this, dialog](Wt::DialogCode) {
      channelDialog_ = nullptr;
      root()->removeChild(dialog);
    });
    dialog->show();
  }
};

/**
 * @brief Factory used by Wt to create one application instance per session.
 * @param environment Wt request/session environment.
 * @return New ITSV application instance.
 */
std::unique_ptr<Wt::WApplication> createApplication(const Wt::WEnvironment& environment)
{
  return std::make_unique<ItsvApplication>(environment);
}

} // namespace itsv

/**
 * @brief Launch the Wt HTTP server and register the ITSV application factory.
 * @param argc Command-line argument count.
 * @param argv Command-line argument values interpreted by Wt.
 * @return Wt server exit code.
 */
int main(int argc, char** argv)
{
  return Wt::WRun(argc, argv, &itsv::createApplication);
}
