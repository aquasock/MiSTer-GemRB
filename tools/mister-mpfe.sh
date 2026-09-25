#!/usr/bin/env bash
# Shows or sets the MiSTer's HPS SDRAM controller port arbitration (MPFE priority and weights) over SSH.
# Usage: MISTER_HOST=<ip> tools/mister-mpfe.sh show
#        MISTER_HOST=<ip> tools/mister-mpfe.sh default | equal | mpu | mpu-priority | mpu-remap
# Command ports 0-5 carry FPGA traffic, 6 and 8 the L3 interconnect, 7 and 9 the ARM (reference record CVHPS-001).
# The settings are not persistent: every boot restores the preloader values, which the default profile also writes.
set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/../scripts/env.sh"

HOST="${MISTER_HOST:?set MISTER_HOST to the IP address of the MiSTer}"
ASKPASS="$WORK/askpass.sh"
printf '#!/bin/sh\necho 1\n' > "$ASKPASS"; chmod +x "$ASKPASS"
export SSH_ASKPASS="$ASKPASS" SSH_ASKPASS_REQUIRE=force
SSH=(ssh -o StrictHostKeyChecking=accept-new -o PubkeyAuthentication=no -o LogLevel=ERROR "root@$HOST")

# mppriority, mpweight_0_4 .. mpweight_3_4 and remappriority (CVHPS-002, CVHPS-003, CVHPS-005).
REGS=(0xFFC250AC 0xFFC250B0 0xFFC250B4 0xFFC250B8 0xFFC250BC 0xFFC250E0)

read_regs() {
	"${SSH[@]}" "for a in ${REGS[*]}; do devmem \$a 32; done"
}

# Encodes a profile, given as ten static weights, ten priorities and a queue remap mask, into the six register values
# after checking the manual's limits; or decodes register values back into per-port settings.
codec() {
	python3 - "$@" <<'PY'
import sys
mode = sys.argv[1]
def decode(values):
    prio, w0, w1, w2, w3 = values
    field = w0 | w1 << 32 | w2 << 64 | w3 << 96
    for port, name in enumerate(["FPGA0", "FPGA1", "FPGA2", "FPGA3", "FPGA4", "FPGA5", "L3 rd", "MPU rd", "L3 wr", "MPU wr"]):
        print(f"  port {port} {name:6s} priority {prio >> 3 * port & 7} weight {field >> 5 * port & 31}")
    sums = [field >> 50 + 8 * level & 255 for level in range(8)]
    print("  sum of weights by priority: " + " ".join(f"{level}:{s}" for level, s in enumerate(sums) if s))
if mode == "decode":
    values = [int(v, 0) for v in sys.argv[2:8]]
    decode(values[:5])
    print(f"  priority levels sent to the front of the command queue: "
          f"{[level for level in range(8) if values[5] >> level & 1] or 'none'}")
    sys.exit(0)
# Static weights, priorities and the priority levels whose transactions jump the command queue.
profiles = {
    "default":      ([16] * 6 + [4, 8, 4, 8], [0] * 10, 0x00),
    "equal":        ([8] * 6 + [4, 8, 4, 8], [0] * 10, 0x00),
    "mpu":          ([4] * 6 + [4, 16, 4, 16], [0] * 10, 0x00),
    "mpu-priority": ([16] * 6 + [4, 8, 4, 8], [0] * 7 + [1, 0, 1], 0x00),
    "mpu-remap":    ([16] * 6 + [4, 8, 4, 8], [0] * 7 + [1, 0, 1], 0x02),
}
weights, prio, remap = profiles[sys.argv[2]]
sums = [0] * 8
for w, p in zip(weights, prio):
    sums[p] += w
for w, p in zip(weights, prio):
    if not 0 <= w <= 31 or sums[p] > 255 or sums[p] - w >= 128:
        sys.exit(f"invalid profile: port weight {w} at priority {p} with sum {sums[p]}")
field = 0
for port, w in enumerate(weights):
    field |= w << 5 * port
for level, s in enumerate(sums):
    field |= s << 50 + 8 * level
mppriority = 0
for port, p in enumerate(prio):
    mppriority |= p << 3 * port
print(" ".join(f"0x{v:08X}" for v in [mppriority] + [field >> 32 * i & 0xFFFFFFFF for i in range(4)] + [remap]))
PY
}

case "${1:-show}" in
	show) ;;
	default|equal|mpu|mpu-priority|mpu-remap)
		read -r -a values <<< "$(codec encode "$1")"
		# mppriority first, as the manual recommends for run-time updates (CVHPS-004).
		"${SSH[@]}" "$(for i in 0 1 2 3 4 5; do printf 'devmem %s 32 %s; ' "${REGS[$i]}" "${values[$i]}"; done)"
		mapfile -t readback < <(read_regs)
		for i in 0 1 2 3 4 5; do
			[ "$((readback[i]))" -eq "$((values[i]))" ] ||
				{ echo "readback mismatch at ${REGS[$i]}: ${readback[$i]} != ${values[$i]}" >&2; exit 1; }
		done
		echo "applied profile $1"
		;;
	*) echo "usage: MISTER_HOST=<ip> $0 show | default | equal | mpu | mpu-priority | mpu-remap" >&2; exit 1 ;;
esac
mapfile -t current < <(read_regs)
echo "mppriority ${current[0]}  mpweight ${current[1]} ${current[2]} ${current[3]} ${current[4]}  remappriority ${current[5]}"
codec decode "${current[@]}"
