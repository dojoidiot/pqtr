# LABS TODO

## Current: Integration Testing

- [ ] Get tune working with the pipe
- [ ] Test mode: produce all pipe artefacts to `tmp/var/tune/`
- [ ] Operating mode: produce only tail image
- [ ] Visual review of outputs

## Pending: Full ACEO (45 dials)

- [ ] Upgrade `etc/aceo.json` from 41 to 45 dials (add 4 detail dials)
- [ ] Generate `etc/aceo_full.json` (45×45 covariance matrix)
- [ ] Validate eigenstructure (~12D for 99% variance)

See `doc/FULL_ACEO_PLAN.md` for detailed roadmap.

## Done

- [x] SPSA bootstrap for initial covariance (41 dials)
- [x] ACEO/CMA-ES covariance training
- [x] Store trained covariance in `etc/aceo.json`
- [x] Code ready for 45 dials (`GEOS_DIAL_COUNT = 45`)
