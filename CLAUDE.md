# PQTR Project Rules

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
- MAINs: RAWS, LABS, DESK, FAST, PLAY, SITE
- Each MAIN is self-contained with `src/`, `inc/`, `lib/`, `bin/`
- Dependencies via `wire.sh` symlinks

## Conventions
- 45 dials total in LABS pipeline
- Vibes = portable style presets (.pipe.json)
- RAWS output is scene-linear (flat) - this is correct
- TUNE matches styles, not RAWS appearance
