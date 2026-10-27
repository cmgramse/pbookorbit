# PBookOrbit

PBookOrbit lets you use your [BookOrbit](https://github.com/bookorbit/bookorbit) library
from a PocketBook. You can browse it, download books and audiobooks, and read or listen
to them with the PocketBook's own reader and audiobook player. It's designed to look like
one of the device's built-in apps rather than a port of something else.

## Features

- Sign in with the same server, username and password you use on the BookOrbit website.
  Your password isn't stored, and the device appears in your account's session list as
  "PocketBook <model>".
- The library is laid out like the PocketBook's own library app, with covers, authors and
  series. You can filter to ebooks or audiobooks, sort it a few ways, jump to a letter
  with the A-Z rail and page through with the page keys.
- Tabs for collections, series and authors.
- Pages for each book and author, with links through to everything else by that author
  or in that series.
- Search that updates as you type, and remembers recent searches.
- Ebooks download to `Books` and open in the reader. Audiobooks go to `Audio Books` and
  play in the Audiobooks app. If a book has both, it's shown once.
- It can find books you already had on the device (from KOReader's BookOrbit plugin, for
  example) and mark them as downloaded.
- Works with night mode, and uses your BookOrbit accent colour.
- Adds its own tile to the applications screen, and shows up properly in the task
  manager.

## Screenshots

| | |
|:---:|:---:|
| ![Sign-in page](docs/images/login.png) | ![Library](docs/images/library.png) |
| Signing in | The library |
| ![Book info page](docs/images/book-info.png) | ![Series tab](docs/images/series.png) |
| A book's info page | Series |
| ![Authors tab](docs/images/authors.png) | ![Author info page](docs/images/author-info.png) |
| Authors | An author's page |

## Installation

So far it has only been tested on an Era Color running firmware 6.10, and 6.10 is the
only firmware it's built for. It may work on other 6.10 devices. Your BookOrbit account
will need permission to download books.

1. Download `pbookorbit.app` from a release, or build it yourself (see below).
2. Plug the PocketBook into your computer and copy `pbookorbit.app` into its
   `applications` folder.
3. Eject it, go to Applications and open `pbookorbit`.
4. On first run it adds a PBookOrbit tile to the applications screen. The tile won't show
   up until the device restarts, so it'll offer to do that for you.
5. Sign in with your server address (`books.example.com`, say), username and password.
   It uses https unless you type `http://` in front.

To update, copy the new `pbookorbit.app` over the old one.

To uninstall, delete `applications/pbookorbit.app` and the two icons in
`applications/icons` (`pbookorbit.bmp` and `pbookorbit_f.bmp`). You can remove the tile
from the device itself. Before adding the tile the app saved the original launcher config
as `system/config/desktop/view.json.before-pbookorbit`, so you can also rename that back to
`view.json`. The app keeps its data in `system/config/pbookorbit.db` and its cached covers
in `system/cache/pbookorbit-covers`, and both can be deleted.

## Development

It's a Qt 6/QML app built on the PocketBook SDK. Builds go through
[pocketbook-kit](kit), which runs the SDK inside a container, so all you need on your
machine is `git`, `make` and podman or docker.

```sh
git clone --recurse-submodules <this repo> pbookorbit
cd pbookorbit
make setup    # first time only: builds the container image and downloads the SDK
make run      # build and run in the emulator
make arm      # build for the device, into build/pbookorbit/<sdk>/arm/pbookorbit.app
```

The kit lives in `kit/` as a submodule. See its README for picking an SDK (use
`kit/pb.sh use B300-6.10` for the Era Color and other B300 devices), making the emulator
match your device, and running headless tests with `kit/pb.sh test . <script>`. Set
`PBOOKORBIT_DB=<file>` to point the app at a different database, which keeps tests away
from your own sign-in.

If you're handing the app to people who all use the same server, you can build it with
the server already filled in. They can still change it.

```sh
make release SERVER=books.example.com
```

The icons are SVGs in `src/icons`. Run `make icons` after editing them to regenerate the
BMPs (you'll need `rsvg-convert` and ImageMagick).

There's more detail on how it works, including the BookOrbit API calls it makes, in
[docs/technical.md](docs/technical.md). [NOTES.md](NOTES.md) is a collection of things
learned about the PocketBook firmware and SDK, and [TODO.md](TODO.md) is the to-do list.

## Diagnostics

The app writes `pbookorbit.log` to the root of the device's storage. It's cleared every
time the app starts, and normally only has startup information and errors in it.

To get a fuller log, tap the title at the top of the screen six times. That turns on
diagnostics logging, which records requests and timings, Wi-Fi connections, screen
updates, and memory use every 10 seconds. It stays on until you tap six times again.
If you're reporting a bug, turn it on, make the problem happen, then plug in the device
and attach the log.

When running in the emulator, the log is in `build/pbookorbit/<sdk>/pc/system/mnt/ext1`.
