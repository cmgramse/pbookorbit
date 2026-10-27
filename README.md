# PBookOrbit

PocketBook client for a [BookOrbit](https://github.com/bookorbit/bookorbit) server. It signs
in with a normal BookOrbit account and lists the 20 most recently added books.

It builds with [pocketbook-kit](kit), which runs the PocketBook SDK in a
container, so the host needs only `git`, `make` and podman or docker:

```sh
git clone --recurse-submodules <this repo> pbookorbit
cd pbookorbit
make setup    # one-off: the kit's container image and SDK download
make run      # emulator
make arm      # device -> build/pbookorbit/<sdk>/arm/pbookorbit.app
```

The kit is pinned as a submodule in `kit/`. See its README for choosing the SDK (`kit/pb.sh
use B300-6.10` for B300 devices such as the Era Color), making the emulator look like your
device, and headless tests (`kit/pb.sh test . <script>`).

To hand the app to someone who uses a particular server, build it with that server filled
in (it can still be deleted or changed):

```sh
make release SERVER=books.example.com
```

The next build without it is back to an empty field.

## Signing in

Enter the server's address (`books.example.com`; https is assumed unless `http://` is
typed) and the same username and password as the BookOrbit website. Once a sign-in has
worked the server is saved and shown as a pill; its cross forgets it, to sign in to
another. The app opens a session for the device, shown as "PocketBook <model>" in the
account's session list. The password is not saved. The session's refresh token is saved
in the app's database (see below). Signing out revokes the session on the server.

## Local data

The app keeps its data in an SQLite database, `pbookorbit.db`, in the system config folder:
`/mnt/ext1/system/config` on the device, `build/pbookorbit/<sdk>/pc/system/config` in the
emulator. `PBOOKORBIT_DB` points the app at a different file, e.g. for automated tests.

- `settings`: key/value. `serverUrl`, `username`, `refreshToken`, `accent`, `recentSearches` (a JSON
  array) and `findBooksOffered` (0/1).
- `books`: a local cache of BookOrbit books by id. `data` is the book's list entry as JSON,
  so what's kept can grow without changing the table. `title` and `series` are generated
  columns pulled out of it for querying; add more the same way with
  `ALTER TABLE books ADD COLUMN ... GENERATED ALWAYS AS (json_extract(data, '$.field'))`.
- `book_files`: the books' files on the device (`path`, `book_id`), downloaded or found.

Only the downloaded books are cached so far. Deleting a book's files keeps its cache entry.
With the device on USB, `sqlite3 <mount>/system/config/pbookorbit.db` opens it on a PC.

## Home screen tile

Each time it starts, the app makes sure it has a tile on the home screen, named PBookOrbit
with its own icon. The icons are built into the app (`src/icons`) and written to
`applications/icons/pbookorbit.bmp` and `pbookorbit_f.bmp` (the focused one) on the device,
only if they aren't there: replace them there to change the tile, or delete them to get the
app's own back at the next start. Keep them 110x110 (on the Era Color) or the tile won't
line up. The app adds itself to the launcher's `system/config/desktop/view.json`, at the
end of the first group of tiles, keeping a copy of the file as it was
(`view.json.before-pbookorbit`). The tile shows after the next restart: the first time,
the app says so and offers to restart the device. Moving or removing
it on the device sticks: the app only places it the first time. To take it off completely, put the copy back.

The task manager shows the app with the same name and the normal icon. The icons' line art
is in `src/icons/*.svg`: after changing it, `make icons` renders the BMPs (it needs
`rsvg-convert` and ImageMagick).

## Diagnostics

The app writes `pbookorbit.log` to the root of the device's USB storage
(`build/pbookorbit/<sdk>/pc/system/mnt/ext1` in the emulator). The log is cleared each time the app
starts. Every 10 seconds it records the frames rendered, memory use (`VmRSS`) and thread
count. It also logs every request with its status and time, every Wi-Fi connect, and each
time the book list switches A2 on or off while it scrolls (see `src/ui/eink.h`).

On e-ink every frame is a slow screen update, so `frames` should be 0 while the app is idle.
Anything that animates (a blinking cursor, `BusyIndicator`, transitions, Flickable momentum)
keeps the display busy and makes the whole device feel hung.

## The API

BookOrbit doesn't document its API. Everything below comes from its source:
`server/src/modules/auth/` and `server/src/modules/book/`, with types in `packages/types/src/`.

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

### Books

`POST /books/query` with a `BookQuery` (`packages/types/src/query.ts`):

```json
{"sort": [{"field": "addedAt", "dir": "desc"}], "pagination": {"page": 1, "size": 20},
 "q": "optional search text", "filter": {"optional": "GroupRule"}}
```

It returns `{items, total, page, size}`. Each item has `id`, `title`, `authors`, `seriesName`,
`seriesIndex`, `files: [{id, format, role, sizeBytes}]`, `hasCover`, `readingProgress`,
`readStatus` and more. The sort fields are listed in `StaticSortField`.

### Authors and series

Found by trying the endpoints against the server:

- `GET /authors/:id` returns one author: `name`, `sortName`, `description` (plain text),
  `bookCount`, `birthDate`/`birthYear`, `deathDate`/`deathYear`, `website`, `genres`,
  `imageUrl` and more.
- `GET /series?libraryId=&author=<text>` lists the series with an author whose name
  contains the text, ignoring case. `author=Martin` finds every Martin's series, so the
  client keeps only those whose `authors` include the exact name.
