#!/usr/bin/env python3
"""Interactive Wt/Chromium regression checks against a *running* ITSV server.

Run on a machine with ITSV, Chromium and Playwright installed:
  python3 -m pip install playwright pillow
  python3 -m playwright install chromium
  python3 tests/browser_smoke.py --file /path/to/multichannel.mseed

Tests canvas/backing sizes and pixel colors at three window widths, channel
selection, preview navigation, pointer resizing, and localStorage persistence.
This script does not launch ITSV; start `./run` in another terminal first.
"""
import argparse
import re
import time
from pathlib import Path


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print('PASS:', message)


def checked_amplitude_modes(page):
    """Open Ampl and read Wt's actual checkbox states from the menu DOM."""
    page.get_by_text('Ampl', exact=True).first.click()
    page.wait_for_timeout(150)
    return page.evaluate('''() => {
      const labels = ['Auto','Fixed','Zero-centered (A0)',
                      'First trace (A1)','All same (AA)'];
      const menus = [...document.querySelectorAll('ul.itsv-popup-menu')];
      const menu = menus.find(m => [...m.querySelectorAll('li')]
        .some(li => li.textContent.trim() === 'Auto'));
      if (!menu) return null;
      return Object.fromEntries(labels.map(label => {
        const row = [...menu.querySelectorAll('li')]
          .find(li => li.textContent.trim() === label);
        const input = row && row.querySelector('input[type="checkbox"]');
        return [label, input ? input.checked : null];
      }));
    }''')


def check_amplitude_menu(page, expected):
    states = checked_amplitude_modes(page)
    check(states is not None and all(x is not None for x in states.values()),
          'all five amplitude modes have Wt checkboxes')
    active = [name for name, checked in states.items() if checked]
    check(active == [expected], f'only {expected} has a check mark: {states}')


def choose_amplitude_menu_item(page, name):
    page.get_by_text(name, exact=True).last.click()
    page.wait_for_timeout(150)


def canvas_state(page):
    return page.evaluate('''() => {
      const scroller = document.querySelector('[data-itsv-waveform-scroll="1"]');
      const surface = el => {
        if (!el) return null;
        const r = el.getBoundingClientRect();
        const w = el.width?.baseVal?.value ?? el.width;
        const h = el.height?.baseVal?.value ?? el.height;
        return {top:r.top, left:r.left, cssWidth:r.width, cssHeight:r.height,
                bitmapWidth:Number(w), bitmapHeight:Number(h)};
      };
      const valid = el => {
        const r = el.getBoundingClientRect();
        return r.width > 200 && r.height > 60;
      };
      // Do not silently treat the preview as BOTH canvases when the main
      // WPaintedWidget is blank or has a zero-width backing surface.
      const main = scroller && [...scroller.querySelectorAll('canvas,svg')]
        .find(valid);
      const preview = [...document.querySelectorAll('canvas,svg')]
        .filter(el => (!scroller || !scroller.contains(el)) && valid(el))
        .sort((a,b) => a.getBoundingClientRect().top - b.getBoundingClientRect().top)
        .at(-1);
      return {main:surface(main),preview:surface(preview),dpr:devicePixelRatio};
    }''')


def has_blue_preview(page, area):
    from PIL import Image
    from io import BytesIO
    png = page.screenshot()
    img = Image.open(BytesIO(png)).convert('RGB')
    # Sample the *background* in the waveform half rather than the left label.
    left = max(0, int(area['left'] + area['cssWidth'] * .42))
    right = min(img.width, int(area['left'] + area['cssWidth'] * .92))
    top = max(0, int(area['top'] + area['cssHeight'] * .25))
    bottom = min(img.height, int(area['top'] + area['cssHeight'] * .70))
    crop = img.crop((left, top, right, bottom))
    pixels = list(crop.getdata())
    blue = sum(1 for r,g,b in pixels if r < 35 and g < 35 and b > 130)
    return blue / max(1,len(pixels))


