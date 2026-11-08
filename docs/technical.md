# How PBookOrbit works

Notes for working on the app: how it talks to BookOrbit, what it stores on the device,
and how it hooks into the PocketBook launcher. Build instructions are in the
[README](../README.md#development), and firmware and SDK findings are in
[NOTES.md](../NOTES.md).

## Signing in

The sign-in page asks for the server address (`books.example.com`, with https assumed
unless `http://` is typed), plus the same username and password as the BookOrbit website.
After a successful sign-in the server is saved and shown as a pill. Builds can prefill a server with `make release SERVER=...`.

Each device gets its own session, listed in the BookOrbit account as "PocketBook <model>".
The password isn't stored; the session's refresh token is, in the app's database. Signing
out ends the session on the server.

## Offline

The app counts as online until a request gets no reply: no connection, a refused one, or
a time-out. Wi-Fi turns itself off when idle, so before each request the app reconnects
it with `NetConnectSilent()`, and failing that it's offline. Only things the user starts
(signing in, a download) show the system's Wi-Fi dialog (`NetConnect2()`).

While offline, requests fail at once with "No connection" instead of being sent, and
covers wait. The app checks the server again when Wi-Fi has connected, which it looks
for (`QueryNetwork()`) when the user taps or presses a key, at most every 3 seconds, since the system's
`EVT_NET_CONNECTED` doesn't reach it (see NOTES.md) and polling would cost battery, or when
the user asks (`goOnline()`); any reply means online.
A session refresh that fails offline keeps the session.

Offline, a red badge with a crossed-out Wi-Fi sign follows the header's title, the header menu has
Reconnect (`goOnline()`), and every list shows downloaded
books only, with the switch for it turned on and disabled. Back online, each list goes
back to the user's own setting. Empty lists say so in the middle of the page, with a line
on why (`EmptyState.qml`), saying when it's because the app is offline.

QML hands the client the downloaded books (`setLocalBooks()`), and offline the Series and
Authors views, an author's series and search are built from them. Rows built this way have
negative ids, no author photos, and only the downloaded books' formats; back online they
count as not loaded, so each view fetches the server's rows when it next shows. Authors
already fetched that session are kept, filtered to downloaded books. Authors take their
id, sort name and photo from the `authors` table where they've been fetched before, and
Collections lists the saved collections that have downloaded books. Author and book
details show what was last saved. "Find books on this
device" is hidden offline, since it needs the server's library.

Set `PBOOKORBIT_OFFLINE=1` to run as though there were no network, for testing.

## Local data

Data lives in an SQLite database, `pbookorbit.db`, in the system config folder
(`/mnt/ext1/system/config` on the device, `build/pbookorbit/<sdk>/pc/system/config` in
the emulator). Set `PBOOKORBIT_DB` to use a different file, for example in tests. With
the device plugged in, `sqlite3 <mount>/system/config/pbookorbit.db` opens it from a PC.

- `settings` is key/value: `serverUrl`, `username`, `refreshToken`, `accent` (the
  BookOrbit accent colour), `recentSearches` (JSON array), `findBooksOffered` (0/1),
  `diagnosticsLogging` (0/1) and the chosen library for each account (see
  [Libraries and books](#libraries-and-books)).
- `books` caches BookOrbit books by id. `data` holds the book's list entry as JSON, so
  new fields don't need a schema change. `title` and `series` are generated columns for
  querying; more can be added with
  `ALTER TABLE books ADD COLUMN ... GENERATED ALWAYS AS (json_extract(data, '$.field'))`.
- `book_files` maps files on the device (`path`) to books (`book_id`), whether downloaded
  or found.

- `book_details` keeps those books' full details (`GET /books/:id`), saved when a book
  downloads and whenever its Book info page loads them.
- `authors` keeps BookOrbit authors by id, with the authors list's entry and their
  details (`GET /authors/:id`) as JSON, each saved when fetched.
- `collections` and `collection_books` keep the user's collections in order, and each
  one's book ids, newest first.

Only downloaded books are cached at the moment. Their entries are replaced whenever they
appear in a list fetched from the server, so status and progress stay current. Deleting a book's files leaves its cache
entry in place.

Covers and author photos are cached as PNGs in `/mnt/ext1/system/cache/pbookorbit-covers`,
named `<book id>` or `author-<author id>`, then a hash of the version and the size drawn.
Offline, a cover or photo not cached at the size wanted is scaled from the largest cached
copy, of whatever version, and saved under that version's name, so the current version is
still fetched once online.

## Downloads and existing books

Ebooks download to `/mnt/ext1/Books` and audiobooks to `/mnt/ext1/Audio Books`. After
each download the app asks the system to rescan, so the book appears in the system
library. Ebooks open in the system reader and audiobooks in the Audiobooks app
(`/ebrmain/bin/audio_books.app`).

"Find books on this device" in the header menu scans Books, Audio Books and Downloads for
files the app hasn't recorded (books downloaded with KOReader, for instance) and marks the
ones in the library as downloaded. It searches the library selected when it starts and
finishes there even if you switch, so books in another library need another run after
selecting it. A file matches a library book if, checked in order:

1. KOReader's BookOrbit plugin (if installed) lists it for this server in its state file
   (`bookorbit_sync_state*.lua` in `applications/koreader/settings`), and the file
   hasn't changed.
2. Its partial MD5 matches a library file. This is KOReader's hash
   (`util.partialMD5`), which BookOrbit stores for every file.
3. Its file name matches a library file's.

For 2 and 3, the candidates are books in the same format whose title appears in the file
name. A file that has candidates but matches none of them either way is kept as an unsure
match, the best candidate, such as an older copy of a book whose file BookOrbit has since
rewritten with new metadata, changing its hash and name. Unsure matches are only logged for
now: the dialog says how many books it matched, and they count as not matched until there's
a list to confirm them in (see TODO.md). See `src/core/bookfinder.h`.

## Home screen tile and task manager

On every start the app checks it has a PBookOrbit tile, with its own icon, on the
applications screen.

- The icons are built into the app from the SVGs in `src/icons` (`make icons`) and
  written to `applications/icons/pbookorbit.bmp` and `pbookorbit_f.bmp` (focused) if
  they're missing. Replace them there to change the tile, or delete them to get the
  defaults back on the next start. On the Era Color they need to be 110x110, 8-bit grey,
  or the tile won't line up.
- The app adds itself to the launcher's `system/config/desktop/view.json` as
  `U_pbookorbit`, at the end of the first group, after saving the original as
  `view.json.before-pbookorbit`. It only places the tile once, so moving or removing it
  on the device sticks.
- The launcher only reads `view.json` at boot, so the tile appears after a restart. The
  first time, the app explains this and offers to restart.
- The running task is given the same name and icon, so the task manager shows PBookOrbit
  instead of `pbookorbit`.

The code is in `src/ui/launchertile.cpp`. NOTES.md covers the `view.json` format and how
the task manager picks names and icons.

## Diagnostics log

`pbookorbit.log` is written to the root of the device's storage
(`build/pbookorbit/<sdk>/pc/system/mnt/ext1` in the emulator) and cleared on each start.
`diagnostics::log()` always writes, for errors and startup information.
`diagnostics::trace()` only writes while diagnostics logging is on (tap the title six
times). It records each request with status and timing, Wi-Fi connections and changes in
the network state (`QueryNetwork()`, checked every 2 seconds), the book list switching A2
on and off while scrolling (see `src/ui/eink.h`), and every 10 seconds the frames rendered,
memory use (`VmRSS`), thread count and network state.

Every frame on e-ink is a slow screen update, so `frames` should be 0 when the app is
idle. Anything that animates (a blinking cursor, `BusyIndicator`, transitions, Flickable
momentum) keeps the display busy and makes the whole device feel frozen.

## The BookOrbit API

BookOrbit's API isn't documented. This comes from its source (`server/src/modules/`,
types in `packages/types/src/`), its web client (`client/src`), and testing against a
server.

- Base URL: `<server>/api/v1`
- Errors are in NestJS format: `{"statusCode": 401, "message": "Invalid credentials"}`,
  where `message` may also be an array.

### Sessions

`POST /auth/login` with `{"username", "password", "clientKind": "native", "deviceLabel"}`
returns `NativeCredentials` and the user:

```json
{"accessToken": "...", "accessTokenExpiresAt": "...", "refreshToken": "<64 hex>",
 "refreshTokenExpiresAt": "...", "sessionId": 17, "user": {...}}
```

`clientKind: "native"` returns the refresh token in the body. Without it the server sets
browser cookies instead.

- Access tokens last about 15 minutes. Send them as `Authorization: Bearer <accessToken>`.
- Refresh tokens last 7 days. `POST /auth/refresh` with `{"refreshToken"}` returns new
  `NativeCredentials` with a new refresh token, and the old one stops working shortly
  after. Always save the new one, and never run two refreshes at once.
- `POST /auth/logout` with `{"refreshToken"}` ends the session.
- Login is limited to 5 attempts a minute.

On a 401 the client refreshes and retries the request once.

### Libraries and books

- `GET /libraries` returns the user's libraries, `[{id, name, bookCount, ...}]`. With more
  than one, the header menu lets you choose. The choice is saved per server and username
  (setting `selectedLibrary|<server>|<lowercased username>`), and stays when you sign out.
  If it's no longer listed, the app says so and uses the first. Only this list shows a
  library is usable: books return 403 for one you can't access, series and authors an
  empty list.
- Each library has its own list caches, named `library:<id>`, `series:<id>`,
  `authors:<id>` and `genres:<id>`. Collections belong to the account, so they stay as they are when you
  switch. Their books can be in any library you can access; merging an ebook and audiobook
  pair only looks in the selected one.
- The downloaded lists cover the whole device, whichever library is selected: offline,
  and with "Downloaded, all libraries" on. Authors with "Downloaded only" on is the
  exception online, showing the selected library's authors who have books on the device.
- `POST /libraries/:id/books` takes a `BookQuery` (`packages/types/src/query.ts`) and
  returns a page of books:

  ```json
  {"sort": [{"field": "addedAt", "dir": "desc"}], "pagination": {"page": 1, "size": 20},
   "q": "optional search text", "filter": {"optional": "GroupRule"}}
  ```

  The response is `{items, total, page, size}`. Items include `id`, `title`, `authors`,
  `seriesName`, `seriesIndex`, `files: [{id, format, role, sizeBytes}]`, `hasCover`,
  `readingProgress` and `readStatus`. Sort fields are listed in `StaticSortField`.
- `POST /libraries/:id/books/jump-buckets` takes the same query and returns the A-Z rail's
  sections.
- `GET /books/:id` returns a single book in full, including each file's `filename` and
  `fileHash`.
- `GET /books/:id/thumbnail` returns the cover.
- `GET /books/files/:fileId/download` downloads a file.

### Collections

- `GET /collections` returns the user's collections and other users' public ones, in the
  user's order: `[{id, mediaType, name, icon, displayOrder, bookCount, ...}]`.
- `POST /collections/:id/books/query` and `.../jump-buckets` work like the library
  equivalents, with an extra `collectionOrder` sort.

### Authors and series

- `GET /authors?libraryId=&sort=name&order=asc&page=&size=` lists a library's authors.
- `GET /authors/:id` returns one author, including `name`, `sortName`, `description`
  (plain text), `bookCount`, `birthDate`/`birthYear`, `deathDate`/`deathYear`,
  `website`, `genres` and `imageUrl`.
- `GET /series?libraryId=&sort=name&order=asc&page=&size=` lists a library's series, each
  with `coverBookIds` for its covers.
- `GET /series?libraryId=&author=<text>` lists series with any author whose name contains
  the text, ignoring case. `author=Martin` matches every Martin, so the client filters to
  series whose `authors` include the exact name.

### Genres

- The server has no list of a library's genres: `GET /metadata/genres?q=` needs a search
  term and covers every library, and `GET /statistics/genre-distribution` stops at 25. So
  the Genres tab pages `POST /libraries/:id/books` with the filter
  `{"type": "rule", "field": "genre", "operator": "isNotEmpty"}`, sorted by title, 200 books
  a page, and gathers each item's `genres` (exact names, so "Sci-Fi" and "sci-fi" are two
  genres). An ebook and its audiobook count as one book.
- The list is cached as `genres:<id>` only once every page has loaded (`total` reached). A
  page that fails, isn't `{items: [...], total: n}`, or comes back empty before the total
  leaves the genres incomplete: the pages that loaded show with a note, or the last complete
  list stays. Tapping Genres again, opening the tab, a library switch or going back online
  loads them again.
- A genre's books are the library's matching `{"field": "genre", "operator": "includesAny",
  "value": [name]}`. Offline, the genres are those of the downloaded books saved with a
  `genres` list; books saved before then show under no genre until they're refreshed
  online.

### Preferences

- `GET /user-preferences/theme` returns `{settings: {theme, accent, ...}}`, or
  `{settings: null}` if the user has never synced their theme. The app reads the accent
  colour from it.
