#!/usr/bin/env python3
"""Record and replay timed Linux input events on MiSTer.

Recording observes evdev devices without grabbing them, so the user's input
continues to reach MiSTer normally. Replay creates one uinput device for each
recorded source and preserves the event timing.
"""

from __future__ import annotations

import argparse
import fcntl
import glob
import json
import os
import selectors
import signal
import struct
import sys
import time
from collections import Counter, defaultdict
from pathlib import Path


FORMAT = "MISTER_INPUT_TRACE_V1"
INPUT_EVENT = struct.Struct("@llHHi")
EV_SYN = 0x00
EV_KEY = 0x01
EV_REL = 0x02
EV_ABS = 0x03
EV_MSC = 0x04
SYN_REPORT = 0
CLOCK_MONOTONIC = 1

IOC_WRITE = 1
IOC_READ = 2


def ioc(direction: int, kind: str, number: int, size: int) -> int:
    return (direction << 30) | (size << 16) | (ord(kind) << 8) | number


def iow(kind: str, number: int, size: int = 4) -> int:
    return ioc(IOC_WRITE, kind, number, size)


def ior(kind: str, number: int, size: int) -> int:
    return ioc(IOC_READ, kind, number, size)


EVIOCSCLOCKID = iow("E", 0xA0)
UI_DEV_CREATE = (ord("U") << 8) | 1
UI_DEV_DESTROY = (ord("U") << 8) | 2
UI_SET_EVBIT = iow("U", 100)
UI_SET_KEYBIT = iow("U", 101)
UI_SET_RELBIT = iow("U", 102)
UI_SET_ABSBIT = iow("U", 103)
UI_SET_MSCBIT = iow("U", 104)

CODE_IOCTL = {
    EV_KEY: UI_SET_KEYBIT,
    EV_REL: UI_SET_RELBIT,
    EV_ABS: UI_SET_ABSBIT,
    EV_MSC: UI_SET_MSCBIT,
}


def ev_name(fd: int) -> str:
    size = 256
    buffer = bytearray(size)
    fcntl.ioctl(fd, ior("E", 0x06, size), buffer, True)
    return bytes(buffer).split(b"\0", 1)[0].decode("utf-8", "replace")


def list_devices(_args: argparse.Namespace) -> int:
    for path in sorted(glob.glob("/dev/input/event*")):
        try:
            fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK)
            try:
                name = ev_name(fd)
            finally:
                os.close(fd)
            print(f"{path}\t{name}")
        except OSError as error:
            print(f"{path}\t[unavailable: {error}]", file=sys.stderr)
    return 0


def write_json_line(output, value: dict) -> None:
    output.write(json.dumps(value, separators=(",", ":"), sort_keys=True) + "\n")
    output.flush()


