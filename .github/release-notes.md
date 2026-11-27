## What's new in 0.4.0

- Updates from GitHub: when a new version is released, the app shows what's new and installs it for you, then restarts into it
  - It checks GitHub a few seconds after starting; Settings > Updates checks again any time
  - The download is checked (size and SHA-256) before anything is replaced; Cancel or a failure leaves the app as it was
  - Needs a payload loader on port 9021 (etaHEN, or kstuff with elfldr) and a folder install; otherwise the dialog says why and offers the release page
- Fixed: text typed quickly on a USB keyboard, followed by Enter in the same instant, was lost (the keyboard closed first)
- Fixed: when loading more search results failed, scrolling down never tried again; it now retries after a few seconds and says what went wrong
- Fixed: answers for screens you had already left were kept in memory
- Fixed: KamTape answers starting with a byte-order mark, or without a status, were read as errors
- New tests: release reading, and a fuzz run of everything that reads network data

## Updating from 0.3.0

0.3.0 can't update itself yet: install this version from ProsperoStore or by copying the folder. From 0.4.0 on, updates install from the app.

## Install

- Unzip PPSA52005.zip and copy the PPSA52005 folder to /data/homebrew/ on the console, or install it from ProsperoStore
- Needs a PS5 that runs homebrew apps and an internet connection

## Known limits

- Beta: not yet tested on a real PS5
- Videos play in the PS5 browser, not in the app
- Unofficial app, not made or endorsed by KamTape
