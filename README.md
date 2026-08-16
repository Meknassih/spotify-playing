# spotify-playing

## Synopsis
Gets the currently playing (or paused) song title and artist using `playerctl` from Spotify. Both attributes are truncated to a specific length (specified as a `define` that can be changed in the code). When one or both attributes are longer than that length, each subsequent call will return the next `length` characters offsetted to the right which results in a horizontal scrolling animation when the program is executed repeatedly in quick succession.

Works great as a [Polybar](https://github.com/polybar/polybar) script module.

Example for "Symphony No. 40 in G minor, K. 550: I. Allegro Molto" by "Wolfgang Amadeus Mozart" outputs `Symphony No. 40  - Wolfgang Amadeus` on the first run. Then `ymphony No. 40 i - olfgang Amadeus `, then `mphony No. 40 in - lfgang Amadeus M`, etc...

Tested on Debian 13 (Trixie).

## Using with Polybar

![spotify-playing as a Polybar module demo GIF](docs/images/spotify-playing-demo.gif)

Module configuration for Polybar:
```
[module/spotify-playing]
type = custom/script
exec = ~/.config/i3/polybar/spotify-playing
interval = 1
# NerdFont required for the Spotify logo below
format-prefix = " "
format-prefix-foreground = "#1ED760"
label = %output%
click-left = playerctl -p spotify previous
click-middle = playerctl -p spotify play-pause
click-right = playerctl -p spotify next
scroll-up = playerctl -p spotify volume +0.2
scroll-down = playerctl -p spotify volume -0.2
```

## Libraries
- [qlibc-2.5.1](https://wolkykim.github.io/qlibc/doc/html/index.html)
- [jsonc-0.19](https://json-c.github.io/json-c/)
