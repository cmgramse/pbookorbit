# To do

- Page buttons: floating buttons at the bottom left and right of the book list that scroll
  a whole page up or down, as in the system library (screenshots 9 and 10). Paging suits
  e-ink better than scrolling: one clean redraw per page.
- On-device detection only knows the books this app downloaded (`Downloads` records
  where each went). Books copied on some other way could be matched against the device's
  own library.
- Big audiobooks (1 GB+) might want a warning, or a check for free space, before they
  download. Check on the device that the audiobook player finds them in Audio Books.
- Views: Collections, Series and Authors in the footer still show the book list.
- Libraries: the list shows the first library (`loadLibrary()` in `bookorbitclient.cpp`).
  Choose one when the server has several.
- Jump rail: dragging along it, and jumping by date for the date sorts (the jump-buckets
  endpoint returns days or months for those).
- Make the server address configurable (`DEFAULT_SERVER_URL` in `bookorbitclient.cpp`).
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
