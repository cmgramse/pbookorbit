# To do

- Page buttons: floating buttons at the bottom left and right of the book list that scroll
  a whole page up or down, as in the system library (screenshots 9 and 10). Paging suits
  e-ink better than scrolling: one clean redraw per page.
- On-device detection: rows show a random answer for now (`decorateBook()` in
  `bookorbitclient.cpp`). Record the books the app downloads, and possibly match the
  device's own library.
- Views: Collections, Series and Authors in the footer still show the book list.
- A–Z index for the letter sorts, to jump to a letter.
- Search: run searches and keep real recent searches (sample terms for now).
- Make the server address configurable (`DEFAULT_SERVER_URL` in `bookorbitclient.cpp`).
- Test data for reading status: the `app` test account has no reading progress or
  statuses, so "Completed" and "N% read" can't be seen in the emulator. Add a mock, e.g. an
  environment variable that makes `decorateBook()` give books sample progress and statuses.