def has_selection_rectangle(page, area, label_width):
    """Check for both inset horizontal edges (not just two time markers).

    The test uses pixel sampling because WPainter commands are rendered into
    the canvas and are not DOM nodes. The test browser has DPR=1.
    """
    from PIL import Image
    from io import BytesIO
    img = Image.open(BytesIO(page.screenshot())).convert('RGB')
    plot_height = area['cssHeight'] - 5 - 26
    if plot_height < 14:
        return False
    inset = max(4.0, min(10.0, plot_height * 0.08))
    left = max(0, round(area['left'] + label_width + 6))
    right = min(img.width, round(area['left'] + area['cssWidth'] - 6))
    top = area['top'] + 5 + inset
    bottom = area['top'] + 5 + plot_height - inset

    def has_horizontal_edge(y):
        # Allow a few pixels for canvas borders and pixel rounding.
        longest = 0
        for row in range(max(0, round(y) - 4), min(img.height, round(y) + 5)):
            run = 0
            for x in range(left, right):
                r, g, b = img.getpixel((x, row))
                if r > 210 and g > 220 and b > 215:
                    run += 1
                    longest = max(longest, run)
                else:
                    run = 0
        return longest >= 15

    return has_horizontal_edge(top) and has_horizontal_edge(bottom)


def run(args):
    from playwright.sync_api import sync_playwright
    with sync_playwright() as tool:
      browser = tool.chromium.launch(headless=not args.show)
      page = browser.new_page(viewport={'width':1600,'height':900}, device_scale_factor=1)
      page.goto(args.url, wait_until='domcontentloaded', timeout=30000)
      check(page.get_by_text('File', exact=True).count() > 0, 'Wt application loaded')
      page.get_by_text('File', exact=True).first.click()
      page.get_by_text('Open MiniSEED...', exact=True).click()
      page.locator('input[type=file]').last.set_input_files(str(Path(args.file).resolve()))
      page.get_by_role('button', name=re.compile('Upload')).last.click()
      page.get_by_text('Select Channels', exact=True).first.wait_for(timeout=45000)
      page.get_by_role('button', name='Select all').click()
      page.get_by_role('button', name='OK').last.click()
      page.wait_for_timeout(500)
      check_amplitude_menu(page, 'Auto')
      # On first load every visible trace expands to share the viewport,
      # until the 56px readable minimum would be violated.
      scroller = page.locator('[data-itsv-waveform-scroll="1"]')
      rows = page.locator('[data-itsv-row-height]').first
      count = int(rows.get_attribute('data-itsv-row-count'))
      height = int(rows.get_attribute('data-itsv-row-height'))
      usable = scroller.evaluate('el => el.clientHeight') - 64
      expected = max(56, min(1600, usable // max(1, count)))
      check(count >= 3 and abs(height - expected) <= 1,
            f'initial {count} waveform(s) fill main viewport (min 56px)')
      page.set_viewport_size({'width': 1600, 'height': 650})
      page.wait_for_timeout(450)
      compact = int(rows.get_attribute('data-itsv-row-height'))
      usable_compact = scroller.evaluate('el => el.clientHeight') - 64
      check(compact == max(56, min(1600, usable_compact // count)),
            'automatic waveform rows fit a smaller browser window')
      page.set_viewport_size({'width': 1600, 'height': 900})
      page.wait_for_timeout(450)
      restored = int(rows.get_attribute('data-itsv-row-height'))
      check(restored >= compact, 'automatic rows expand on larger browser viewport')
      choose_amplitude_menu_item(page, 'Fixed')
      check_amplitude_menu(page, 'Fixed')
      choose_amplitude_menu_item(page, 'Fixed')  # same mode stays checked
      check_amplitude_menu(page, 'Fixed')
      choose_amplitude_menu_item(page, 'Zero-centered (A0)')
      check_amplitude_menu(page, 'Zero-centered (A0)')
      choose_amplitude_menu_item(page, 'Auto')
      check_amplitude_menu(page, 'Auto')
      # Keyboard amplitude scaling also switches the mode to Fixed.
      choose_amplitude_menu_item(page, 'Auto')
      main = canvas_state(page)['main']
      page.mouse.click(main['left'] + 290, main['top'] + 45)
      page.keyboard.press('ArrowUp')
      page.wait_for_timeout(250)
      check_amplitude_menu(page, 'Fixed')
      choose_amplitude_menu_item(page, 'Auto')
      for width in (1000,1600,1920):
        page.set_viewport_size({'width':width,'height':900})
        page.wait_for_timeout(500)
        state=canvas_state(page)
        check(state['preview'] is not None and state['main'] is not None,
              f'waveform and overview have distinct, nonzero drawing surfaces at {width}px')
        main = state['main']
        scrollbox = page.locator('[data-itsv-waveform-scroll="1"]').bounding_box()
        viewport_width = page.locator('[data-itsv-waveform-scroll="1"]').evaluate('el => el.clientWidth')
        check(scrollbox is not None and main['top'] < state['preview']['top'],
              f'main drawing is in the scroll viewport at {width}px')
        check(abs(main['cssWidth'] - viewport_width) < 7,
              f'main drawing width matches scroll viewport at {width}px: {main}')
        check(abs(main['bitmapWidth'] - main['cssWidth']) < 5 or
              abs(main['bitmapWidth'] / max(1,state['dpr']) - main['cssWidth']) < 5,
              f'main backing width matches display at {width}px: {main}')
        # Check that the main viewport paints its own waveform background.
        visible_top = {'left':scrollbox['x'], 'top':scrollbox['y'],
                       'cssWidth':scrollbox['width'],
                       'cssHeight':min(scrollbox['height'], 110)}
        main_blue = has_blue_preview(page, visible_top)
        check(main_blue > .65,
              f'main waveform has a blue plot rather than a blank gray pane at {width}px '
              f'({main_blue:.1%})')
        view = state['preview']
        check(view['cssWidth'] > 0.85 * width and view['cssHeight'] > 60,
              f'overview occupies usable size at {width}px')
        check(abs(view['bitmapWidth'] - view['cssWidth']) < 5 or
              abs(view['bitmapWidth'] / max(1,state['dpr']) - view['cssWidth']) < 5,
              f'overview backing width matches display at {width}px: {view}')
        fraction=has_blue_preview(page,view)
        check(fraction > .50, f'blue overview background visible at {width}px ({fraction:.1%})')
        label_width=page.evaluate("Number(localStorage.getItem('itsv.layout.labelWidth') || '150')")
        check(has_selection_rectangle(page, view, label_width),
              f'overview displays a closed selection rectangle at {width}px')
      # Native waveform scrolling must be isolated from the fixed preview.
      scroller = page.locator('[data-itsv-waveform-scroll="1"]')
      check(scroller.count() == 1, 'main waveform pane has a native scroll container')
      geometry = scroller.evaluate('''el => ({clientHeight:el.clientHeight,scrollHeight:el.scrollHeight})''')
      check(geometry['clientHeight'] > 150, 'waveform scroll viewport has usable height')
      if geometry['scrollHeight'] > geometry['clientHeight'] + 10:
        scroller.evaluate('el => {el.scrollTop=150}')
        page.wait_for_timeout(100)
        check(scroller.evaluate('el => el.scrollTop') > 0, 'native vertical scrolling works')
        scroller.evaluate('el => {el.scrollTop=0}')

      # A plain click should switch the selected trace and update the overview.
      main=canvas_state(page)['main']
      initial_rows=page.locator('[data-itsv-row-height]').first
      row_h=int(initial_rows.get_attribute('data-itsv-row-height'))
      second_y=main['top']+row_h * 1.5
      label_x=main['left']+50
      page.mouse.click(label_x,second_y)
      page.wait_for_timeout(300)
      status=page.locator('body').inner_text()[-200:]
      check('overview:' in status.lower(), 'label click updates selected overview channel')
      # Ensure the full waveform overview responds to horizontal navigation.
      page.get_by_text('View', exact=True).first.click()
      page.get_by_text('Expand horizontal', exact=True).click()
      page.wait_for_timeout(250)
      area=canvas_state(page)['preview']
      before=page.locator('body').inner_text()[-350:]
      page.mouse.click(area['left'] + area['cssWidth'] * .73,
                       area['top'] + area['cssHeight'] * .48)
      page.wait_for_timeout(300)
      after=page.locator('body').inner_text()[-350:]
      check(before != after, 'clicking overview changes time view/status')
      # Check persistence of drag-resized left information panel.
      main=canvas_state(page)['main']
      label=int(page.evaluate("localStorage.getItem('itsv.layout.labelWidth') || '150'"))
      x=main['left'] + label
      y=main['top'] + row_h * .50
      page.mouse.move(x,y)
      page.mouse.down()
      page.mouse.move(x+37,y,steps=6)
      page.mouse.up()
      page.wait_for_timeout(300)
      updated=page.evaluate("Number(localStorage.getItem('itsv.layout.labelWidth'))")
      check(abs(updated-label-37) <= 4, 'left panel drag saves width in localStorage')
      # Height splitter sends a single final update, not global mouseup events.
      splitter=page.locator('[style*="cursor:row-resize"]').last
      r=splitter.bounding_box()
      check(r is not None, 'preview height drag handle available')
      initial=page.evaluate("Number(localStorage.getItem('itsv.layout.previewHeight') || 145)")
      page.mouse.move(r['x']+r['width']*.5,r['y']+r['height']*.5)
      page.mouse.down()
      page.mouse.move(r['x']+r['width']*.5,r['y']-25,steps=6)
      page.mouse.up()
      page.wait_for_timeout(350)
      updated=page.evaluate("Number(localStorage.getItem('itsv.layout.previewHeight'))")
      check(updated > initial, 'preview splitter increases height and saves preference')
      # All waveform rows share a single height. Drag any boundary (first
      # one is reachable without scrolling) and verify browser persistence.
      scroller.evaluate('el => {el.scrollTop=0}')
      main=canvas_state(page)['main']
      previous=int(initial_rows.get_attribute('data-itsv-row-height'))
      first_separator=main['top']+previous-1
      x=main['left']+80
      page.mouse.move(x,first_separator)
      page.mouse.down()
      page.mouse.move(x,first_separator+300,steps=12)
      page.mouse.up()
      page.wait_for_timeout(450)
      next_height=int(initial_rows.get_attribute('data-itsv-row-height'))
      stored=int(page.evaluate("localStorage.getItem('itsv.layout.rowHeight') || '0'"))
      check(next_height > previous and next_height == stored,
            'dragging one divider grows every row and saves fixed height')
      check(page.evaluate("localStorage.getItem('itsv.layout.autoFitRows')") == '0',
            'manual row resize switches off automatic fit')
      count=int(initial_rows.get_attribute('data-itsv-row-count'))
      viewport_height=scroller.evaluate('el => el.clientHeight')
      painted_height=canvas_state(page)['main']['cssHeight']
      check(abs(painted_height-max(viewport_height,count*next_height+64)) < 9,
            'canvas content height equals uniform row heights plus time axis')
      main_blue = has_blue_preview(page, {'left':scroller.bounding_box()['x'],
                'top':scroller.bounding_box()['y'],
                'cssWidth':scroller.bounding_box()['width'], 'cssHeight':80})
      check(main_blue > .65, 'main waveform remains painted after row resizing')
      page.reload(wait_until='domcontentloaded')
      page.wait_for_timeout(400)
      check(page.evaluate("localStorage.getItem('itsv.layout.rowHeight')") == str(next_height),
            'shared row height survives reload in localStorage')
      # Verify a saved height much larger than a browser viewport survives
      # restoration without unintended clamping.
      page.evaluate("localStorage.setItem('itsv.layout.rowHeight', '1200')")
      page.reload(wait_until='domcontentloaded')
      page.wait_for_function("document.querySelector('[data-itsv-row-height]')?.getAttribute('data-itsv-row-height') === '1200'")
      check(page.locator('[data-itsv-row-height]').first.get_attribute('data-itsv-row-height') == '1200',
            'saved shared height of 1200 pixels restores correctly')
      # Re-enable automatic fitting after verifying that a manual height was
      # restored across a page reload.
      page.get_by_text('View', exact=True).first.click()
      page.get_by_text('Fit waveform heights to window', exact=True).click()
      page.wait_for_timeout(400)
      actual = int(page.locator('[data-itsv-row-height]').first.get_attribute('data-itsv-row-height'))
      count = int(page.locator('[data-itsv-row-count]').first.get_attribute('data-itsv-row-count'))
      viewport_height = page.locator('[data-itsv-waveform-scroll="1"]').evaluate('el => el.clientHeight')
      check(actual == max(56, min(1600, (viewport_height-64)//count)),
            'Fit waveform heights to window re-enables dynamic sizing')
      check(page.evaluate("localStorage.getItem('itsv.layout.autoFitRows')") == '1',
            'auto-fit preference saved in browser localStorage')
      browser.close()
      print('PASS: browser smoke complete')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://127.0.0.1:8081')
    parser.add_argument('--file', required=True, help='MiniSEED fixture (preferably 3+ channels)')
    parser.add_argument('--show', action='store_true', help='show browser during run')
    run(parser.parse_args())