def record(args: argparse.Namespace) -> int:
    selector = selectors.DefaultSelector()
    descriptors: list[dict] = []
    buffers: dict[int, bytearray] = {}
    for index, path in enumerate(args.devices):
        fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK)
        try:
            fcntl.ioctl(fd, EVIOCSCLOCKID, struct.pack("@i", CLOCK_MONOTONIC))
        except OSError as error:
            os.close(fd)
            raise RuntimeError(f"cannot select monotonic timestamps for {path}: {error}") from error
        descriptor = {"id": index, "path": path, "name": ev_name(fd)}
        descriptors.append(descriptor)
        buffers[fd] = bytearray()
        selector.register(fd, selectors.EVENT_READ, descriptor)

    stopping = False

    def request_stop(_signum, _frame) -> None:
        nonlocal stopping
        stopping = True

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)
    started_ns = time.monotonic_ns()
    first_event_ns: int | None = None
    event_count = 0
    with open(args.output, "w", encoding="utf-8", buffering=1) as output:
        write_json_line(
            output,
            {
                "kind": "header",
                "format": FORMAT,
                "started_monotonic_ns": started_ns,
                "input_event_size": INPUT_EVENT.size,
                "devices": descriptors,
            },
        )
        while not stopping:
            for key, _mask in selector.select(timeout=0.25):
                fd = key.fd
                try:
                    chunk = os.read(fd, INPUT_EVENT.size * 256)
                except BlockingIOError:
                    continue
                if not chunk:
                    raise RuntimeError(f"input device closed: {key.data['path']}")
                pending = buffers[fd]
                pending.extend(chunk)
                while len(pending) >= INPUT_EVENT.size:
                    raw = bytes(pending[: INPUT_EVENT.size])
                    del pending[: INPUT_EVENT.size]
                    sec, usec, event_type, code, value = INPUT_EVENT.unpack(raw)
                    event_ns = sec * 1_000_000_000 + usec * 1_000
                    if first_event_ns is None:
                        first_event_ns = event_ns
                    write_json_line(
                        output,
                        {
                            "kind": "event",
                            "t_ns": event_ns - first_event_ns,
                            "device": key.data["id"],
                            "type": event_type,
                            "code": code,
                            "value": value,
                        },
                    )
                    event_count += 1
        write_json_line(
            output,
            {
                "kind": "end",
                "events": event_count,
                "recording_duration_ns": time.monotonic_ns() - started_ns,
                "event_duration_ns": 0 if first_event_ns is None else event_ns - first_event_ns,
            },
        )

    for key in list(selector.get_map().values()):
        selector.unregister(key.fd)
        os.close(key.fd)
    print(f"recorded {event_count} events from {len(descriptors)} devices to {args.output}")
    return 0 if event_count else 3


def read_trace(path: str) -> tuple[dict, list[dict], dict | None]:
    header = None
    end = None
    events = []
    with open(path, "r", encoding="utf-8") as source:
        for line_number, line in enumerate(source, 1):
            try:
                item = json.loads(line)
            except json.JSONDecodeError as error:
                raise RuntimeError(f"{path}:{line_number}: {error}") from error
            kind = item.get("kind")
            if kind == "header":
                if header is not None:
                    raise RuntimeError("trace contains multiple headers")
                header = item
            elif kind == "event":
                events.append(item)
            elif kind == "end":
                end = item
            else:
                raise RuntimeError(f"trace contains unknown record kind: {kind!r}")
    if header is None or header.get("format") != FORMAT:
        raise RuntimeError(f"not a {FORMAT} trace")
    previous = -1
    device_ids = {device["id"] for device in header["devices"]}
    for event in events:
        if event["t_ns"] < previous:
            raise RuntimeError("event timestamps are not monotonic")
        if event["device"] not in device_ids:
            raise RuntimeError(f"event refers to unknown device {event['device']}")
        previous = event["t_ns"]
    return header, events, end


def validate(args: argparse.Namespace) -> int:
    header, events, end = read_trace(args.trace)
    counts = Counter(event["device"] for event in events)
    duration = (events[-1]["t_ns"] / 1_000_000_000) if events else 0.0
    print(f"format={header['format']} devices={len(header['devices'])} events={len(events)} duration_s={duration:.3f}")
    for device in header["devices"]:
        print(f"device={device['id']} events={counts[device['id']]} name={device['name']} path={device['path']}")
    if end is None:
        print("warning: trace has no clean end marker", file=sys.stderr)
        return 4
    if end.get("events") != len(events):
        raise RuntimeError("end marker event count does not match trace")
    return 0 if events else 3


