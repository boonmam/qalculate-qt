# Stacked fractions for Qalculate! Qt

This branch is based on Qalculate! Qt 5.13.0. It displays fractions vertically in
parsed expressions, calculation history, and live history previews. Enable or
disable **Stacked fractions** in Preferences, next to **Format result**. Both
options must be enabled. Stacked fractions are enabled by default in this fork.

The expression editor still accepts normal linear input: `(x+1)/(x-2)`.
Nested fractions, function denominators, grouped expressions, and rich-text
superscripts are supported. The calculation engine and persisted history stay
unchanged. Copying restores the original expression; history search includes
the text of rendered fractions. Equations render at Retina resolution and
follow the configured font and light/dark palette.

This is a display renderer, not a complete equation editor or TeX implementation.
Quoted text, matrices, multiline content, signed E-notation, and excessively
large or unsupported expressions retain the original Qt rendering. Scientific
notation printed with a multiplication sign and superscript exponent works.
Rendered expressions are selected as whole images; copying returns their
original rich text. The preference can restore ordinary character selection.

## Build on Apple Silicon macOS

Install Xcode Command Line Tools and Homebrew, then:

```sh
brew install qtbase qttools libqalculate gmp mpfr pkgconf
/opt/homebrew/opt/qtbase/bin/qmake qalculate-qt.pro
make -j6
open qalculate-qt.app
```

The locally built app uses the installed Homebrew libraries. To bundle its
runtime dependencies for a standalone installation, run `macdeployqt` against
a copy of the app before signing it.

## Renderer regression checks

```sh
/opt/homebrew/opt/qtbase/bin/qmake tests/stackedfractions.pro -o tests/Makefile
make -C tests -j4
QT_QPA_PLATFORM=offscreen tests/stackedfractions-test
```

Tests cover arithmetic grouping and division associativity, nesting, powers,
function denominators, malformed-input fallback, copy round trips through Qt's
HTML serializer, image resource decoding, history search, and both palettes.
