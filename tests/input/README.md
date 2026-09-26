# Official MiSTer cold-load input replay

`gemrb-menu-autosave.jsonl` is the accepted physical-input capture beginning at
the stable Baldur's Gate II GemRB menu and ending paused in the loaded `AR4000`
autosave. Its SHA256 is
`4d42358d3ea17121acd8e33ec7ab36a0cf41d2f41686b90c25258df4aae1e630`.

The official test is the no-panning variant. On a freshly rebooted MiSTer, stage
this trace and `tools/mister-input-harness.py`, then start replay before loading
the canonical MGL:

```sh
python3 /media/fat/gemrb/input-harness/mister-input-harness.py replay \
  /media/fat/gemrb/input-harness/gemrb-menu-autosave.jsonl \
  --exclude-key 103 --exclude-key 105 --exclude-key 106 --exclude-key 108 \
  --lead-in 25 --device-delay 1 --settle 5 &
sleep 2
/media/fat/Scripts/noodles-launcher.sh
sleep 1
printf '%s\n' 'load_core /media/fat/_Utility/Baldurs Gate II (GemRB).mgl' > /dev/MiSTer_cmd
```

The accepted replay contains 1,695 events over 57.101 seconds. It loads the
autosave, moves the character by mouse without viewport panning, and ends with
a Space press that pauses the game. Keep the original timing and do not use
`--start-at`, `--restore-prefix-motion`, `--wait-file`, or a tuned menu trace.

For the secondary panning comparison, replay the same trace and omit all four
`--exclude-key` options. That form contains all 1,707 captured events and
stutters more according to the hardware test.
