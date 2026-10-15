# Phase 2 — thermal input and BYPASS, P2_THERMAL_INPUT_V1

Checkpoint: 2026-10-15. Base commit: 5676cdd28753c97def096f12d94757f0db27e8e1.

## Justification & Paper Alignment

Choice: port the existing thermal input preparation and BYPASS before N5.
Classification: **Engineering Decision — not paper-derived**.
Specific rule: translate `src/enhancement/experiment_b.py::prepare_thermal_uint8_plane`
and `src/enhancement/baselines.py::BypassThermalEnhancer` from the Python lab.
Source byte hashes are recorded with the fixtures. Copy ownership and OpenCV
color conversion do not change scientific logic. No new threshold or policy.

## Contract

- Preparation admits nonempty 2D OpenCV uint8 images with 1, 3 or 4 channels.
- One channel: clone. BGR: COLOR_BGR2GRAY. BGRA: COLOR_BGRA2GRAY, ignoring alpha.
- BYPASS admits only a prepared nonempty 2D CV_8UC1 plane and clones it.
- Inputs are immutable; outputs own their storage and retain image geometry.
- Strided/ROI inputs are supported, including conversion independent of outside pixels.
- Applicable validation precedence: empty, uint8 depth, image dimensionality,
  channel count. Invalid inputs throw std::invalid_argument; OpenCV errors propagate.
- cv::Mat is a typed C++ boundary: Python's non-array TypeError has no direct
  input representation. CV_8UC1 represents both accepted Python H×W and H×W×1.
- No normalization, contrast mapping, enhancement selection, N5 or UI changes.

## Implemented

`core/enhancement/thermal_input.h/.cpp` adds two free functions, without policy
classes or factories. CMake registers the source in qart_core and two tests:
ThermalInputContract and Phase2ThermalInputParity. OpenCV include directories
are public because the new header exposes cv::Mat. Existing targets are retained.

The repository consequently has 40 regular plus 10 optional real-data test
definitions. This count is not a report that all 50 have run or passed.

## Verification performed

Focused C++ builds passed with g++ C++17, -Wall -Wextra -Werror, using locally
extracted coherent Ubuntu OpenCV 4.6.0 core/imgproc dependencies.

- ThermalInputContract passed: immutable/owned output, strided ROI equivalence,
  grayscale preservation, BGR channel order, ignored alpha, constant BYPASS,
  empty/wrong-depth/wrong-channels/non-image dimensions and color BYPASS rejection.
- Phase2ThermalInputParity passed: all 12 fixtures exactly equal byte-for-byte
  for both preparation and subsequent BYPASS. Python OpenCV 5.0.0 generated the
  fixtures using the actual lab functions. The 12 cases span 1×1, 1×17, 19×1,
  31×33 and channel counts 1/3/4. See fixture provenance for exact environment.
- Golden bytes were checked against recorded hashes and deterministic regeneration.
- git diff --check passed.

These focused programs were built directly, independent of the existing Qt
project configuration. Full root CMake/CTest and Qt were not run in this
environment. Real-data parity, N5 translation/parity, target runtime and UI
acceptance remain open. Cross-version agreement here is limited to these
preparation/BYPASS fixtures; it does not establish N5 or universal OpenCV parity.

## Reproduce in the normal project toolchain

Configure the existing project with BUILD_TESTING=ON and coherent Qt/OpenCV
packages, build test_thermal_input and test_phase2_thermal_input_parity, then:

```sh
ctest --test-dir <build-directory> --output-on-failure -R '^(ThermalInputContract|Phase2ThermalInputParity)$'
```

To regenerate fixtures in a separate new directory using the Python lab:

```sh
python tools/generate_phase2_input_fixtures.py --lab-root <FusionDemos> --output <new-directory>
```

Compare against committed fixtures; do not overwrite golden outputs to hide a
C++ mismatch. The generator intentionally rejects an existing output directory.
Fixtures are small deterministic test data, not a bundled dataset or new quality
experiment.

## Phase boundary

BYPASS remains the default planned production behavior; explicitly selected N5
is the accepted optional enhancement scope. N7 remains deferred. This change
ports the first boundary only and does not yet wire a default/mode into the UI.
No registration, fusion, detection or new manual-review campaign is introduced.

Phase 2 remains OPEN. The earlier 50% weighted milestone estimate receives no
additional completed-milestone credit yet: this is partial progress within the
C++ implementation/parity milestones. Next: translate the unchanged N5_NUMERIC_V1
contract, then run its own numerical parity. The runtime descriptor subset
remains a separate unresolved contract; it is not frozen by this input port.
