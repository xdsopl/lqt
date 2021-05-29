### Playing with lossless image compression based on the quadtree data structure

Quick start:

Encode [smpte.ppm](smpte.ppm) [PNM](https://en.wikipedia.org/wiki/Netpbm) picture file to ```encoded.lqt```:

```
./encode smpte.ppm encoded.lqt
```

Decode ```encoded.lqt``` file to ```decoded.ppm``` picture file:

```
./decode encoded.lqt decoded.ppm
```

Watch ```decoded.ppm``` picture file in [feh](https://feh.finalrewind.org/):

```
feh decoded.ppm
```

### Disable color space transformation:

Use the [sRGB](https://en.wikipedia.org/wiki/SRGB) color space directly instead of the default ```1``` [Reversible Color Transform](https://en.wikipedia.org/wiki/JPEG_2000#Color_components_transformation):

```
./encode smpte.ppm encoded.lqt 0
```

### Limited storage capacity

Use up to ```65536``` bits of space instead of the default ```0``` (no limit) and discard quality bits, if necessary, to stay below ```65536``` bits:

```
./encode smpte.ppm encoded.lqt 1 65536
```

