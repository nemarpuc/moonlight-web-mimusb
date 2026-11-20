# Security Policy

## Supported Versions

Only the [latest release](https://github.com/nemarpuc/moonlight-web-mimusb/releases/latest)
receives fixes. A release carries upstream's tag and is built with libmimusb's newest release; the
board must run the firmware of the same libmimusb version.

## Reporting Vulnerabilities

Report vulnerabilities privately through the repository's
[GitHub Security Advisories form](https://github.com/nemarpuc/moonlight-web-mimusb/security/advisories/new).
Do not put passwords, serial numbers or captured input in a public issue. A problem in
Moonlight-Web itself goes to its [upstream project](https://github.com/linckosz/moonlight-web).

## What the add-on exposes

The viewer's keyboard and mouse become real USB input on the target. The add-on routes only the
server owner's events to a board; guests stay on the operating system path, where upstream's
checks apply. Anyone who can open the board's serial port can also type on the target, so keep
that port to trusted users.
