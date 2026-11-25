#!/usr/bin/env python3
"""End-to-end checks of the PC build, run by CTest. The real game plays
without a window, fast, with scripted keys (PFR_INPUT) and screenshots
(PFR_SHOT); a run fails on a crash or a hang (watchdog), on missing or empty
frames, or when the game does not reach the expected maps (PFR_TRACE).

  smoke_test.py EXE WORK            boot -> intro -> title (holds until Start,
                                    needs the music sequencer) -> new game ->
                                    controls guide -> Prof. Oak; intro music
                                    recorded and checked non-silent.
  smoke_test.py --world EXE WORK    new game -> bedroom (2F) -> stairs -> 1F ->
                                    Pallet Town -> wild battle (PFR_TEST hook:
                                    Charmander vs Rattata, fought with A) ->
                                    back on the map.
  smoke_test.py --save EXE WORK     new game -> Pallet Town -> Start menu: bag,
                                    trainer card, SAVE; a second run continues
                                    the save (quest log recap) and must give
                                    control back where the game was saved,
                                    then opens the party and a summary.

The scripts are frame-exact for FireRed and LeafGreen alike (same scripts,
same maps); the save file name comes from the executable (firered.sav /
leafgreen.sav).
"""
import array
import math
import os
import subprocess
import sys
import wave

# Common opening: title (Start), new game, controls guide, Prof. Oak's speech
# and the naming screens all pass with A presses (the name becomes AAAAAAA);
# control is given in the bedroom (map 4.1) around frame 7700.
NEW_GAME = "2600-2604:START,3000-7600/30:A"
# Bedroom -> stairs (down-left stair warp: press LEFT on it) -> 1F -> the
# door mat -> Pallet Town (map 3.0), free around frame 8470.
TO_TOWN = ("7750-7810:RIGHT,7830-7895:UP,7900-7920:LEFT,8150-8245:DOWN,8255-8345:LEFT,"
           "8360-8390:DOWN")


def frame_bytes(path):
    with open(path, "rb") as f:
        return f.read()[54:]   # skip the BMP header


def colors(path):
    data = frame_bytes(path)
    return len({data[i:i + 3] for i in range(0, len(data) - 2, 3)})


def run_game(exe, work, env_extra, timeout=900):
    env = dict(os.environ, PFR_HEADLESS="1", PFR_FAST="1", PFR_HANG_EXIT="1", PFR_TRACE="1")
    env.setdefault("PFR_MUTE", "1")
    env.update(env_extra)
    r = subprocess.run([exe], cwd=work, env=env, capture_output=True, text=True, timeout=timeout)
    return r.returncode, r.stdout + r.stderr


def crashed(rc, log):
    return rc != 0 or "[pfr:crash]" in log or "[pfr:hang]" in log


def traces(log):
    return [l for l in log.splitlines() if l.startswith("[pfr] trace@")]


def check_frames(work, names, errors, min_colors=16):
    for n in names:
        p = os.path.join(work, n + ".bmp")
        if not os.path.exists(p):
            errors.append(f"no {n} frame")
            continue
        c = colors(p)
        print(f"{n}: {c} colours")
        if c < min_colors:
            errors.append(f"{n} frame looks empty ({c} colours)")


def report(errors, log, ok_message):
    for e in errors:
        print("FAIL", e)
    if errors:
        print(log[-4000:])
        return 1
    print(ok_message)
    return 0


def main_smoke(exe, work):
    rc, log = run_game(exe, work, dict(
        PFR_MUTE="0", PFR_WAV="smoke.wav", PFR_EXIT_AFTER="4100",
        PFR_INPUT="2600-2604:START,3000-3900/30:A",
        PFR_SHOT="100:copyright.bmp,1200:intro.bmp,2000:title1.bmp,2400:title2.bmp,"
                 "2800:controls.bmp,4000:oak.bmp"))
    errors = []
    if crashed(rc, log):
        errors.append(f"exit {rc}, crash or hang reported")
    check_frames(work, ("copyright", "intro", "title1", "title2", "controls", "oak"), errors)
    shots = {n: os.path.join(work, n + ".bmp") for n in ("title1", "title2", "controls", "oak")}
    if all(os.path.exists(p) for p in shots.values()):
        t1, t2, controls, oak = (frame_bytes(shots[n]) for n in ("title1", "title2", "controls", "oak"))
        # The title screen must still be up 400 frames later (not back to the
        # intro): only the flames change, most pixels stay.
        same = sum(1 for a, b in zip(t1, t2) if a == b) / len(t1)
        print(f"title screen held: {same:.0%} identical")
        if same < 0.5:
            errors.append(f"title screen did not hold ({same:.0%} identical)")
        if controls == t2 or oak == controls:
            errors.append("Start/A did not change screens")
    wav = os.path.join(work, "smoke.wav")
    if os.path.exists(wav):
        w = wave.open(wav, "rb")
        a = array.array("h")
        a.frombytes(w.readframes(w.getnframes()))
        sr = w.getframerate()
        w.close()
        seg = a[2 * 8 * sr:2 * 20 * sr]    # 8 s .. 20 s: intro music (Gengar vs Nidorino)
        rms = math.sqrt(sum(x * x for x in seg) / max(1, len(seg)))
        print(f"intro music RMS {rms:.0f}")
        if rms < 200:
            errors.append(f"intro music silent (RMS {rms:.0f})")
    else:
        errors.append("no recording")
    return report(errors, log, "game smoke test passed")


