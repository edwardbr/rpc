# Telemetry Animation Refresh Notes

## Overview
- Updated `animation_telemetry_service` to present zones as non-overlapping boxes laid out in a tree derived from `parentZone` metadata and service creation events.
- Nodes that belong to a zone (impls, stubs, proxies, etc.) are now clamped within their parent zone’s bounds, preserving visual hierarchy while the force simulation runs.
- Playback timeline is scaled to a readable default duration and exposes a speed selector (0.25×–2×) so long traces can be reviewed interactively.
- Channel routing events now draw dashed `channel_route` edges whenever add-ref telemetry includes `destinationChannelZone` / `callerChannelZone`, making intermediary hops explicit.
- Interface proxy lifecycles collapse to a single node per `(zone, destination zone, object)`; deletions now remove the node and associated impl links cleanly.
- The event log can be collapsed to reclaim space, and the SVG canvas resizes with the viewport to keep the network view full-screen.
- Added per-type visibility toggles so you can focus on object/interface proxies (default) or bring back service proxies, stubs, impls, and activity pulses as needed. Hidden node types also hide their incident edges.
- Event log now lives in a collapsible side panel; it starts hidden by default to maximise canvas space but can be expanded on demand.

## Zone Layout Details
- `ensureZoneNode` hydrates missing ancestors, tracks metadata names, and triggers `recomputeZoneLayout`.
- `recomputeZoneLayout` builds a parent→children map, assigns column order via a depth-first traversal, and positions each zone deterministically (`zoneLayout[x,y]`).
- `targetX/Y` and tick clamping ensure child nodes remain inside the rounded rectangle representing their zone, with vertical spacing scaled to keep multi-level trees readable.
- Zones that receive `service_deletion` events are removed from the layout immediately; subsequent telemetry will not resurrect them unless a new `service_creation` arrives. The root placeholder remains as `Zone 0 (root)` to distinguish it from real services such as `host`.

## Channel Routing Visualisation
- `updateProxyCounts` inspects add-ref telemetry to synthesize `channel_route` links from the proxy to the channel zone, and `route` links from channel zones to destination zones.
- Caller channel zones also gain bridging links so both legs of the hop are visible.
- Styling for `channel_route` uses a blue dashed line to distinguish it from direct `route` links.
- Channel and bridge edges are reference-counted, so they disappear automatically once every owning proxy is released or the parent zone is torn down.

## Playback Controls
- Controls bar now includes a `<select id="speed-select">` with presets 0.25×, 0.5×, 1×, 1.5×, 2×.
- Timer delta incorporates the selected multiplier; the tooltip on the clock still exposes the actual captured time span for reference.
- Display timeline defaults to ~0.5 s per event (minimum 8 s) but still supports scrubbing to any point.

## Regenerating Artifacts
1. Rebuild the host telemetry target to pick up changes:
   - `cmake --build build`
2. Rerun the relevant tests to reproduce telemetry output (e.g. `ctest --test-dir build --output-on-failure` or the specific suite).
3. Open the regenerated HTML in `build/animation/...` to validate zone layout, channel routing, and the speed control.

## Follow-Ups / Ideas
- Consider storing channel route edges only while active to reduce visual clutter in long traces.
- Layer labels within zone rectangles (e.g. ref counts, outstanding proxies) if additional metrics become available.
- Provide a legend entry for channel routes to clarify the new dashed blue edges.
