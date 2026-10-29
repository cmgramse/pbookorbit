# Notes

Findings worth keeping, about the device and SDK, that the code alone doesn't explain.

## Getting downloaded books into the system library

The SDK has no documented call to add a book to the system library. What we found
(October 2026, firmware 6.10 SDK):

- **`BookReady(path)`** (inkview.h, no documentation) is for reader apps, not for
  libraries. Disassembled, it writes the path to `/tmp/.current` (the book currently open),
  sets the calling task's subtask info, and sends an event to `bookshelf.app`.
  `BookPreparing(path)` only sets the subtask info. inkshelf found that on firmware 6.11+
  `BookReady` alone makes a new book appear in the library, while on 6.8 it didn't until a
  reboot; 6.9 and 6.10 untested.
- **Scan events** in inkview.h: `EVT_STARTSCAN` (215), `EVT_SCANSTOPPED` (216),
  `EVT_SCANPROGRESS` (213), `EVT_STOPSCAN` (214), and `USERSCANNER` / `SYSTEMSCANNER`
  (`scanner.app`).
- **`reader_db_manager.h`** has `addBook(...)`, writing the reader's database directly.
  Avoided: internal, and easy to get wrong.

What other apps do:

1. **Ask the resident scanner to scan** ([pbweb](https://github.com/UndeRus/pbweb)):
   `SendEventTo(-3, EVT_STARTSCAN, 0, 0)`, said to be what the firmware itself does
   (`PBScanClient::startDeviceScan()`). The scanner answers with `EVT_SCANSTOPPED`.
   What the SDK confirms: -3 is inkview.h's `OTHERTASKS` (every task but the caller;
   `ALLTASKS` is -1), and 0xD7 is `EVT_STARTSCAN` (215). `SendEventTo` checks that
   multitasking is on and hands over to `hw_task_sendevent`; nothing in libinkview sends
   `EVT_STARTSCAN` itself. What it can't confirm: that the firmware's scanner listens for
   it. `PBScanClient` isn't in the SDK; it's in the firmware's own apps and libraries
   (`/ebrmain/...` on the device), which would need copying off the device to check.
   pbweb doesn't send `EVT_STOPSCAN` (the firmware answers "Not Implemented"). Some
   devices (Pro 903) have no scanner service.
2. **Start `scanner.app` itself**
   ([inkshelf #87](https://github.com/N-Combinator/inkshelf/pull/87),
   [#90](https://github.com/N-Combinator/inkshelf/pull/90)): from a background process
   that outlives the app, grouping close downloads into one scan and stopping the scanner
   after 2 minutes. Shows the scanner's progress screen. Skipped on 6.11+ (by
   `GetSoftwareVersion()`), where `BookReady` is enough.
3. **Rely on `BookReady`**: enough on 6.11+ only, per inkshelf.

**We use option 1**: `Downloads::requestLibraryScan()` after each saved download, and a
handler on the e-ink platform's InkView events logs "library scan finished" when
`EVT_SCANSTOPPED` arrives. In the emulator `SendEventTo` returns -1 (no scanner there).

**On the device (Era Color, 6.10, 24 October 2026)**: a book downloaded and opened by the
app appeared in the system library, added at the time it opened. That run's log was lost,
so whether the scan or the opening added it isn't known yet: a download that isn't opened
would tell. If the scan alone doesn't, option 2 is the fallback.

## Wi-Fi

Logged on the Era Color (6.10, 29 October 2026), turning Wi-Fi off and on from the status
bar, connecting with `NetConnect2`, and leaving Wi-Fi to switch itself off when idle:

- **`EVT_NET_CONNECTED` and `EVT_NET_DISCONNECTED` never reach the app** (through the e-ink
  platform's InkView event handler), though other system events do (217, 220, and 271,
  `EVT_TIME_CHANGED`, several times after each connection).
- **`QueryNetwork()` follows Wi-Fi**: `0x0203` (`NET_WIFIREADY` set) while connected,
  `0x0003` while not, whether it was turned off or went idle. So the app looks at it
  when the user taps or presses a key while offline, rather than polling.
- `NetConnectSilent()` returns -12 at once when Wi-Fi is off, and connects in about 4 s
  when it's on but idle. `NetConnect2()` (the system's dialog) took 7-8 s.
- `NetConnectSilent()` blocks its thread for those seconds. Called from a worker thread
  (`QThread::create`) it works on the Era Color (6.10), and the UI draws and scrolls
  meanwhile.

## Opening books

`OpenBook(path, "", OB_ADDTOLAST | OB_WITHRETURN)` opens a book in whatever app the
device associates with it, not necessarily the system reader: on the Era Color EPUBs went
to KOReader (its `.sdr` folder appeared next to the book). The associations are in
`/mnt/ext1/system/config/handlers.cfg` (per file, `path:app`). The app stayed running in
the background while the reader was open.

`OpenBook` picks the app by extension from `extensions.cfg` (the firmware's
`/ebrmain/config/extensions.cfg`, with additions in `/mnt/ext1/system/config/`, where
KOReader adds its `epub` line). On 6.10 it lists `mp3` and `m3u` for `music_player.app`
and no audiobook formats, so an `.m4b` gets "Unknown book type", while `OpenBook` still
returns 0.

Open with (the library's book menu, screenshot 16) lists every app on the format's
`extensions.cfg` line, in order: on the Era Color `epub:@EPUB_file:1:koreader.app,
eink-reader_with_blink.app,eink-reader_with_epub2.app:ICON_EPUB` (KOReader's line).
InkView's `GetSupportedFileTypes()` returns these lines with `program` as that
comma-separated list; the emulator reads its one `extensions.cfg` twice, so lines repeat.
The names are language strings named after the app, `@eink-reader_with_blink` and so on;
KOReader has none in the firmware's: the Era Color's user language file,
`/mnt/ext1/system/language/en.txt`, has `@koreader=KOReader` only because the user added
it by hand, and without it the app is listed as `koreader`. The SDK's
emulator language file predates the blink and epub2 readers, so the emulator shows the
apps' file names. Choosing an app records it for that file in `handlers.cfg`
(`SetFileHandler`), so `OpenBook` and the system library open it there from then on.

Audiobooks play in the system's Audiobooks app, `/ebrmain/bin/audio_books.app`. Its
command line (`DataManager::parseArguments`, a `QCommandLineParser`) takes:

- `--bookPlay <path>`: plays that book. The app uses this.
- `--bookPath <path>` with `--bookTitle` and `--bookAuthor` (base64): seems to be for
  books from the PocketBook cloud.
- plain paths: added to its library.

`OpenTask(path, argc, argv, TASK_MAKEACTIVE)` starts it with `path` put in front of
`argv`, or, if it is already running, sends the arguments to it and brings it to the
front. Both work on the Era Color (25 October 2026): `OpenTask` returns 0 when it started
the app, 1 when the app was already running and switched to the new book.

## The home screen tile

Apps in `/mnt/ext1/applications` get no tile of their own: the launcher's tiles come from
`/mnt/ext1/system/config/desktop/view.json`, on the user's storage. The firmware documents
the format in `_comment` arrays inside the file itself (read on the Era Color, 6.10,
27 October 2026):

- `applications` maps a key to `{path, title, icon, focused_icon}` (also `param`, the
  app's arguments, and `visible`). User apps' keys must start with `U_`; the firmware's
  are `PB_...`. `path` is the `.app` and is required for user apps. An icon is either a
  theme icon's name or an "object path", `{"path": "<path on user space>"}`.
- `view.groups[]` are the groups of tiles (`@General`, `@Services`, `@Games`), each with a
  `title`, an `apps` list of keys and a `sort` (`title` by default, or `fixed`).
- KOReader's entry, put there by hand, gives absolute paths as plain strings for its icons,
  which works as well.
- `hash.txt` beside it has `hash2`, the MD5 of the current view.json (`hash1` is likely
  the firmware's own). After the app first edited the file, the restart set hash2 to the
  new file's MD5 and kept the edit, so the hash needs no attention.
- `apps.json` beside it is the home screen's row of shortcuts (`{"id": ...}` per app,
  `SYS_Apps` for the applications list). The user picks those: the app leaves it alone.
- Icons: KOReader's for the Era Color are 106x110, 8-bit uncompressed BMP; its focused
  one is a 110 px black disc cut to 106 wide, with the drawing in white. The theme's own
  icons (`desktop_app_*`, `ctrl_panel_*`) are 110x110, and so are the app's (`src/icons`,
  rendered from the `.svg` beside them by `make icons`). pocketbook-statistics found 106x64 on a PB629,
  and that an icon of the wrong size isn't shown at all (48 px wide) or puts the label
  out of line with its neighbours' (128 px tall).
- The launcher reads the file at boot, so a new tile shows after a restart. The app can
  restart the device as KOReader does (its `frontend/device/pocketbook/device.lua`):
  `iv_ipc_request(MSG_REBOOT, 1, nullptr, 0, 0)`. Confirmed on the Era Color. Confirmed on
  the Era Color: the tile appeared and opened the app.

## The task manager

The task manager names a task with `TaskNameToHumanName(appname)`: the program's name
without its folder and extension (`pbookorbit`), looked up as `@pbookorbit` in the
language files, or left as it is. KOReader shows as KOReader on the Era Color only because
the user added `@koreader=KOReader` to the language file on its storage
(`/mnt/ext1/system/language/en.txt`); the firmware's `en.txt` has no such entry. The
built-in apps' icons there are theme
icons, `ctrl_panel_<app>` (110x110, e.g. `ctrl_panel_sudoku`), and any other app gets
`ctrl_panel_app_unknown`.

A task can set its own name and icon: `SetTaskParameters(task, appname, name, icon,
flags)` (`TASK_DONTCHANGE` keeps the flags) and `SetSubtaskInfo(task, subtask, name,
book)`, which KOReader calls with the open book's title. The work is done by the
firmware's `hw_task_setparameters`, which isn't in the SDK, so whether a NULL appname
means "unchanged" is unknown: the app passes its own appname back, from `GetTaskInfo`.
`src/ui/launchertile.cpp` makes both calls at startup with the tile's title and icon.
Confirmed on the Era Color: the name shows.

The icon has to be 8-bit grey, as the theme's are (`GetResource` gives depth 8, rows
padded to 4 bytes). The message carries the `ibitmap` as it is (header, then height x
scanline bytes), and the task manager draws whatever comes as 8-bit: InkView's
`LoadBitmap` turns an 8-bit BMP with a palette into a 24-bit bitmap, which came out as
three squashed copies of the picture side by side. The app draws the icon into
`NewBitmap8` itself.

## InkView dialogs from Qt

InkView's `Dialog`, `DialogSynchro`, `Message` and progress bar don't work in a Qt app,
tested in the emulator: `DialogSynchro` froze the app and drew nothing; `Dialog` drew over
a stale screen buffer, without its title, and its buttons didn't respond. They belong to
InkView's own event loop and canvas, which Qt replaces. Dialogs are drawn in QML instead,
after the native look (`DownloadDialog.qml`).

## The user's accent colour

BookOrbit's appearance settings (theme, accent, corner radius, background) live in the
browser unless the user turns on syncing them to their account:

- `GET /auth/me` → `settings.syncThemePreferences`: true while they sync.
- `GET /user-preferences/theme` → `{settings: {theme, accent, radius, background,
  brightness}}`, or `{settings: null}` if they never synced. **After syncing is turned
  off the server still returns the last synced theme**, so check `syncThemePreferences`
  first. The settings can't be cleared (`PUT` rejects null, no `DELETE`).
- `accent` is a palette name ("teal" by default, "wisteria", ...). The web client defines
  each as OKLCH lightness/chroma for its light and dark themes plus a hue, and a tone
  ("vivid" or "pastel"); `src/ui/accents.cpp` has the light-theme values, converted to sRGB.
  Pastel accents take dark text.

The app uses the accent for the on-device format pills, falling back to teal. It reads
`/user-preferences/theme` only, so a user who stopped syncing gets their last synced
accent; that's fine for a highlight colour. The test
account's synced theme was set to wisteria to try this, then back to the defaults
(system, teal) with syncing off.

## Colours on the colour e-ink screen

The Era Color's screen (Kaleido-style colour e-ink) shows colours much paler than an LCD,
and the system library compensates: its covers are far more saturated and brighter than
the same images drawn plainly, and dithered (a visible fine pattern).

**It is InkView's `adjustAreaDefault()`** (or `adjustBitmapDefault()`, the same thing on
an ibitmap). Tested on the device (October 2026) with a throwaway probe app that ran
InkView's adjustments on the original pixels of covers and saved the results, compared
with the system library's screenshots of the same covers (blurred and lined up, RMS of
255):

| | Martin covers, list | Doors of Eden, list | Doors of Eden, Book info |
|---|---|---|---|
| unchanged | 19.1 | 45.2 | 49.0 |
| `adjustAreaDefault`, 24-bit RGB | **5.1** | **7.9** | **19.0** |
| `adjustAreaDefault`, BGR | 9.5 | 14.1 | 25.8 |
| `adjustAreaWithLabColorsSaturation`, 3.0 | 9.7 | 12.9 | 22.6 |
| fitted formula, c' = Y + 3 (c - Y) | 6.8 | 18.0 | 25.2 |

(The rest of the difference is mostly the source: the library draws the EPUB's own cover,
the app BookOrbit's thumbnail.) Details:

- The data is 24-bit RGB rows, with any scanline. The output is dithered and reduced to a
  limited palette, so the app caches adjusted covers as PNG at the size they are drawn.
- `getBrightnessContrastGammaAdjustParams()` reports brightness 100, contrast 100, gamma
  120 on the device and in the emulator alike.
- **In the emulator `adjustAreaDefault()` hardly changes anything**, so colours there
  look like the plain image; judge them on the device.
- `src/ui/einkcolors.cpp` uses it for covers and, averaged over a patch, for the accent colour.

Before the probe, the transform was fitted from screenshots alone: HSV saturation
S' = 1 - (1 - S)^5 matched the dark covers but not the bright one; saturating around
Rec. 709 luma by 3 matched both better, but not as well as the real function.

## Compiled QML

`CONFIG += qtquickcompiler` has qmlcachegen compile every QML and JS file in `qml.qrc` to
bytecode at build time, linked into the app, so the device doesn't parse or compile QML
at startup and there's no QML disk cache to manage. Checked in the emulator (October
2026, headless, empty cache directory): a normal run wrote no `.qmlc`/`.jsc` files, while
`QML_DISK_CACHE=qmlc-read,qmlc-write` (which ignores the compiled-in units) compiled from
source and wrote 38.

**The units only work on the exact Qt they were built for**: their header records Qt
6.8.2 (the SDK sysroot), and the engine rejects them on any other version, silently
compiling from source on every launch instead. The app uses the firmware's Qt, so the
log's "Qt 6.8.2, QML compiled for 6.8.2" line (after "started") says whether they match;
it ends "mismatch, compiling QML at startup" when they don't.

**Few bindings are compiled to C++**: 35 functions in all, about one per file; the rest
run as bytecode. `qmlcachegen --verbose` (the build's command plus `--verbose`) gives 950
warnings. The main causes:

| Cause | Count | Example |
|---|---|---|
| Calls into `import "units.js" as Units` | ~195 | `Units.dp(15)` almost everywhere |
| Context properties from `main.cpp`, invisible to the compiler | ~125 | `bookOrbit` (75), `eink` (18), `downloads`, `bookFinder`, `covers` |
| `property var` data from C++ | 71 | `BookRow.book` (15), `GroupRow.group`, `AuthorInfoPage.author` |
| Ids from an outer file or component | ~60 | `page` (25), `root`, `bookMenu`, `row` |
| Functions without type annotations | 31 | `show(authorId)`, `dateText(date, year)` |
| Delegates' implicit `modelData` / `index` | ~10 | |
| QtQuick.Controls' `TextField` not resolved | ~25 | `serverField`, `searchField` |
| `try`/`catch` (not supported) | 11 | |

Fixing them, cheapest first: type annotations on functions; `required property` in
delegates and `pragma ComponentBehavior: Bound`; typed units (a singleton instead of the
JS library); C++ singletons (`QML_SINGLETON`) instead of context properties, which needs
type information for qmlcachegen (`CONFIG += qmltypes` in qmake, or CMake's
`qt_add_qml_module`); typed book/group/author objects instead of `var`. Most of these
bindings run once when an item is created (layout maths), so the likely gain is in the
list delegates (`BookRow`, `GroupRow`), created while scrolling.
