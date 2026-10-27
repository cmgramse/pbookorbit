# How PBookOrbit works

Technical notes for working on the app: how it talks to BookOrbit, what it keeps on the
device, and how it fits into the PocketBook's launcher. For building it, see the
[README](../README.md#development). Findings about the PocketBook firmware and SDK are in
[NOTES.md](../NOTES.md).

## Signing in

The sign-in page takes the server's address (`books.example.com`; https is assumed unless
`http://` is typed) and the same username and password as the BookOrbit website. Once a
sign-in has worked, the server is saved and shown as a pill; its cross forgets it, to
sign in to another. A build can suggest a server (`make release SERVER=...`).

The app opens a session for the device, which BookOrbit lists in the account's sessions
as "PocketBook <model>". The password is not saved: the session's refresh token is, in the
app's database. Signing out ends the session on the server.

## Local data

The app keeps its data in an SQLite database, `pbookorbit.db`, in the system config folder:
`/mnt/ext1/system/config` on the device, `build/pbookorbit/<sdk>/pc/system/config` in the
emulator. `PBOOKORBIT_DB` points the app at a different file, e.g. for automated tests.
With the device on USB, `sqlite3 <mount>/system/config/pbookorbit.db` opens it on a PC.

- `settings`: key/value. `serverUrl`, `username`, `refreshToken`, `accent` (the account's
  BookOrbit accent colour), `recentSearches` (a JSON array), `findBooksOffered` (0/1) and
  `diagnosticsLogging` (0/1).
- `books`: a local cache of BookOrbit books by id. `data` is the book's list entry as JSON,
  so what's kept can grow without changing the table. `title` and `series` are generated
  columns pulled out of it for querying; add more the same way with
  `ALTER TABLE books ADD COLUMN ... GENERATED ALWAYS AS (json_extract(data, '$.field'))`.
- `book_files`: the books' files on the device (`path`, `book_id`), downloaded or found.

Only the downloaded books are cached so far. Deleting a book's files keeps its cache entry.

Covers and authors' photos are cached as PNGs in `/mnt/ext1/system/cache/pbookorbit-covers`.

## Downloads and books already on the device

Ebooks download to `/mnt/ext1/Books` and audiobooks to `/mnt/ext1/Audio Books`, after
which the app asks the system to scan for new books so they show in the system library.
Ebooks open in the system reader; audiobooks play in the system Audiobooks app
(`/ebrmain/bin/audio_books.app`).

**Find books on this device** (in the header menu) looks through the Books, Audio Books
and Downloads folders for files the app hasn't recorded, such as books downloaded with
KOReader, and records the ones in the library as downloaded. A file is a library book
when, in order:

1. KOReader's BookOrbit plugin says so: its state file (`bookorbit_sync_state*.lua` in
   `applications/koreader/settings`) lists the book and file ids of every file it
   downloaded or matched, for this server, and the file is unchanged.
2. Its partial MD5 matches a library file's. This is KOReader's hash
   (`util.partialMD5`), which BookOrbit stores for each file.
3. Its name is a library file's name.

For 2 and 3, the candidates are the library books whose title is in the file's name with
the same format. A file whose name has a book's title and author but matched neither way
is kept as unsure, for the user to add or not. See `src/core/bookfinder.h`.

## Home screen tile and task manager

Each time it starts, the app makes sure it has a tile in the applications, named
PBookOrbit with its own icon.

- The icons are built into the app (`src/icons`, rendered from the `.svg` files there by
  `make icons`) and written to `applications/icons/pbookorbit.bmp` and `pbookorbit_f.bmp`
  (the focused one) on the device, only if they aren't there: replace them there to
  change the tile, or delete them to get the app's own back at the next start. Keep them
  110x110, 8-bit grey (on the Era Color), or the tile won't line up.
- The app adds itself to the launcher's `system/config/desktop/view.json` as `U_pbookorbit`,
  at the end of the first group of tiles, keeping a copy of the file as it was
  (`view.json.before-pbookorbit`). Moving or removing the tile on the device sticks: the
  app only places it the first time.
- The launcher reads `view.json` at boot, so the tile shows after a restart. The first
  time, the app says so and offers to restart the device.
- The app also gives its running task the tile's name and icon, so the task manager
  shows PBookOrbit rather than `pbookorbit`.

The code is `src/ui/launchertile.cpp`. NOTES.md has the format of `view.json` and how the
task manager gets its names and icons.

## Diagnostics log

