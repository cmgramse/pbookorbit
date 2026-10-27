# To do

- On-device detection only knows the books this app downloaded (`Downloads` records
  where each went). Books copied on some other way could be matched against the device's
  own library.
- Big audiobooks (1 GB+) might want a warning, or a check for free space, before they
  download. Check on the device that the audiobook player finds them in Audio Books.
- Views: Authors in the footer still shows the book list.
- Cache what the views fetch, so they open at once and work offline: the collections and
  series lists, the covers, authors, formats and book ids gathered for each (a request per
  collection, and one per 20 series, on every load), and perhaps book list pages. Keep it
  on the device beside the covers, show it straight away, and refresh it in the background
  when online. The series list is only fetched once a session for now, so changes on the
  server don't show until the app restarts; the collections list is fetched every time its
  tab opens. Series covers have no version in the list, so a changed one isn't fetched
  again.
- Libraries: the list shows the first library (`loadLibrary()` in `src/core/bookorbitclient.cpp`).
  Choose one when the server has several.
- Jump rail: dragging along it, and jumping by date for the date sorts (the jump-buckets
  endpoint returns days or months for those).
- Make the server address configurable (`DEFAULT_SERVER_URL` in `src/core/bookorbitclient.cpp`).
- Test data for reading status: the `app` test account has no reading progress or
  statuses, so "Completed" and "N% read" can't be seen in the emulator. Add a mock, e.g. an
  environment variable that makes `decorateBook()` give books sample progress and statuses.
- Downloaded books list from what was saved at download time, so status and percentage
  read can go stale: refresh them from the server in the background when online.
- The list entries have no lastReadAt, so the Recently read sort shows every date section
  as "Never" (in the downloaded list it also can't order by it). Find where BookOrbit
  keeps the last read date.
- Offline start: the app still opens on the full library, which needs the server. It
  could open on the downloaded books when there is no network.
- Check on the device that downloaded books show in the system library without a reboot
  (the app asks the scanner to scan; see NOTES.md for what else could be done).
- Books with an ebook and an audiobook: once one version is downloaded, a tap opens it;
  add a way to get or open the other version.
- Search results don't merge an ebook and its audiobook kept as separate records (a search
  can match only one of them, e.g. by narrator); both rows show both formats instead.
- Book menu: the system's menu also has Mark as read, Add to collection, Add to Favorites,
  Select and Open with; the Book info page has a favourite heart and CHOOSE ACTION.
- Deleting a book asks the scanner to scan again: check on the device that the book also
  leaves the system library. KOReader's `.sdr` folder beside the book is left in place.
- Once the app's UI is settled, delete the device screenshots in the repo root
  (`scr0001.bmp`...) and their entry in `.gitignore`. Code comments refer to them by number,
  e.g. "screenshot 9".
- Home screen tile: check it in night mode (the icons are black on white). Other devices
  will want other icon sizes (106x64 on a PB629).
- Check the log's "Qt ..., QML compiled for ..." line on the device: if the firmware's Qt
  isn't 6.8.2 the compiled QML is ignored and compiled on every launch (see NOTES.md,
  Compiled QML).
- Compiled QML: most bindings run as bytecode, not C++ (NOTES.md has why). Time how long
  the first book list takes to fill on the device; if it's slow, start with `BookRow` and
  `GroupRow` (typed functions, required properties, then typed units and singletons).