def create_uinput_device(device: dict, events: list[dict], uinput_path: str) -> int:
    fd = os.open(uinput_path, os.O_WRONLY | os.O_NONBLOCK)
    codes: dict[int, set[int]] = defaultdict(set)
    abs_values: dict[int, list[int]] = defaultdict(list)
    for event in events:
        event_type = event["type"]
        code = event["code"]
        if event_type in CODE_IOCTL:
            codes[event_type].add(code)
        if event_type == EV_ABS:
            abs_values[code].append(event["value"])
    for event_type, event_codes in codes.items():
        fcntl.ioctl(fd, UI_SET_EVBIT, event_type)
        request = CODE_IOCTL[event_type]
        for code in sorted(event_codes):
            fcntl.ioctl(fd, request, code)

    absmax = [0] * 64
    absmin = [0] * 64
    absfuzz = [0] * 64
    absflat = [0] * 64
    for code, values in abs_values.items():
        if code >= len(absmax):
            raise RuntimeError(f"ABS code {code} exceeds legacy uinput limit")
        low, high = min(values), max(values)
        if low == high:
            low -= 1
            high += 1
        absmin[code] = low
        absmax[code] = high
    name = f"MiSTer replay {device['name']}".encode("utf-8")[:79]
    setup = struct.pack(
        "@80sHHHHi" + "i" * 256,
        name,
        0x03,
        0x1209,
        0x4761 + int(device["id"]),
        1,
        0,
        *(absmax + absmin + absfuzz + absflat),
    )
    os.write(fd, setup)
    fcntl.ioctl(fd, UI_DEV_CREATE)
    return fd


def replay(args: argparse.Namespace) -> int:
    header, events, _end = read_trace(args.trace)
    by_device: dict[int, list[dict]] = defaultdict(list)
    for event in events:
        by_device[event["device"]].append(event)
    descriptors = {device["id"]: device for device in header["devices"]}
    outputs = {
        device_id: create_uinput_device(descriptors[device_id], device_events, args.uinput)
        for device_id, device_events in by_device.items()
    }
    try:
        time.sleep(args.device_delay)
        start_ns = time.monotonic_ns() + int(args.lead_in * 1_000_000_000)
        for event in events:
            target_ns = start_ns + int(event["t_ns"] / args.speed)
            while True:
                remaining_ns = target_ns - time.monotonic_ns()
                if remaining_ns <= 0:
                    break
                time.sleep(min(remaining_ns / 1_000_000_000, 0.05))
            raw = INPUT_EVENT.pack(0, 0, event["type"], event["code"], event["value"])
            os.write(outputs[event["device"]], raw)
        for fd in outputs.values():
            os.write(fd, INPUT_EVENT.pack(0, 0, EV_SYN, SYN_REPORT, 0))
        time.sleep(args.settle)
    finally:
        for fd in outputs.values():
            try:
                fcntl.ioctl(fd, UI_DEV_DESTROY)
            finally:
                os.close(fd)
    duration = (events[-1]["t_ns"] / args.speed / 1_000_000_000) if events else 0.0
    print(f"replayed {len(events)} events in {duration:.3f} seconds")
    return 0


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    subparsers = result.add_subparsers(dest="command", required=True)
    listing = subparsers.add_parser("list", help="list readable evdev devices")
    listing.set_defaults(function=list_devices)

    recording = subparsers.add_parser("record", help="record evdev events until SIGINT or SIGTERM")
    recording.add_argument("output")
    recording.add_argument("devices", nargs="+")
    recording.set_defaults(function=record)

    validation = subparsers.add_parser("validate", help="validate and summarize a trace")
    validation.add_argument("trace")
    validation.set_defaults(function=validate)

    playback = subparsers.add_parser("replay", help="replay a trace through uinput")
    playback.add_argument("trace")
    playback.add_argument("--uinput", default="/dev/uinput")
    playback.add_argument("--lead-in", type=float, default=1.0)
    playback.add_argument("--device-delay", type=float, default=1.0)
    playback.add_argument("--settle", type=float, default=0.5)
    playback.add_argument("--speed", type=float, default=1.0)
    playback.set_defaults(function=replay)
    return result


def main() -> int:
    args = parser().parse_args()
    if getattr(args, "speed", 1.0) <= 0:
        raise SystemExit("--speed must be positive")
    try:
        return args.function(args)
    except (OSError, RuntimeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
