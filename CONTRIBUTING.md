# Contributing

Use the [issue tracker](https://github.com/nemarpuc/moonlight-web-mimusb/issues) for
proposed changes and submit patches through
[pull requests](https://github.com/nemarpuc/moonlight-web-mimusb/pulls).

## What belongs here

This repository holds only new files (`overlay/`), a few small patches (`patches/`) and the
installer (`installer/`). It stores no upstream Moonlight-Web source, and the frontend is never
changed: see [DESIGN.md](DESIGN.md). A change to Moonlight-Web itself belongs upstream.

- Keep every patch as small as it can be, with one line of context.
- Configuration keys have no defaults: a new key is required, and the parser names it when missing.
- A claim that something works on hardware comes with the evidence (board, firmware version, host
  and target OS).

## Checking a change

```bash
# Prepare upstream's newest release with the overlay and patches applied
python tools/prepare.py --tag latest --out work

# The backend compiles against upstream and libmimusb (syntax only)
g++ -std=c++17 -fsyntax-only -Wall -Wextra -DMOONLIGHTWEB_MIMUSB \
  -I work/moonlight-web/backend/native-host/src \
  -I work/moonlight-web/backend/native-host/include \
  -I work/libmimusb/include \
  work/moonlight-web/backend/native-host/src/input/MimUsbInput.cpp

# The configuration parser test
c++ -std=c++17 -Wall -Wextra -Ioverlay/backend/native-host/src/input \
  tests/test_config.cpp overlay/backend/native-host/src/input/MimUsbConfig.cpp -o test_config && ./test_config
```

The `Patches` workflow runs the same steps on every push, also against upstream's `main`.

## Code of Conduct

Everyone taking part is expected to follow the [Code of Conduct](CODE_OF_CONDUCT.md).
