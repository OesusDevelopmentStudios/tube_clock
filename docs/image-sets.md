# Image sets

An image set is the collection of pictures the clock uses to show digits.

## On the clock

The files live in the clock's LittleFS file system:

```
<set>_<screen>_<digit>.png
```

| Part | Values | Meaning |
| --- | --- | --- |
| `set` | `a-z`, `0-9`, `-`; 1-20 characters | Name of the set, shown in the clock menu |
| `screen` | `0`-`5` | Tube, `0` = leftmost |
| `digit` | `0`-`9` | Digit the picture shows |

Example: `retro_0_7.png` is the "7" of the set `retro` on the leftmost tube.

**Fallback:** if `<set>_<screen>_<digit>.png` does not exist, the clock uses `<set>_0_<digit>.png`. A set where all
tubes look the same therefore needs only the 10 files for screen 0; you only add files for the tubes that
should look different. A set is complete (and shown in the menu) when all ten `<set>_0_<digit>.png` files exist.

Pictures are 135 × 240 pixels PNG files. Smaller pictures are centred on black; larger ones are cropped. The
display shows 16-bit colour (RGB565). Palette PNGs, true colour and transparency (blended on black) are supported.

The file system has about 2.7 MB on clocks with 4 MB of flash, 6.7 MB with 8 MB and 14.7 MB with 16 MB. Typical sets
need 50-200 KB (10 files), so around 10-20 sets fit on a 4 MB clock.

## Importing pictures with the app

You do not have to prepare PNG files yourself. **Import Image Set…** accepts PNG, JPEG, BMP, GIF, WebP and TIFF
pictures of any size, and converts them:

1. scales them to 135 × 240. *Show whole picture* adds borders in a colour of your choice, *Fill the tube*
   crops, *Stretch* distorts.
2. rounds the colours to what the display can show (RGB565), which shrinks the files without a visible
   difference on the clock
3. saves them as optimised PNG, using a palette when the picture has 256 colours or fewer

The app takes the digit from the file name:

| File names | Result |
| --- | --- |
| 10 pictures whose names **end in the digit**: `0.png` … `9.png`, `digit-7.jpg`, `20.bmp` … `29.bmp` | The same picture on every tube |
| Pictures named `<anything>_<tube>_<digit>.<ext>`, e.g. `x_0_0.jpg` … `x_0_9.jpg` plus `x_5_1.jpg` | Per-tube pictures (the set needs at least tube 0 for every digit) |

The clock faces of the original EleksTubeHAX project (`10.bmp` … `19.bmp` for the first face, and so on) and the
extra artwork in [assets/](../assets) can be imported directly: select the 10 files of one face.

More clock faces made by the community:

- <https://github.com/upiir/ips_clock_100x_themes>
- <https://github.com/upiir/EleksTube_ips_custom_theme>
- <https://github.com/upiir/rgb_glow_tube_clock>

## Sets shipped with the app

[data/](../data) contains the seven clock faces of EleksTubeHAX, converted with
[scripts/convert_legacy_faces.py](../scripts/convert_legacy_faces.py): `blue-ribbon`, `cyan-and-pink`, `modern-neon`,
`nixie-thin-blue`, `nixie-tube`, `soap-bubble-rainbow`, `yellow-and-blue-dots`.

## Uploading over WiFi

On a clock's page in the app, **Import Image Set…** followed by **Upload to Clock** sends the files with the HTTP
API (`PUT /api/files/<name>`, see [http-api.md](http-api.md)). The app checks the free space first.
