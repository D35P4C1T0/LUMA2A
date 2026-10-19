# Statistical audio table variation

The four 2,048-entry tables use a fixed realization of a smooth Gaussian random
field. Every stored value differs from the assembly reference; the maximum
relative difference in each table is 0.5% (within binary64 rounding). No random
numbers are generated during audio processing.

For each stage, six independent standard normal draws use seed `0x554241`.
The smooth field and varied table are:

```
x_i = i / 2047
w_k = Normal(0, 1) / (k + 1)^2,  k = 0,...,5
g_i = (2*x_i - 1)^2 * sum(w_k * cos(k*pi*x_i))
delta_i = 0.005 * g_i / max_j(abs(g_j))
varied_i = reference_i * (1 + delta_i)
```

The quadratic envelope reduces disturbance near the centre of each circuit
curve. The cosine series avoids independent noise between adjacent entries.
The field is bounded after sampling, so the 0.5% table limit is deterministic,
not a confidence interval. Final binary64 rounding is moved toward the reference
when necessary to keep `abs(varied-reference) <= 0.005*abs(reference)`.
Zero reference values would remain zero. The supplied reference has no zero
entries. The bound applies to stored entries; nonlinear feedback and parabolic
interpolation require separate audio measurements.

The generator reads four little-endian binary64 arrays at file offsets
`0x176e08`, `0x17ae18`, `0x17ee28`, and `0x182e38` in the local assembly reference.
The complete original arrays matched the previous implementation byte for byte.
The reference binary SHA-256 must be
`f9d1852e3cf54ee9364a645f8c01e4aca3bcf33d41ade7ecfad50183cb0e2b32`.
The binary and original table values are not included in this repository.

## Reproduce

Run from the repository root with the local reference binary:

```sh
python3 tools/audio_table_model.py --reference '/path/to/assembly-reference.vst3' --write
cmake --build build --target model_render ModelChecks UiPreview --parallel 4
python3 tools/audio_table_model.py \
  --reference '/path/to/assembly-reference.vst3' \
  --renderer build/model_render \
  --report docs/audio-table-validation.json
ctest --test-dir build --output-on-failure
```

Omit `--write` to verify the installed tables without modifying them.
The script requires only the Python standard library. Audio validation also
requires the local `evidence/` corpus and its `final-vector/manifest.json`.
These private inputs are not distributed. A missing eligible audio pair fails
validation. Table-only verification does not claim to validate rendered audio.

## Measured error

[Validation report](audio-table-validation.json) records the model coefficients,
table checksum, input and reference audio checksums, and every measured case.
For the 65 supported static-control cases (14,678,000 channel samples), the
largest peak-normalized sample error is **0.296931%**, and the largest relative
RMS error is **0.316250%**. Both remain below 0.5%.

The measurements compare newly rendered output against saved native reference
audio across 12 sample rates from 8 to 384 kHz, mono/stereo, multiple block sizes,
impulses, tones, noise, compression and recovery cases. They are defined as:

```
peak-normalized error = max(abs(candidate-reference)) / max(abs(reference))
relative RMS error   = sqrt(sum((candidate-reference)^2) / sum(reference^2))
```

The denominators apply per case. Individual near-zero samples are not used as
denominators. Silent reference cases must produce exactly silent output.
Twenty manifest cases are explicitly excluded: generated signals without saved
input files, host bypass, and parameter automation events unsupported by the
offline renderer. No new execution of the Windows reference binary is claimed.
The measured corpus bound does not establish a universal bound for every
possible signal or control trajectory.
