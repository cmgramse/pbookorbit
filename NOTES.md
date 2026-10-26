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
KOReader adds `@koreader=KOReader` to `/mnt/ext1/system/language/en.txt`. The SDK's
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
  ("vivid" or "pastel"); `accents.cpp` has the light-theme values, converted to sRGB.
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
- `einkcolors.cpp` uses it for covers and, averaged over a patch, for the accent colour.

Before the probe, the transform was fitted from screenshots alone: HSV saturation
S' = 1 - (1 - S)^5 matched the dark covers but not the bright one; saturating around
Rec. 709 luma by 3 matched both better, but not as well as the real function.
