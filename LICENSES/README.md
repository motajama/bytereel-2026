# License scope

The project intentionally uses different licenses for different kinds of
work. CERN recommends using a suitable hardware license for design files and
a software license for firmware and code; the license texts are kept separate
accordingly.

| Material | License | Scope |
|---|---|---|
| Original firmware, VM, assembler, build scripts, tests, tools, and example programs | GNU GPL-3.0-or-later | Original software in this repository, except files identified as third-party or historical |
| Hardware design source, future CAD, drawings, and hardware interface documentation | CERN-OHL-S-2.0 | Files under `hardware/` and `spec/HARDWARE.md`, unless a file states otherwise |
| General project documentation | CC BY 4.0 | Original documentation under `docs/`, `spec/`, and project README files, unless a file states otherwise |
| Bundled composite-video library | Upstream MIT | Preserve the copyright and license notice in `firmware/libraries/ESP_8_BIT_Color_Composite_Video_Library/` |

The complete CERN hardware license is in
[`CERN-OHL-S-2.0.txt`](CERN-OHL-S-2.0.txt). The canonical license identifier
is `CERN-OHL-S-2.0`.

Documentation is offered under Creative Commons Attribution 4.0 International
(CC BY 4.0): <https://creativecommons.org/licenses/by/4.0/>. The full legal
code is maintained by Creative Commons at that canonical URL.

## Third-party and historical material

The project-wide licenses do not relicense third-party material. The bundled
composite-video library contains its own upstream MIT license and notices.
Files under `reference/` include historical emulator, manual, and
service-firmware materials; review the individual file and its original
source before reusing them. Their presence in this repository does not imply
that they are covered by the GPL or CC BY grant.

GPL-3.0-or-later is copyleft: when distributing covered software or modified
versions, recipients must receive the corresponding source under GPL terms.
It does not prohibit commercial or private use. The license text is in the
repository-root `LICENSE` file. See the
[GNU GPLv3 page](https://www.gnu.org/licenses/gpl-3.0.html) for the official
license and its terms.

Local device backup files, if present in a developer checkout, are excluded
from the public source release. Do not add raw device flash captures to the
repository; use the documented source files and generated firmware artifacts.