The app writes `pbookorbit.log` to the root of the device's USB storage
(`build/pbookorbit/<sdk>/pc/system/mnt/ext1` in the emulator), cleared each time the app
starts. `diagnostics::log()` always writes, for errors and what the app found out at
startup. `diagnostics::trace()` writes only while diagnostics logging is on (tap the title
six times), for detail: every request with its status and time, every Wi-Fi connect, each
time the book list switches A2 on or off while it scrolls (see `src/ui/eink.h`), and every
10 seconds the frames rendered, memory use (`VmRSS`) and thread count.

On e-ink every frame is a slow screen update, so `frames` should be 0 while the app is idle.
Anything that animates (a blinking cursor, `BusyIndicator`, transitions, Flickable momentum)
keeps the display busy and makes the whole device feel hung.

## The BookOrbit API

BookOrbit doesn't document its API. What's below comes from its source
(`server/src/modules/`, with types in `packages/types/src/`) and its web client's calls
(`client/src`), or from trying the endpoints against a server.

- Base URL: `<server>/api/v1`
- Errors use NestJS's format: `{"statusCode": 401, "message": "Invalid credentials"}`.
  `message` can also be an array.

### Sessions

`POST /auth/login` with `{"username", "password", "clientKind": "native", "deviceLabel"}`
returns `NativeCredentials` and the user:

```json
{"accessToken": "...", "accessTokenExpiresAt": "...", "refreshToken": "<64 hex>",
 "refreshTokenExpiresAt": "...", "sessionId": 17, "user": {...}}
```

`clientKind: "native"` puts the refresh token in the body. Without it, the server sets
browser cookies instead.

- The access token lasts about 15 minutes. Send it as `Authorization: Bearer <accessToken>`.
- The refresh token lasts 7 days. `POST /auth/refresh` with `{"refreshToken"}` returns new
  `NativeCredentials`, including a **new** refresh token. The old one stops working shortly
  after, so always save the new one, and never run two refreshes at once.
- `POST /auth/logout` with `{"refreshToken"}` ends the session.
- Login is rate-limited to 5 attempts a minute.

The client refreshes when a request gets a 401, then retries the request once.

### Libraries and books

- `GET /libraries` lists the user's libraries, `[{id, name, bookCount, ...}]`. The app
  uses the first.
- `POST /libraries/:id/books` with a `BookQuery` (`packages/types/src/query.ts`) returns
  a page of the library's books:

  ```json
  {"sort": [{"field": "addedAt", "dir": "desc"}], "pagination": {"page": 1, "size": 20},
   "q": "optional search text", "filter": {"optional": "GroupRule"}}
  ```

  It returns `{items, total, page, size}`. Each item has `id`, `title`, `authors`,
  `seriesName`, `seriesIndex`, `files: [{id, format, role, sizeBytes}]`, `hasCover`,
  `readingProgress`, `readStatus` and more. The sort fields are listed in
  `StaticSortField`.
- `POST /libraries/:id/books/jump-buckets` with the same query gives the sections the
  A–Z rail jumps to.
- `GET /books/:id` returns one book in full, including its files' `filename` and
  `fileHash`.
- `GET /books/:id/thumbnail` is the book's cover.
- `GET /books/files/:fileId/download` downloads one of a book's files.

### Collections

- `GET /collections` lists the user's own collections and others' public ones, in the
  user's order: `[{id, mediaType, name, icon, displayOrder, bookCount, ...}]`.
- `POST /collections/:id/books/query` and `.../jump-buckets` work like the library's
  book query, for a collection's books (with a `collectionOrder` sort).

### Authors and series

- `GET /authors?libraryId=&sort=name&order=asc&page=&size=` lists a library's authors.
- `GET /authors/:id` returns one author: `name`, `sortName`, `description` (plain text),
  `bookCount`, `birthDate`/`birthYear`, `deathDate`/`deathYear`, `website`, `genres`,
  `imageUrl` and more.
- `GET /series?libraryId=&sort=name&order=asc&page=&size=` lists a library's series, each
  with `coverBookIds` for its covers.
- `GET /series?libraryId=&author=<text>` lists the series with an author whose name
  contains the text, ignoring case. `author=Martin` finds every Martin's series, so the
  client keeps only those whose `authors` include the exact name.

### Preferences

- `GET /user-preferences/theme` returns `{settings: {theme, accent, ...}}`, or
  `{settings: null}` for a user who has never synced their theme to the server. The app
  takes the accent colour from it.