def main_world(exe, work):
    rc, log = run_game(exe, work, dict(
        PFR_EXIT_AFTER="12500", PFR_TEST="battle@8700",
        PFR_INPUT=f"{NEW_GAME},{TO_TOWN},9200-12400/45:A",
        PFR_SHOT="7720:bedroom.bmp,8100:livingroom.bmp,8690:town.bmp,9000:battle.bmp,"
                 "9400:battle2.bmp,10400:battle3.bmp,12490:after.bmp"))
    errors = []
    if crashed(rc, log):
        errors.append(f"exit {rc}, crash or hang reported")
    t = traces(log)
    for label, map_id in (("the bedroom", "map=4.1 "), ("the living room", "map=4.0 "), ("Pallet Town", "map=3.0 ")):
        if not any(map_id in l and "free=1" in l for l in t):
            errors.append(f"never free in {label}")
    if "test: wild battle" not in log:
        errors.append("the test battle did not start")
    after = [l for l in t if int(l.split("@")[1].split()[0]) > 9200]
    if not any("map=3.0 " in l and "free=1" in l for l in after):
        errors.append("no control back in Pallet Town after the battle")
    check_frames(work, ("bedroom", "livingroom", "town", "battle", "battle2", "battle3", "after"), errors)
    return report(errors, log, "world + battle test passed")


# Pallet Town: Start menu (BAG, player, SAVE, OPTION, EXIT without Pokemon):
# bag, trainer card, then SAVE -> YES.
SAVE_RUN = ",".join([NEW_GAME, TO_TOWN,
                     "8700-8703:START", "8760-8763:A",            # BAG
                     "9050-9053:B", "9200-9203:DOWN", "9250-9253:A",  # back, trainer card
                     "9500-9503:B", "9700-9703:DOWN", "9750-9753:A",  # back, SAVE
                     "9900-9903:A", "10200-10203:A", "10400-10403:A"])


def main_save(exe, work, save_name):
    errors = []
    rc, log = run_game(exe, work, dict(PFR_EXIT_AFTER="10700", PFR_INPUT=SAVE_RUN,
                                       PFR_SHOT="9000:bag.bmp,9450:card.bmp,9850:save.bmp"))
    if crashed(rc, log):
        errors.append(f"first run: exit {rc}, crash/hang")
    save = os.path.join(work, save_name)
    if not os.path.exists(save) or os.path.getsize(save) != 0x20000:
        errors.append(f"no 128 KiB {save_name} after saving")
    free = [l for l in traces(log) if "free=1" in l]
    saved_pos = free[-1].split()[2:4] if free else None   # map=..., pos=...
    print("saved at", " ".join(saved_pos or ["?"]))

    # Continue: title, CONTINUE, the quest log recap (B skips it), then a
    # Pokemon for the party screen, Start -> POKEMON -> first one -> summary.
    rc, log = run_game(exe, work, dict(
        PFR_EXIT_AFTER="4800", PFR_TEST="givemon@3500",
        PFR_INPUT="2600-2604:START,2900-2904:A,3100-3400/40:B,3600-3603:START,3660-3663:A,"
                  "3900-3903:A,3960-3963:A,4300-4303:RIGHT",
        PFR_SHOT="2850:continue.bmp,3050:questlog.bmp,3450:back.bmp,3850:party.bmp,"
                 "4200:summary.bmp,4600:summary2.bmp"))
    if crashed(rc, log):
        errors.append(f"second run: exit {rc}, crash/hang")
    if f"flash: loaded {save_name}" not in log:
        errors.append(f"{save_name} not loaded")
    free = [l for l in traces(log) if "free=1" in l]
    if not free:
        errors.append("never got control after Continue")
    elif saved_pos and free[0].split()[2:4] != saved_pos:
        errors.append(f"continued at {free[0].split()[2:4]}, saved at {saved_pos}")
    else:
        print("continued at", " ".join(free[0].split()[2:4]))
    check_frames(work, ("bag", "card", "save", "continue", "questlog", "back", "party", "summary", "summary2"),
                 errors)
    return report(errors, log, "save/continue/menus test passed")


def prepare(work):
    os.makedirs(work, exist_ok=True)
    for f in os.listdir(work):
        if f.endswith((".bmp", ".wav", ".sav", ".bin")):
            os.remove(os.path.join(work, f))


def main():
    mode = "smoke"
    args = sys.argv[1:]
    if args and args[0].startswith("--"):
        mode = args.pop(0)[2:]
    exe, work = os.path.abspath(args[0]), os.path.abspath(args[1])
    prepare(work)
    save_name = ("leafgreen" if "leafgreen" in os.path.basename(exe) else "firered") + ".sav"
    if mode == "world":
        return main_world(exe, work)
    if mode == "save":
        return main_save(exe, work, save_name)
    return main_smoke(exe, work)


if __name__ == "__main__":
    sys.exit(main())
