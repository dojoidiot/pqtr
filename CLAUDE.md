# PQTR Project Rules

## Session Restart
When starting a new session, read these files to get up to speed:
1. `README.md` - Project overview and architecture
2. `next.md` - Current state and next steps

## Hard Shut
When the user says **"hard shut"**, immediately:
1. Read `LABS/doc/docs.md` for documentation standards
2. Update all affected docs to reflect current implementation:
   - `LABS/doc/tldr.md` - current status summary
   - `LABS/doc/todo.md` - remaining work
   - `LABS/README.md` - if architecture changed
   - Module docs if dials/features changed
3. Apply docs.md rules: present tense, correct terminology, accurate counts
4. Verify dial counts match code (45 style dials)
5. Update any stale cross-references

## Output Paths
- Working files: `<PROJECT>/tmp/` (gitignored)
- Test outputs: `<PROJECT>/tmp/var/tune/`
- Test images: `<PROJECT>/var/pics/`
- Never use `/tmp/` - keep outputs in project tree

## Build
- Always wire before build: `./wire.sh && make`
- OpenCV needs: `LD_LIBRARY_PATH=lib/opencv/build/lib`

## Testing LABS
```bash
cd LABS
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/tune \
  var/pics/DSC00144.ARW preview \
  --save-area tmp/var/tune \
  --full --optimizer hybrid --fine --logs
```

## Project Structure
- MAINs: GEAR, LABS, DESK, FAST, PLAY, SITE
- Each MAIN is self-contained with `src/`, `inc/`, `lib/`, `bin/`
- Dependencies via `wire.sh` symlinks

## Conventions
- 45 dials total in LABS pipeline
- Vibes = portable style presets (.pipe.json)
- GEAR output is scene-linear (flat) - this is correct
- TUNE matches styles, not GEAR appearance
