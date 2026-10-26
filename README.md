# PBookOrbit

PocketBook client for a [BookOrbit](https://github.com/bookorbit/bookorbit) server. It signs
in with a normal BookOrbit account and lists the 20 most recently added books.

```sh
./pb.sh run apps/pbookorbit    # emulator
./pb.sh arm apps/pbookorbit    # device -> build/pbookorbit/<sdk>/arm/pbookorbit.app
```

The server is currently fixed at `https://books.example.com` (`DEFAULT_SERVER_URL` in
`bookorbitclient.cpp`).

## Signing in

Use the same username and password as the BookOrbit website. The app opens a session for
the device, shown as "PocketBook <model>" in the account's session list. The password is
not saved. The session's refresh token is saved to `pbookorbit.ini` in the system config
folder: `/mnt/ext1/system/config` on the device, `build/pbookorbit/<sdk>/pc/system/config` in the
emulator. Signing out revokes the session on the server.

## Diagnostics

The app writes `pbookorbit.log` to the root of the device's USB storage
(`build/pbookorbit/<sdk>/pc/system/mnt/ext1` in the emulator). The log is cleared each time the app
starts. Every 10 seconds it records the frames rendered, memory use (`VmRSS`) and thread
count. It also logs every request with its status and time, every Wi-Fi connect, and each
time the book list switches A2 on or off while it scrolls (see `eink.h`).

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
