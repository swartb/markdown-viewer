# Third-party materiaal

Deze applicatie bevat of gebruikt werk van derden. Hieronder de herkomst en de
bijbehorende licenties.

## Flea

Ontwerp, maten, kleurrollen en de keuze van de iconen zijn overgenomen uit
[Flea](https://github.com/thisisgm/flea).

```
MIT License

Copyright (c) 2026 GM

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Lucide

De SVG-paddata van de lijniconen komt uit
[Lucide](https://lucide.dev) (Lucide-static 1.38.0), via Flea's `ui/js/Icons.js`.

```
ISC License

Copyright (c) for portions of Lucide are held by Cole Bemis 2013-2022 as part
of Feather (MIT). All other copyright (c) for Lucide are held by Lucide
Contributors 2022.

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
```

## Omarchy

De applicatie leest tijdens runtime het actieve thema uit
[Omarchy](https://github.com/omacom/omarchy): `colors.toml`, `shell.toml` en de
tekstschaal uit `Style.qml`. Omarchy is MIT-gelicenseerd; er is geen code uit
Omarchy meegebundeld.

## Qt 6 en het lettertype

Qt 6 (Widgets, Svg) wordt dynamisch gelinkt onder LGPL-3.0/GPL-3.0. De
monospace-UI gebruikt de fontconfig-alias `monospace`; op Omarchy is dat
JetBrains Mono Nerd Font (OFL-1.1). Beide worden niet meegeleverd.
