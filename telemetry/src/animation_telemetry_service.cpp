/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include <rpc/telemetry/animation_telemetry_service.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace
{
    constexpr const char* kAnimationStyles = R"CSS(
body {
    font-family: "Segoe UI", Roboto, sans-serif;
    margin: 0;
    background-color: #0f172a;
    color: #e2e8f0;
    display: flex;
    flex-direction: column;
    min-height: 100vh;
    overflow: hidden;
}

.header {
    padding: 16px 24px 8px 24px;
    background: linear-gradient(90deg, #1e293b 0%, #0f172a 80%);
    box-shadow: 0 2px 4px rgba(15, 23, 42, 0.6);
}

.header h1 {
    margin: 0;
    font-size: 22px;
    letter-spacing: 0.02em;
}

.header .subtitle {
    margin-top: 4px;
    font-size: 14px;
    color: #94a3b8;
}

.controls {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 12px 24px;
    background-color: #111c3a;
    border-bottom: 1px solid rgba(148, 163, 184, 0.1);
    flex-wrap: wrap;
}

.controls button {
    background-color: #2563eb;
    color: #e2e8f0;
    border: none;
    padding: 6px 14px;
    border-radius: 6px;
    font-size: 14px;
    cursor: pointer;
    transition: background-color 0.15s ease-in-out, transform 0.15s ease-in-out;
}

.controls button:hover {
    background-color: #1d4ed8;
    transform: translateY(-1px);
}

.controls button:disabled {
    opacity: 0.5;
    cursor: default;
    transform: none;
}

.controls .speed-control {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    font-size: 13px;
    color: #cbd5f5;
}

.controls select {
    background-color: #1e293b;
    color: #e2e8f0;
    border: 1px solid rgba(148, 163, 184, 0.4);
    border-radius: 6px;
    padding: 4px 6px;
    font-size: 13px;
}

.controls input[type="range"] {
    width: 320px;
}

.controls .time-label {
    font-variant-numeric: tabular-nums;
    min-width: 80px;
    text-align: right;
}

.type-filters {
    display: inline-flex;
    align-items: center;
    gap: 10px;
    flex-wrap: wrap;
}

.type-filters .filter-title {
    font-size: 12px;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    color: #94a3b8;
}

.type-filters .filter-item {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    font-size: 12px;
    background-color: rgba(30, 58, 138, 0.35);
    padding: 4px 8px;
    border-radius: 6px;
}

.type-filters .filter-item input[type="checkbox"] {
    accent-color: #2563eb;
    width: 14px;
    height: 14px;
}

#main-layout {
    display: flex;
    flex: 1 1 auto;
    min-height: 0;
    width: 100%;
    overflow: hidden;
}

#viz-container {
    position: relative;
    flex: 1 1 auto;
    min-height: 480px;
    background-color: #0b1120;
    overflow: hidden;
}

#viz-container .overlay-hint {
    position: absolute;
    top: 12px;
    right: 16px;
    background: rgba(15, 23, 42, 0.8);
    padding: 6px 12px;
    border-radius: 6px;
    font-size: 12px;
    color: #cbd5f5;
}

#event-panel {
    display: flex;
    flex-direction: column;
    background-color: #0f172a;
    border-left: 1px solid rgba(148, 163, 184, 0.1);
    width: 320px;
    max-width: 40vw;
    padding: 12px 24px 24px 24px;
    transition: width 0.2s ease-in-out, opacity 0.2s ease-in-out, padding 0.2s ease-in-out;
    overflow: hidden;
    position: relative;
    z-index: 10;
}

#event-panel h2 {
    margin: 0;
    font-size: 16px;
    color: #cbd5f5;
}

.event-log-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    margin-bottom: 12px;
}

.event-log-toggle {
    background-color: #1e293b;
    color: #e2e8f0;
    border: 1px solid rgba(148, 163, 184, 0.4);
    padding: 4px 10px;
    border-radius: 6px;
    font-size: 12px;
    cursor: pointer;
    transition: background-color 0.15s ease-in-out, transform 0.15s ease-in-out;
}

.event-log-toggle:hover {
    background-color: #334155;
    transform: translateY(-1px);
}

#event-log-body {
    flex: 1;
    overflow-y: auto;
    transition: max-height 0.2s ease-in-out, opacity 0.2s ease-in-out, padding 0.2s ease-in-out;
    min-height: 0;
}

.event-log-body-collapsed {
    flex: 0 !important;
    max-height: 0 !important;
    padding-top: 0 !important;
    padding-bottom: 0 !important;
    opacity: 0;
    overflow: hidden !important;
}

.event-panel-collapsed {
    width: 0 !important;
    padding: 12px 0 !important;
    opacity: 0;
    pointer-events: none;
    border-left: none;
}

#event-log {
    list-style: none;
    margin: 0;
    padding: 0;
    display: block;
}

#event-log li {
    padding: 8px 10px;
    border-radius: 6px;
    background: rgba(15, 23, 42, 0.75);
    box-shadow: inset 0 0 0 1px rgba(37, 99, 235, 0.2);
    font-size: 12px;
    margin-bottom: 6px;
}

#event-log li .timestamp {
    color: #94a3b8;
    font-variant-numeric: tabular-nums;
}

.node circle, .node rect {
    stroke: rgba(226, 232, 240, 0.35);
    stroke-width: 1.2px;
}

.node text {
    fill: #f8fafc;
    pointer-events: none;
    font-size: 12px;
    text-anchor: middle;
}

.node.zone rect {
    rx: 12px;
    ry: 12px;
}

.node.zone .zone-background {
    fill: rgba(56, 189, 248, 0.08);
    stroke: rgba(56, 189, 248, 0.35);
    stroke-width: 1.6px;
}

.node.zone .zone-label {
    fill: #38bdf8;
    font-size: 13px;
    font-weight: 600;
    letter-spacing: 0.08em;
    text-transform: uppercase;
}

.node.zone .zone-sublabel {
    fill: #94a3b8;
    font-size: 11px;
}

.zone-anchor-checkbox {
    position: absolute;
    top: 5px;
    right: 5px;
    width: 16px;
    height: 16px;
    accent-color: #2563eb;
    background-color: rgba(15, 23, 42, 0.8);
    border: 1px solid rgba(148, 163, 184, 0.4);
    border-radius: 3px;
    cursor: pointer;
    z-index: 20;
}

.link {
    stroke: rgba(148, 163, 184, 0.25);
    stroke-width: 1.4px;
}

.link.route {
    stroke-dasharray: 4 2;
    stroke: rgba(14, 165, 233, 0.6);
}

.link.contains {
    stroke: rgba(226, 232, 240, 0.2);
}

.link.activity {
    stroke: rgba(249, 115, 22, 0.95);
    stroke-width: 3.5px;
}

.link.channel_route {
    stroke: rgba(59, 130, 246, 0.7);
    stroke-width: 1.6px;
    stroke-dasharray: 6 3;
}

.link.impl_route {
    stroke: rgba(244, 114, 182, 0.75);
    stroke-width: 1.6px;
    stroke-dasharray: 3 2;
}

.legend {
    display: flex;
    gap: 16px;
    padding: 12px 24px;
    background: rgba(15, 23, 42, 0.95);
    border-top: 1px solid rgba(148, 163, 184, 0.08);
    font-size: 12px;
    align-items: center;
    flex-wrap: wrap;
}

.legend .item {
    display: inline-flex;
    align-items: center;
    gap: 6px;
}

.legend .swatch {
    width: 14px;
    height: 14px;
    border-radius: 50%;
    display: inline-block;
}

.legend .swatch.zone {
    border-radius: 4px;
}

.info-banner {
    font-size: 12px;
    color: #94a3b8;
}
)CSS";

    constexpr const char* kAnimationScript = R"JS(
(function() {
    const width = 1280;
    const height = 720;
    const levelSpacing = 120;
    const paddingBottom = 80;
    const maxLogEntries = 60;
    const epsilon = 1e-6;

    const palette = {
        zone: '#38bdf8',
        service_proxy: '#a855f7',
        object_proxy: '#fb7185',
        interface_proxy: '#fbbf24',
        impl: '#22d3ee',
        stub: '#4ade80',
        activity: '#f97316'
    };

    const levelByType = {
        zone: 0,
        service_proxy: 1,
        object_proxy: 2,
        stub: 2,
        impl: 3,
        interface_proxy: 3,
        activity: 4
    };

    const maxNodeLevel = Math.max(...Object.values(levelByType));

    const nodeRadiusByType = {
        zone: 28,
        service_proxy: 18,
        object_proxy: 16,
        stub: 16,
        impl: 14,
        interface_proxy: 14,
        activity: 10
    };

    const linkStrengthByType = {
        contains: 0.8,
        route: 0.4,
        activity: 0.2,
        impl_route: 0.5,
        channel_route: 0.5
    };

    const zoneBoxWidth = 260;
    const zoneBoxHeight = 280;
    const zoneLabelOffset = -zoneBoxHeight / 2 + 18;
    const zoneColumnSpacing = zoneBoxWidth + 160;
    let zoneVerticalSpacing = zoneBoxHeight + 160;
    let canvasWidth = width;
    let canvasHeight = height;

    const nodes = new Map();
    const links = new Map();
    const zoneLayout = new Map();
    const implByAddress = new Map();
    const objectToImpl = new Map();
    const interfaceToImplLink = new Map();
    const stubByObject = new Map();
    const interfaceProxyIndex = new Map();
    const interfaceProxyKeyById = new Map();
    const proxyLinkIndex = new Map();
    const linkUsage = new Map();
    const activeZones = new Set([1]);
    const nodeTypeFilters = [
        { key: 'service_proxy', label: 'Service Proxies', defaultVisible: false },
        { key: 'object_proxy', label: 'Object Proxies', defaultVisible: false },
        { key: 'interface_proxy', label: 'Interface Proxies', defaultVisible: true },
        { key: 'stub', label: 'Stubs', defaultVisible: false },
        { key: 'impl', label: 'Implementations', defaultVisible: true },
        { key: 'activity', label: 'Activity Pulses', defaultVisible: false }
    ];
    const typeVisibility = new Map();
    nodeTypeFilters.forEach((filter) => typeVisibility.set(filter.key, filter.defaultVisible));
    const zoneAliases = new Map();
    let primaryZoneId = null;

    function normalizeZoneNumber(zoneNumber) {
        if (zoneNumber === undefined || zoneNumber === null || Number.isNaN(zoneNumber)) {
            return primaryZoneId || 1;
        }
        if (zoneNumber === 0) {
            return primaryZoneId || 1;
        }
        return zoneNumber;
    }

    function noteZoneAlias(zoneNumber, name) {
        if (!name) {
            return;
        }
        if (!zoneAliases.has(zoneNumber)) {
            zoneAliases.set(zoneNumber, new Set());
        }
        const aliases = zoneAliases.get(zoneNumber);
        const previousSize = aliases.size;
        aliases.add(name);
        if (aliases.size !== previousSize && nodes.has(`zone-${zoneNumber}`)) {
            const zoneNode = nodes.get(`zone-${zoneNumber}`);
            if (zoneNode) {
                zoneNode.label = formatZoneLabel(zoneNumber);
            }
        }
    }

    function formatZoneLabel(zoneNumber) {
        const aliases = Array.from(zoneAliases.get(zoneNumber) || []);
        if (aliases.length === 0) {
            return `Zone ${zoneNumber}`;
        }
        const [primary, ...rest] = aliases;
        if (rest.length === 0) {
            return primary;
        }
        return `${primary} (aka ${rest.join(', ')})`;
    }

    function isNodeVisible(node) {
        if (!node) {
            return false;
        }
        if (node.type === 'zone') {
            return true;
        }
        if (!typeVisibility.has(node.type)) {
            return true;
        }
        return typeVisibility.get(node.type);
    }

    function resolveNodeId(endpoint) {
        if (!endpoint) {
            return null;
        }
        if (typeof endpoint === 'object') {
            return endpoint.id;
        }
        return endpoint;
    }

    function toZoneId(value) {
        if (value === undefined || value === null) {
            return null;
        }
        const numeric = Number(value);
        if (!Number.isFinite(numeric)) {
            return null;
        }
        return `zone-${numeric}`;
    }

    function ensureZoneNode(zoneNumberRaw) {
        const zoneNumber = normalizeZoneNumber(zoneNumberRaw);
        const zoneId = toZoneId(zoneNumber);
        if (!zoneId) {
            return null;
        }
        if (!nodes.has(zoneId)) {
            if (!activeZones.has(zoneNumber)) {
                return null;
            }
            const metadata = zoneMetadata[zoneNumber] || {};
            const parentNumber = metadata.parent !== undefined ? normalizeZoneNumber(metadata.parent) : null;
            let parentZoneId = toZoneId(parentNumber);
            if (parentNumber === undefined || parentNumber === null || parentNumber === zoneNumber) {
                parentZoneId = null;
            }
            if (parentZoneId && !nodes.has(parentZoneId)) {
                ensureZoneNode(parentNumber);
            }
            const initialLabel = formatZoneLabel(zoneNumber);
            const node = {
                id: zoneId,
                type: 'zone',
                label: initialLabel,
                zone: zoneNumber,
                parent: parentZoneId,
                refCount: 0
            };
            nodes.set(zoneId, node);
            if (parentZoneId && nodes.has(parentZoneId)) {
                const linkId = `zone-link-${parentZoneId}-${zoneId}`;
                if (!links.has(linkId)) {
                    links.set(linkId, {
                        id: linkId,
                        source: parentZoneId,
                        target: zoneId,
                        type: 'contains'
                    });
                }
            }
            recomputeZoneLayout();
        } else {
            const metadata = zoneMetadata[zoneNumber] || {};
            const node = nodes.get(zoneId);
            if (node) {
                if (metadata.name) {
                    noteZoneAlias(zoneNumber, metadata.name);
                }
                node.label = formatZoneLabel(zoneNumber);
            }
        }
        return zoneId;
    }

    function recomputeZoneLayout() {
        zoneLayout.clear();
        const zoneNodes = Array.from(nodes.values()).filter((node) => node.type === 'zone');
        if (zoneNodes.length === 0) {
            return;
        }
        const childrenByParent = new Map();
        const roots = [];
        zoneNodes.forEach((node) => {
            const parentId = node.parent && node.parent !== node.id ? node.parent : null;
            if (parentId && nodes.has(parentId)) {
                if (!childrenByParent.has(parentId)) {
                    childrenByParent.set(parentId, []);
                }
                childrenByParent.get(parentId).push(node);
            } else {
                roots.push(node);
            }
        });
        roots.sort((a, b) => (a.zone || 0) - (b.zone || 0));
        let currentColumn = 0;
        const seen = new Set();

        function assign(node, depth) {
            if (!node || seen.has(node.id)) {
                return;
            }
            seen.add(node.id);
            const children = (childrenByParent.get(node.id) || [])
                .slice()
                .sort((a, b) => (a.zone || 0) - (b.zone || 0));
            if (children.length === 0) {
                const order = currentColumn++;
                zoneLayout.set(node.id, { depth, order });
            } else {
                children.forEach((child) => assign(child, depth + 1));
                const childOrders = children
                    .map((child) => zoneLayout.get(child.id)?.order)
                    .filter((value) => value !== undefined);
                const order = childOrders.length > 0
                    ? childOrders.reduce((sum, value) => sum + value, 0) / childOrders.length
                    : currentColumn++;
                zoneLayout.set(node.id, { depth, order });
            }
        }

        roots.forEach((root) => assign(root, 0));

        if (zoneLayout.size === 0) {
            return;
        }

        const maxDepth = Math.max(...Array.from(zoneLayout.values()).map((layout) => layout.depth || 0));
        const baseY = canvasHeight - paddingBottom;
        const minY = zoneBoxHeight / 2 + 80;
        if (maxDepth > 0) {
            const available = Math.max(baseY - minY, zoneBoxHeight);
            const rawSpacing = available / Math.max(maxDepth, 1);
            const minSpacing = Math.max(zoneBoxHeight * 0.6, 140);
            const maxSpacing = zoneBoxHeight + 240;
            let spacing = rawSpacing;
            if (spacing < minSpacing) {
                spacing = Math.min(minSpacing, available / Math.max(maxDepth, 1));
            }
            spacing = Math.min(spacing, maxSpacing);
            if (spacing * maxDepth > available + 1) {
                spacing = rawSpacing;
            }
            zoneVerticalSpacing = Math.max(spacing, zoneBoxHeight * 0.75);
        } else {
            zoneVerticalSpacing = zoneBoxHeight + 160;
        }

        const orders = Array.from(zoneLayout.values()).map((layout) => layout.order);
        const minOrder = Math.min(...orders);
        const orderOffset = minOrder < 0 ? -minOrder : 0;

        zoneLayout.forEach((layout, zoneId) => {
            const adjustedOrder = layout.order + orderOffset;
            const x = 160 + adjustedOrder * zoneColumnSpacing;
            const y = canvasHeight - paddingBottom - layout.depth * zoneVerticalSpacing;
            layout.x = x;
            layout.y = y;
            const zoneNode = nodes.get(zoneId);
            if (zoneNode) {
                zoneNode.fx = x;
                zoneNode.fy = y;
                zoneNode.x = x;
                zoneNode.y = y;
            }
        });
    }
    const hasDuration = totalDuration > epsilon;
    const desiredPlaybackSeconds = Math.max(events.length * 0.5, 8);
    const fallbackStep = desiredPlaybackSeconds / Math.max(events.length + 1, 2);
    const timeScale = hasDuration ? Math.max(desiredPlaybackSeconds / totalDuration, 1) : 1;
    const nominalDisplayDuration = hasDuration ? totalDuration * timeScale : desiredPlaybackSeconds;
    const eventDisplayTimes = hasDuration
        ? events.map((evt) => evt.timestamp * timeScale)
        : events.map((_, index) => (index + 1) * fallbackStep);
    const displayDuration = eventDisplayTimes.length > 0
        ? Math.max(eventDisplayTimes[eventDisplayTimes.length - 1], nominalDisplayDuration)
        : nominalDisplayDuration;
    ensureZoneNode(0);
    let processedIndex = 0;
    let processedDisplayTime = 0;
    let currentTime = 0;
    let timer = null;
    let playing = false;
    let playbackSpeed = 5;

    const vizContainer = d3.select('#viz-container');

    const svgRoot = vizContainer
        .append('svg')
        .attr('class', 'telemetry-svg');

    function resizeSvg() {
        const node = vizContainer.node();
        if (!node) {
            return;
        }
        const bounds = node.getBoundingClientRect();
        canvasWidth = Math.max(bounds.width, 640);
        canvasHeight = Math.max(bounds.height, 480);
        svgRoot
            .attr('width', canvasWidth)
            .attr('height', canvasHeight);
    }

    const zoomLayer = svgRoot.append('g');

    svgRoot.call(d3.zoom()
        .scaleExtent([0.2, 2.5])
        .on('zoom', (event) => {
            zoomLayer.attr('transform', event.transform);
        }));

    const linkLayer = zoomLayer.append('g').attr('class', 'links');
    const nodeLayer = zoomLayer.append('g').attr('class', 'nodes');

    const hoverTooltip = vizContainer
        .append('div')
        .attr('class', 'overlay-hint')
        .style('pointer-events', 'none')
        .style('opacity', 0);

    const simulation = d3.forceSimulation()
        .alphaDecay(0.02)  // Much slower decay for smoother animation
        .velocityDecay(0.8)  // Higher velocity decay to reduce oscillations
        .force('link', d3.forceLink()
            .id((d) => d.id)
            .distance((d) => 100 + (levelByType[d.type] || 0) * 20)
            .strength((d) => linkStrengthByType[d.type] || 0.2))
        .force('charge', d3.forceManyBody().strength(-160))
        .force('collide', d3.forceCollide().radius((d) => nodeRadiusByType[d.type] + 12))
        .force('x', d3.forceX().x((d) => targetX(d)).strength(0.05))  // Reduced strength for gentler positioning
        .force('y', d3.forceY().y((d) => targetY(d)).strength(0.25))  // Reduced strength for gentler positioning
        .stop();

    const anchoredZones = new Set();

    // Zone hierarchy tracking
    const zoneHierarchy = new Map(); // parentZone -> Set of childZones
    const zoneChildren = new Set();  // Set of all child zones

    const dragBehaviour = d3.drag()
        .on('start', (event, d) => {
            if (!event.active) simulation.alphaTarget(0.3).restart();
            d.fx = d.x;
            d.fy = d.y;
        })
        .on('drag', (event, d) => {
            d.fx = event.x;
            d.fy = event.y;
        })
        .on('end', (event, d) => {
            if (!event.active) simulation.alphaTarget(0);

            // Check if this zone is anchored
            if (d.type === 'zone' && anchoredZones.has(d.id)) {
                // Keep the position fixed for anchored zones
                d.fx = event.x;
                d.fy = event.y;
            } else {
                d.fx = null;
                d.fy = null;
            }
        });

    const timelineSlider = document.getElementById('timeline-slider');
    const timeLabel = document.getElementById('time-display');
    const startButton = document.getElementById('start-button');
    const stopButton = document.getElementById('stop-button');
    const resetButton = document.getElementById('reset-button');
    const speedSelect = document.getElementById('speed-select');
    const eventLog = document.getElementById('event-log');
    const eventLogBody = document.getElementById('event-log-body');
    const showLogCheckbox = document.getElementById('show-log-checkbox');
    const typeFiltersContainer = document.getElementById('type-filters');
    const eventPanel = document.getElementById('event-panel');
    let logCollapsed = false;

    if (typeFiltersContainer) {
        nodeTypeFilters.forEach((filter) => {
            const label = document.createElement('label');
            label.className = 'filter-item';
            const checkbox = document.createElement('input');
            checkbox.type = 'checkbox';
            checkbox.id = `filter-${filter.key}`;
            checkbox.checked = typeVisibility.get(filter.key);
            checkbox.dataset.typeKey = filter.key;
            const caption = document.createElement('span');
            caption.textContent = filter.label;
            label.appendChild(checkbox);
            label.appendChild(caption);
            typeFiltersContainer.appendChild(label);
            checkbox.addEventListener('change', () => {
                typeVisibility.set(filter.key, checkbox.checked);
                updateGraph();
                simulation.alpha(0.3).restart();
            });
        });
    }


    const sliderMax = Math.max(displayDuration, 0.001);
    timelineSlider.max = sliderMax.toFixed(3);
    timelineSlider.step = Math.max(sliderMax / 200, 0.001).toFixed(3);

    startButton.addEventListener('click', () => startPlayback());
    stopButton.addEventListener('click', () => stopPlayback());
    resetButton.addEventListener('click', () => {
        stopPlayback();
        resetState();
        setCurrentTime(0);
    });

    timelineSlider.addEventListener('input', (event) => {
        const target = parseFloat(event.target.value);
        seekTo(target);
    });

    if (speedSelect) {
        speedSelect.addEventListener('change', (event) => {
            const value = parseFloat(event.target.value);
            if (Number.isFinite(value) && value > 0) {
                playbackSpeed = value;
            }
        });
    }

    if (showLogCheckbox && eventLogBody && eventPanel) {
        showLogCheckbox.addEventListener('change', () => {
            logCollapsed = !showLogCheckbox.checked;
            if (logCollapsed) {
                eventLogBody.classList.add('event-log-body-collapsed');
                eventPanel.classList.add('event-panel-collapsed');
            } else {
                eventLogBody.classList.remove('event-log-body-collapsed');
                eventPanel.classList.remove('event-panel-collapsed');
            }
            resizeSvg();
            recomputeZoneLayout();
            updateGraph();
            simulation.alpha(0.3).restart();
        });
    }

    svgRoot.on('mousemove', (event) => {
        const [mx, my] = d3.pointer(event);
        const zone = findClosestZone(mx, my);
        if (zone) {
            hoverTooltip
                .style('opacity', 1)
                .html(makeTooltip(zone))
                .style('right', '16px')
                .style('top', '12px');
        } else {
            hoverTooltip.style('opacity', 0);
        }
    });

    svgRoot.on('mouseleave', () => hoverTooltip.style('opacity', 0));

    resizeSvg();
    recomputeZoneLayout();
    updateGraph();
    simulation.alpha(0.6).restart();

    window.addEventListener('resize', () => {
        resizeSvg();
        recomputeZoneLayout();
        updateGraph();
        simulation.alpha(0.3).restart();
    });
    renderLegend();

    function targetY(node) {
        if (node.type === 'zone') {
            const layout = zoneLayout.get(node.id);
            if (layout) {
                return layout.y;
            }
            return canvasHeight - paddingBottom;
        }
        const level = levelByType[node.type] ?? 0;
        if (node.parent && nodes.has(node.parent)) {
            const parent = nodes.get(node.parent);
            const parentY = parent.y ?? targetY(parent);
            const usableHeight = zoneBoxHeight - 160;
            const step = Math.max(usableHeight / Math.max(maxNodeLevel || 1, 1), 40);
            const baseY = parentY - zoneBoxHeight / 2 + 80;
            return baseY + level * step;
        }
        return canvasHeight - paddingBottom - level * levelSpacing;
    }

    function targetX(node) {
        if (node.type === 'zone') {
            const layout = zoneLayout.get(node.id);
            if (layout) {
                return layout.x;
            }
            return 160;
        }
        if (node.parent && nodes.has(node.parent)) {
            const parent = nodes.get(node.parent);
            return parent.x ?? targetX(parent);
        }
        return canvasWidth / 2;
    }

    function startPlayback() {
        if (playing || events.length === 0) {
            return;
        }
        playing = true;
        const start = performance.now();
        let last = start;
        timer = d3.timer((now) => {
            const deltaMs = now - last;
            last = now;
            const deltaSeconds = (deltaMs / 1000) * playbackSpeed;
            if (deltaSeconds <= 0) {
                return;
            }
            const proposed = currentTime + deltaSeconds;
            if (proposed >= displayDuration - epsilon) {
                seekTo(displayDuration);
                stopPlayback();
                return;
            }
            seekTo(proposed);
        });
        updateButtons();
    }

    function stopPlayback() {
        if (!playing) {
            return;
        }
        playing = false;
        if (timer) {
            timer.stop();
            timer = null;
        }
        updateButtons();
    }

    function updateButtons() {
        startButton.disabled = playing || events.length == 0;
        stopButton.disabled = !playing;
    }

    function resetState() {
        nodes.clear();
        links.clear();
        implByAddress.clear();
        objectToImpl.clear();
        interfaceToImplLink.clear();
        stubByObject.clear();
        interfaceProxyIndex.clear();
        interfaceProxyKeyById.clear();
        proxyLinkIndex.clear();
        linkUsage.clear();
        activeZones.clear();
        activeZones.add(primaryZoneId || 1);
        processedIndex = 0;
        processedDisplayTime = 0;
        clearLog();
        ensureZoneNode(0);
        resizeSvg();
        recomputeZoneLayout();
        updateGraph();
    }

    function setCurrentTime(value) {
        currentTime = Math.min(Math.max(value, 0), displayDuration);
        timelineSlider.value = currentTime.toFixed(3);
        const displaySeconds = currentTime;
        const actualSeconds = hasDuration && timeScale > 0 ? currentTime / timeScale : currentTime;
        timeLabel.textContent = `${displaySeconds.toFixed(2)}s`;
        timeLabel.setAttribute('title', `Actual timeline: ${actualSeconds.toFixed(6)}s`);
    }

    function seekTo(targetTime) {
        const clamped = Math.min(Math.max(targetTime, 0), displayDuration);
        if (clamped + epsilon < processedDisplayTime) {
            resetState();
        }
        processUntil(clamped);
        setCurrentTime(clamped);
    }

    function processUntil(targetDisplayTime) {
        while (
            processedIndex < events.length &&
            (eventDisplayTimes[processedIndex] ?? 0) <= targetDisplayTime + epsilon
        ) {
            applyEvent(events[processedIndex]);
            processedDisplayTime = eventDisplayTimes[processedIndex] ?? targetDisplayTime;
            processedIndex += 1;
        }
        updateGraph();
    }

    function applyEvent(evt) {
        switch (evt.type) {
        case 'service_creation':
            createZone(evt);
            break;
        case 'service_deletion':
            deleteZone(evt);
            break;
        case 'service_proxy_creation':
        case 'cloned_service_proxy_creation':
            createServiceProxy(evt);
            break;
        case 'service_proxy_deletion':
            deleteNode(makeServiceProxyId(evt.data));
            break;
        case 'service_proxy_add_ref':
        case 'service_proxy_release':
        case 'service_proxy_add_external_ref':
        case 'service_proxy_release_external_ref':
            updateProxyCounts(evt);
            break;
        case 'service_try_cast':
        case 'service_add_ref':
            detectZoneHierarchy(evt);
            pulseActivity(evt);
            break;
        case 'service_release':
        case 'service_proxy_try_cast':
            pulseActivity(evt);
            break;
        case 'impl_creation':
            createImpl(evt);
            break;
        case 'impl_deletion':
            deleteImpl(evt);
            break;
        case 'stub_creation':
            createStub(evt);
            break;
        case 'stub_deletion':
            deleteStub(evt);
            break;
        case 'stub_add_ref':
        case 'stub_release':
            updateStubCounts(evt);
            break;
        case 'object_proxy_creation':
            createObjectProxy(evt);
            break;
        case 'object_proxy_deletion':
            deleteNode(makeObjectProxyId(evt.data));
            break;
        case 'interface_proxy_creation':
            createInterfaceProxy(evt);
            break;
        case 'interface_proxy_deletion':
            deleteInterfaceProxy(evt);
            break;
        case 'interface_proxy_send':
        case 'stub_send':
            pulseActivity(evt);
            break;
        case 'message':
            appendLog(evt);
            break;
        default:
            appendLog(evt);
            break;
        }
    }

    function createZone(evt) {
        const rawZone = evt.data.zone;
        const zoneNumber = normalizeZoneNumber(rawZone);
        const zoneId = toZoneId(zoneNumber);
        if (!zoneId) {
            appendLog(evt);
            return;
        }
        if (!primaryZoneId && zoneNumber > 0) {
            primaryZoneId = zoneNumber;
        }
        activeZones.add(zoneNumber);
        if (evt.data.serviceName) {
            noteZoneAlias(zoneNumber, evt.data.serviceName);
        }
        const parentNumberRaw = evt.data.parentZone;
        const parentZoneNumber = parentNumberRaw === undefined || parentNumberRaw === null
            ? null
            : normalizeZoneNumber(parentNumberRaw);
        let parentZoneId = toZoneId(parentZoneNumber);
        if (parentZoneNumber === null || parentZoneNumber === zoneNumber) {
            parentZoneId = null;
        } else if (parentZoneId) {
            ensureZoneNode(parentZoneNumber);
        }
        const displayName = formatZoneLabel(zoneNumber);
        const existing = nodes.get(zoneId);
        const refCount = existing?.refCount || 0;
        const node = existing || { id: zoneId, type: 'zone' };
        node.label = displayName;
        node.zone = zoneNumber;
        node.parent = parentZoneId;
        node.refCount = refCount;
        nodes.set(zoneId, node);

        Array.from(links.entries()).forEach(([key, link]) => {
            if (link.type === 'contains' && link.target === zoneId && link.source !== parentZoneId) {
                links.delete(key);
            }
        });

        if (parentZoneId && nodes.has(parentZoneId)) {
            const linkId = `zone-link-${parentZoneId}-${zoneId}`;
            links.set(linkId, {
                id: linkId,
                source: parentZoneId,
                target: zoneId,
                type: 'contains'
            });
        }

        recomputeZoneLayout();
        appendLog(evt);
    }

    function deleteZone(evt) {
        const zoneNumber = normalizeZoneNumber(evt.data.zone);
        const zoneId = `zone-${zoneNumber}`;
        activeZones.delete(zoneNumber);
        const snapshot = Array.from(nodes.values());
        const stack = [zoneId];
        const toRemove = new Set();
        while (stack.length > 0) {
            const current = stack.pop();
            if (toRemove.has(current)) {
                continue;
            }
            toRemove.add(current);
            snapshot.forEach((candidate) => {
                if (!toRemove.has(candidate.id) && candidate.parent === current) {
                    stack.push(candidate.id);
                }
            });
        }
        Array.from(toRemove).forEach((id) => deleteNode(id));
        appendLog(evt);
    }

    function createServiceProxy(evt) {
        const zoneNumber = normalizeZoneNumber(evt.data.zone);
        const destinationZoneNumber = normalizeZoneNumber(evt.data.destinationZone);
        const callerZoneNumber = evt.data.callerZone === undefined || evt.data.callerZone === null
            ? zoneNumber
            : normalizeZoneNumber(evt.data.callerZone);
        const proxyKeyData = {
            ...evt.data,
            zone: zoneNumber,
            destinationZone: destinationZoneNumber,
            callerZone: callerZoneNumber
        };
        const id = makeServiceProxyId(proxyKeyData);
        const parentZoneId = ensureZoneNode(zoneNumber);
        const destinationZoneId = ensureZoneNode(destinationZoneNumber);
        const label = evt.data.serviceProxyName || evt.data.serviceName || 'service_proxy';
        const node = {
            id: id,
            type: 'service_proxy',
            label: label,
            parent: parentZoneId,
            zone: zoneNumber,
            destinationZone: destinationZoneNumber,
            refCount: 0,
            externalRefCount: 0,
            cloned: evt.type === 'cloned_service_proxy_creation'
        };
        nodes.set(id, node);
        releaseOwnedLinks(id);
        proxyLinkIndex.set(id, new Set());
        if (parentZoneId && nodes.has(parentZoneId)) {
            links.set(`${id}-parent`, {
                id: `${id}-parent`,
                source: parentZoneId,
                target: id,
                type: 'contains'
            });
        }
        if (destinationZoneId && nodes.has(destinationZoneId)) {
            const linkId = `${id}-route-${destinationZoneId}`;
            links.set(linkId, {
                id: linkId,
                source: id,
                target: destinationZoneId,
                type: 'route'
            });
        }
        appendLog(evt);
    }

    function updateProxyCounts(evt) {
        const zoneNumber = normalizeZoneNumber(evt.data.zone);
        const destinationZoneNumber = normalizeZoneNumber(evt.data.destinationZone);
        const callerZoneNumber = evt.data.callerZone === undefined || evt.data.callerZone === null
            ? zoneNumber
            : normalizeZoneNumber(evt.data.callerZone);
        const proxyKeyData = {
            ...evt.data,
            zone: zoneNumber,
            destinationZone: destinationZoneNumber,
            callerZone: callerZoneNumber
        };
        const id = makeServiceProxyId(proxyKeyData);
        if (!nodes.has(id)) {
            return;
        }
        const node = nodes.get(id);
        if (evt.type === 'service_proxy_add_ref') {
            node.refCount = (node.refCount || 0) + 1;
        } else if (evt.type === 'service_proxy_release' && node.refCount > 0) {
            node.refCount -= 1;
        } else if (evt.type === 'service_proxy_add_external_ref') {
            node.externalRefCount = evt.data.refCount;
        } else if (evt.type === 'service_proxy_release_external_ref') {
            node.externalRefCount = evt.data.refCount;
        }
        const channelZoneRaw = evt.data.destinationChannelZone;
        const callerChannelZoneRaw = evt.data.callerChannelZone;
        const channelZoneNumber = channelZoneRaw === undefined || channelZoneRaw === null
            ? null
            : normalizeZoneNumber(channelZoneRaw);
        const callerChannelZoneNumber = callerChannelZoneRaw === undefined || callerChannelZoneRaw === null
            ? null
            : normalizeZoneNumber(callerChannelZoneRaw);
        const parentZoneId = ensureZoneNode(zoneNumber);
        const destinationZoneId = ensureZoneNode(destinationZoneNumber);
        if (channelZoneNumber !== undefined && channelZoneNumber !== null) {
            const channelZoneId = ensureZoneNode(channelZoneNumber);
            if (channelZoneId) {
                const linkId = `${id}-channel-${channelZoneId}`;
                registerOwnedLink(id, linkId, {
                    id: linkId,
                    source: id,
                    target: channelZoneId,
                    type: 'channel_route'
                });
                if (destinationZoneId) {
                    const bridgeId = `channel-bridge-${channelZoneId}-${destinationZoneId}`;
                    registerOwnedLink(id, bridgeId, {
                        id: bridgeId,
                        source: channelZoneId,
                        target: destinationZoneId,
                        type: 'route'
                    });
                }
            }
        }
        if (callerChannelZoneNumber !== undefined && callerChannelZoneNumber !== null && parentZoneId) {
            const callerChannelZoneId = ensureZoneNode(callerChannelZoneNumber);
            if (callerChannelZoneId) {
                const callerBridgeId = `channel-bridge-${parentZoneId}-${callerChannelZoneId}`;
                registerOwnedLink(id, callerBridgeId, {
                    id: callerBridgeId,
                    source: parentZoneId,
                    target: callerChannelZoneId,
                    type: 'route'
                });
            }
        }
        appendLog(evt);
    }

    function createImpl(evt) {
        const zoneNumber = normalizeZoneNumber(evt.data.zone);
        const dataForId = { ...evt.data, zone: zoneNumber };
        const id = makeImplId(dataForId);
        const parentZoneId = ensureZoneNode(zoneNumber);
        const node = {
            id: id,
            type: 'impl',
            label: evt.data.name || `impl@${evt.data.address}`,
            parent: parentZoneId,
            zone: zoneNumber,
            address: evt.data.address,
            count: 1
        };
        nodes.set(id, node);
        if (parentZoneId && nodes.has(parentZoneId)) {
            links.set(`${id}-parent`, {
                id: `${id}-parent`,
                source: parentZoneId,
                target: id,
                type: 'contains'
            });
        }
        const addressKey = evt.data.address !== undefined && evt.data.address !== null ? String(evt.data.address) : null;
        if (addressKey) {
            implByAddress.set(addressKey, id);
            for (const [objectId, stubInfo] of stubByObject.entries()) {
                if (stubInfo.addressKey === addressKey) {
                    objectToImpl.set(objectId, id);
                    refreshInterfaceLinksForObject(objectId);
                }
            }
        }
        appendLog(evt);
    }

    function createStub(evt) {
        const zoneNumber = normalizeZoneNumber(evt.data.zone);
        const dataForId = { ...evt.data, zone: zoneNumber };
        const id = makeStubId(dataForId);
        const parentZoneId = ensureZoneNode(zoneNumber);
        const node = {
            id: id,
            type: 'stub',
            label: `stub ${evt.data.object}`,
            parent: parentZoneId,
            zone: zoneNumber,
            object: Number(evt.data.object),
            address: evt.data.address || null,
            refCount: 0
        };
        nodes.set(id, node);
        if (parentZoneId && nodes.has(parentZoneId)) {
            links.set(`${id}-parent`, {
                id: `${id}-parent`,
                source: parentZoneId,
                target: id,
                type: 'contains'
            });
        }
        const objectId = node.object;
        const addressKey = evt.data.address !== undefined && evt.data.address !== null ? String(evt.data.address) : null;
        stubByObject.set(objectId, { id, addressKey });
        if (addressKey) {
            const implId = implByAddress.get(addressKey);
            if (implId) {
                objectToImpl.set(objectId, implId);
                refreshInterfaceLinksForObject(objectId);
            }
        }
        appendLog(evt);
    }

    function updateStubCounts(evt) {
        const id = makeStubId(evt.data);
        if (!nodes.has(id)) {
            return;
        }
        const node = nodes.get(id);
        node.refCount = evt.data.count;
        appendLog(evt);
    }

    function createObjectProxy(evt) {
        const zoneNumber = normalizeZoneNumber(evt.data.zone);
        const destinationZoneNumber = normalizeZoneNumber(evt.data.destinationZone);
        const dataForId = { ...evt.data, zone: zoneNumber, destinationZone: destinationZoneNumber };
        const id = makeObjectProxyId(dataForId);
        const parentZoneId = ensureZoneNode(zoneNumber);
        const destinationZoneId = ensureZoneNode(destinationZoneNumber);
        const node = {
            id: id,
            type: 'object_proxy',
            label: `object_proxy ${evt.data.object}`,
            parent: parentZoneId,
            zone: zoneNumber,
            destinationZone: destinationZoneNumber,
            object: evt.data.object,
            addRefDone: !!evt.data.addRefDone
        };
        nodes.set(id, node);
        if (parentZoneId && nodes.has(parentZoneId)) {
            links.set(`${id}-parent`, {
                id: `${id}-parent`,
                source: parentZoneId,
                target: id,
                type: 'contains'
            });
        }
        if (destinationZoneId && nodes.has(destinationZoneId)) {
            const linkId = `${id}-route-${destinationZoneId}`;
            links.set(linkId, {
                id: linkId,
                source: id,
                target: destinationZoneId,
                type: 'route'
            });
        }
        appendLog(evt);
    }

    function createInterfaceProxy(evt) {
        const zoneNumber = normalizeZoneNumber(evt.data.zone);
        const destinationZoneNumber = normalizeZoneNumber(evt.data.destinationZone);
        const proxyData = {
            ...evt.data,
            zone: zoneNumber,
            destinationZone: destinationZoneNumber
        };
        const key = makeInterfaceProxyKey(proxyData);
        let id = makeInterfaceProxyId(proxyData);
        const parentZoneId = ensureZoneNode(zoneNumber);
        const destinationZoneId = ensureZoneNode(destinationZoneNumber);
        const label = evt.data.name || `if ${evt.data.interface}`;
        const objectId = Number(evt.data.object);
        const existingNode = nodes.get(id);
        const node = existingNode || {
            id: id,
            type: 'interface_proxy',
            label: label,
            parent: parentZoneId,
            zone: zoneNumber,
            destinationZone: destinationZoneNumber,
            object: objectId,
            interface: evt.data.interface
        };

        node.label = label;
        node.parent = parentZoneId;
        node.zone = zoneNumber;
        node.destinationZone = destinationZoneNumber;
        node.object = objectId;
        node.interface = evt.data.interface;
        nodes.set(id, node);

        interfaceProxyIndex.set(key, id);
        interfaceProxyKeyById.set(id, key);

        if (parentZoneId && nodes.has(parentZoneId)) {
            links.set(`${id}-parent`, {
                id: `${id}-parent`,
                source: parentZoneId,
                target: id,
                type: 'contains'
            });
        }
        if (destinationZoneId && nodes.has(destinationZoneId)) {
            const linkId = `${id}-route-${destinationZoneId}`;
            links.set(linkId, {
                id: linkId,
                source: id,
                target: destinationZoneId,
                type: 'route'
            });
        }

        refreshInterfaceLinksForObject(node.object);
        appendLog(evt);
    }

    function removeInterfaceLink(interfaceId) {
        if (!interfaceToImplLink.has(interfaceId)) {
            return;
        }
        const linkId = interfaceToImplLink.get(interfaceId);
        interfaceToImplLink.delete(interfaceId);
        links.delete(linkId);
    }

    function registerOwnedLink(ownerId, linkId, linkData) {
        if (ownerId) {
            let owned = proxyLinkIndex.get(ownerId);
            if (!owned) {
                owned = new Set();
                proxyLinkIndex.set(ownerId, owned);
            }
            if (owned.has(linkId)) {
                links.set(linkId, linkData);
                return;
            }
            owned.add(linkId);
        }
        let entry = linkUsage.get(linkId);
        if (!entry) {
            entry = { data: linkData, count: 0 };
            linkUsage.set(linkId, entry);
        }
        entry.count += 1;
        links.set(linkId, linkData);
    }

    function releaseOwnedLinks(ownerId) {
        const owned = proxyLinkIndex.get(ownerId);
        if (!owned) {
            return;
        }
        owned.forEach((linkId) => {
            const entry = linkUsage.get(linkId);
            if (!entry) {
                links.delete(linkId);
                return;
            }
            entry.count -= 1;
            if (entry.count <= 0) {
                linkUsage.delete(linkId);
                links.delete(linkId);
            }
        });
        proxyLinkIndex.delete(ownerId);
    }

    function refreshInterfaceLinksForObject(objectId) {
        if (objectId === undefined || Number.isNaN(objectId)) {
            return;
        }
        const interfaceNodes = Array.from(nodes.values())
            .filter((node) => node.type === 'interface_proxy' && node.object === objectId);
        interfaceNodes.forEach((node) => removeInterfaceLink(node.id));
        const implId = objectToImpl.get(objectId);
        if (!implId || !nodes.has(implId)) {
            return;
        }
        interfaceNodes.forEach((node) => {
            const linkId = `${node.id}-impl-${implId}`;
            links.set(linkId, {
                id: linkId,
                source: node.id,
                target: implId,
                type: 'impl_route'
            });
            interfaceToImplLink.set(node.id, linkId);
        });
    }

    function deleteImpl(evt) {
        const id = makeImplId(evt.data);
        deleteNode(id);
        appendLog(evt);
    }

    function deleteStub(evt) {
        const id = makeStubId(evt.data);
        deleteNode(id);
        appendLog(evt);
    }

    function deleteInterfaceProxy(evt) {
        const key = makeInterfaceProxyKey(evt.data);
        const id = interfaceProxyIndex.get(key) || makeInterfaceProxyId(evt.data);
        if (interfaceProxyIndex.get(key) === id) {
            interfaceProxyIndex.delete(key);
        }
        interfaceProxyKeyById.delete(id);
        deleteNode(id);
        appendLog(evt);
    }

    function detectZoneHierarchy(evt) {
        // Zone hierarchy detection based on service_add_ref events
        // Higher zone numbers are children of lower zone numbers
        const destinationZone = normalizeZoneNumber(evt.data.destinationZone);
        const callerChannelZone = normalizeZoneNumber(evt.data.callerChannelZone);

        if (destinationZone && callerChannelZone && destinationZone !== callerChannelZone) {
            let parentZone, childZone;

            // Determine parent-child relationship: higher number = child, lower number = parent
            if (destinationZone > callerChannelZone) {
                parentZone = callerChannelZone;
                childZone = destinationZone;
            } else {
                parentZone = destinationZone;
                childZone = callerChannelZone;
            }

            // Store the hierarchy relationship
            if (!zoneHierarchy.has(parentZone)) {
                zoneHierarchy.set(parentZone, new Set());
            }
            zoneHierarchy.get(parentZone).add(childZone);

            // Mark child zone for higher level positioning
            zoneChildren.add(childZone);

            // Update zone positioning based on hierarchy
            updateZoneHierarchyPositioning();
        }
    }

    function updateZoneHierarchyPositioning() {
        // Recalculate zone layout considering hierarchy
        zoneLayout.clear();
        const zoneNodes = Array.from(nodes.values()).filter((node) => node.type === 'zone');
        if (zoneNodes.length === 0) {
            return;
        }

        // Create hierarchy-based depth map
        const hierarchyDepths = new Map();

        // Initialize all zones at depth 0
        zoneNodes.forEach((node) => {
            hierarchyDepths.set(node.zone, 0);
        });

        // Apply hierarchy: child zones get higher depth than parents
        zoneHierarchy.forEach((children, parentZone) => {
            const parentDepth = hierarchyDepths.get(parentZone) || 0;
            children.forEach((childZone) => {
                const currentChildDepth = hierarchyDepths.get(childZone) || 0;
                // Child zones should be at least one level higher than parent
                hierarchyDepths.set(childZone, Math.max(currentChildDepth, parentDepth + 1));
            });
        });

        // Sort zones by hierarchy depth and zone number
        const roots = [];
        const childrenByParent = new Map();

        zoneNodes.forEach((node) => {
            // Check if this zone has a hierarchical parent
            let hasHierarchyParent = false;
            for (const [parentZone, children] of zoneHierarchy.entries()) {
                if (children.has(node.zone)) {
                    hasHierarchyParent = true;
                    const parentNode = zoneNodes.find(n => n.zone === parentZone);
                    if (parentNode) {
                        if (!childrenByParent.has(parentNode.id)) {
                            childrenByParent.set(parentNode.id, []);
                        }
                        childrenByParent.get(parentNode.id).push(node);
                    }
                    break;
                }
            }

            if (!hasHierarchyParent) {
                roots.push(node);
            }
        });

        roots.sort((a, b) => (a.zone || 0) - (b.zone || 0));
        let currentColumn = 0;
        const seen = new Set();

        function assign(node, depth) {
            if (!node || seen.has(node.id)) {
                return;
            }
            seen.add(node.id);

            // Use hierarchy depth if available
            const hierarchyDepth = hierarchyDepths.get(node.zone);
            const actualDepth = hierarchyDepth !== undefined ? hierarchyDepth : depth;

            const children = (childrenByParent.get(node.id) || [])
                .slice()
                .sort((a, b) => (a.zone || 0) - (b.zone || 0));

            if (children.length === 0) {
                const order = currentColumn++;
                zoneLayout.set(node.id, { depth: actualDepth, order });
            } else {
                children.forEach((child) => assign(child, actualDepth + 1));
                const childOrders = children
                    .map((child) => zoneLayout.get(child.id)?.order)
                    .filter((value) => value !== undefined);
                const order = childOrders.length > 0
                    ? childOrders.reduce((sum, value) => sum + value, 0) / childOrders.length
                    : currentColumn++;
                zoneLayout.set(node.id, { depth: actualDepth, order });
            }
        }

        roots.forEach((root) => assign(root, hierarchyDepths.get(root.zone) || 0));

        // Finalize positioning
        if (zoneLayout.size === 0) {
            return;
        }

        const maxDepth = Math.max(...Array.from(zoneLayout.values()).map((layout) => layout.depth || 0));
        const baseY = canvasHeight - paddingBottom;
        const minY = zoneBoxHeight / 2 + 80;

        let zoneVerticalSpacing;
        if (maxDepth > 0) {
            const available = Math.max(baseY - minY, zoneBoxHeight);
            const rawSpacing = available / Math.max(maxDepth, 1);
            const minSpacing = Math.max(zoneBoxHeight * 0.6, 140);
            const maxSpacing = zoneBoxHeight + 240;
            let spacing = rawSpacing;
            if (spacing < minSpacing) {
                spacing = Math.min(minSpacing, available / Math.max(maxDepth, 1));
            }
            spacing = Math.min(spacing, maxSpacing);
            if (spacing * maxDepth > available + 1) {
                spacing = rawSpacing;
            }
            zoneVerticalSpacing = Math.max(spacing, zoneBoxHeight * 0.75);
        } else {
            zoneVerticalSpacing = zoneBoxHeight + 160;
        }

        const orders = Array.from(zoneLayout.values()).map((layout) => layout.order);
        const minOrder = Math.min(...orders);
        const orderOffset = minOrder < 0 ? -minOrder : 0;

        zoneLayout.forEach((layout, zoneId) => {
            const adjustedOrder = layout.order + orderOffset;
            const x = 160 + adjustedOrder * zoneColumnSpacing;
            const y = canvasHeight - paddingBottom - layout.depth * zoneVerticalSpacing;
            layout.x = x;
            layout.y = y;
        });

        // Restart simulation to apply new positions
        if (simulation) {
            simulation.alpha(0.3).restart();
        }
    }

    function pulseActivity(evt) {
        const zoneNumber = normalizeZoneNumber(evt.data.zone);
        const targetZoneNumber = normalizeZoneNumber(evt.data.destinationZone || evt.data.destinationChannelZone || evt.data.callerZone);

        // Create a temporary activity link that will be visible for a short time
        if (zoneNumber !== targetZoneNumber && targetZoneNumber) {
            const activityId = `activity-${evt.type}-${evt.timestamp}-${Math.random()}`;
            const activityLink = {
                id: activityId,
                type: 'activity',
                source: `zone-${zoneNumber}`,
                target: `zone-${targetZoneNumber}`,
                label: evt.type.replace(/_/g, ' '),
                timestamp: evt.timestamp,
                temporary: true
            };

            // Add the activity link temporarily
            links.set(activityId, activityLink);

            // Schedule removal of the activity pulse after 2 seconds
            setTimeout(() => {
                links.delete(activityId);
                updateGraph();
            }, 2000);

            updateGraph();
        }

        appendLog(evt);
    }

    function deleteNode(id) {
        if (!nodes.has(id)) {
            return;
        }
        const node = nodes.get(id);
        if (!node) {
            return;
        }
        if (node.type === 'interface_proxy') {
            removeInterfaceLink(id);
            const key = interfaceProxyKeyById.get(id);
            if (key) {
                interfaceProxyKeyById.delete(id);
                if (interfaceProxyIndex.get(key) === id) {
                    interfaceProxyIndex.delete(key);
                }
            }
        } else if (node.type === 'stub') {
            const objectId = Number(node.object);
            stubByObject.delete(objectId);
            objectToImpl.delete(objectId);
            refreshInterfaceLinksForObject(objectId);
        } else if (node.type === 'service_proxy') {
            releaseOwnedLinks(id);
        } else if (node.type === 'impl') {
            if (node.address !== undefined && node.address !== null) {
                implByAddress.delete(String(node.address));
            }
            for (const [objectId, mappedId] of Array.from(objectToImpl.entries())) {
                if (mappedId === id) {
                    objectToImpl.delete(objectId);
                    refreshInterfaceLinksForObject(Number(objectId));
                }
            }
        } else if (node.type === 'zone') {
            zoneLayout.delete(id);
        }
        nodes.delete(id);
        Array.from(links.keys()).forEach((key) => {
            const link = links.get(key);
            if (link.source === id || link.target === id) {
                links.delete(key);
            }
        });
        if (node.type === 'zone') {
            recomputeZoneLayout();
        }
    }

    function makeServiceProxyId(data) {
        const zone = normalizeZoneNumber(data.zone);
        const destination = normalizeZoneNumber(data.destinationZone);
        const callerRaw = data.callerZone;
        const caller = callerRaw === undefined || callerRaw === null
            ? 'self'
            : normalizeZoneNumber(callerRaw);
        return `service-proxy-${zone}-${destination}-${caller}`;
    }

    function makeStubId(data) {
        const zone = normalizeZoneNumber(data.zone);
        return `stub-${zone}-${data.object}`;
    }

    function makeImplId(data) {
        const zone = normalizeZoneNumber(data.zone);
        return `impl-${zone}-${data.address}`;
    }

    function makeObjectProxyId(data) {
        const zone = normalizeZoneNumber(data.zone);
        const destination = normalizeZoneNumber(data.destinationZone);
        return `object-proxy-${zone}-${destination}-${data.object}`;
    }

    function makeInterfaceProxyKey(data) {
        const zone = normalizeZoneNumber(data.zone);
        const destination = normalizeZoneNumber(data.destinationZone);
        return `${zone}-${destination}-${data.object}`;
    }

    function makeInterfaceProxyId(data) {
        const key = makeInterfaceProxyKey(data);
        const existing = interfaceProxyIndex.get(key);
        if (existing) {
            return existing;
        }
        return `interface-proxy-${key}`;
    }

    function findClosestZone(x, y) {
        const candidates = Array.from(nodes.values()).filter((node) => node.type === 'zone');
        if (candidates.length === 0) {
            return null;
        }
        let closest = null;
        let distance = Infinity;
        for (const node of candidates) {
            const dx = (node.x ?? 0) - x;
            const dy = (node.y ?? 0) - y;
            const dist = Math.sqrt(dx * dx + dy * dy);
            if (dist < distance && dist < 120) {
                distance = dist;
                closest = node;
            }
        }
        return closest;
    }

    function makeTooltip(node) {
        const metadata = zoneMetadata[node.zone] || {};
        const parentZoneRaw = metadata.parent !== undefined ? normalizeZoneNumber(metadata.parent) : null;
        const parentDesc = parentZoneRaw && parentZoneRaw !== node.zone
            ? `parent zone ${parentZoneRaw}`
            : 'top-level zone';
        return `<strong>${formatZoneLabel(node.zone)}</strong><br/>${parentDesc}`;
    }

    function appendLog(evt) {
        const li = document.createElement('li');
        const time = evt.timestamp.toFixed(3).padStart(7, ' ');
        li.innerHTML = `<div class="timestamp">${time}s · ${evt.type}</div><div>${formatEventSummary(evt)}</div>`;
        eventLog.appendChild(li);
        while (eventLog.childNodes.length > maxLogEntries) {
            eventLog.removeChild(eventLog.firstChild);
        }
        if (eventLogBody && !logCollapsed) {
            eventLogBody.scrollTop = eventLogBody.scrollHeight;
        }
    }

    function clearLog() {
        eventLog.textContent = '';
    }

    function formatEventSummary(evt) {
        const data = evt.data;
        switch (evt.type) {
        case 'service_creation':
            return `zone ${data.zone} created (parent ${data.parentZone})`;
        case 'service_deletion':
            return `zone ${data.zone} deleted`;
        case 'service_proxy_creation':
        case 'cloned_service_proxy_creation':
            return `service proxy ${data.serviceProxyName || ''} from zone ${data.zone} to ${data.destinationZone}`;
        case 'service_proxy_deletion':
            return `service proxy removed in zone ${data.zone}`;
        case 'impl_creation':
            return `impl ${data.name} created @${data.address} in zone ${data.zone}`;
        case 'impl_deletion':
            return `impl ${data.address} removed from zone ${data.zone}`;
        case 'stub_creation':
            return `stub ${data.object} created in zone ${data.zone}`;
        case 'stub_deletion':
            return `stub ${data.object} removed from zone ${data.zone}`;
        case 'object_proxy_creation':
            return `object proxy ${data.object} from zone ${data.zone} to ${data.destinationZone}`;
        case 'interface_proxy_creation':
            return `interface proxy ${data.name} on object ${data.object}`;
        case 'message':
            return data.message || 'telemetry message';
        default:
            return Object.keys(data || {}).map((key) => `${key}: ${data[key]}`).join(', ');
        }
    }

    function renderLegend() {
        const legend = d3.select('body').append('div').attr('class', 'legend');
        const info = legend.append('div').attr('class', 'info-banner');
        info.html('Zoom with mouse wheel · Pan by dragging the canvas · Drag nodes to pin positions');
        const items = legend.append('div');
        const groups = [
            { key: 'zone', label: 'Zone (Service)' },
            { key: 'service_proxy', label: 'Service Proxy' },
            { key: 'object_proxy', label: 'Object Proxy' },
            { key: 'interface_proxy', label: 'Interface Proxy' },
            { key: 'impl', label: 'Implementation Object' },
            { key: 'stub', label: 'Stub' }
        ];
        items.selectAll('span.item')
            .data(groups)
            .enter()
            .append('span')
            .attr('class', 'item')
            .html((d) => {
                const extraClass = d.key === 'zone' ? ' zone' : '';
                return `<span class="swatch${extraClass}" style="background:${palette[d.key]}"></span>${d.label}`;
            });
    }

    function updateGraph() {
        const nodeArray = Array.from(nodes.values());
        const visibleNodeArray = nodeArray.filter((node) => isNodeVisible(node));
        const visibleNodeIds = new Set(visibleNodeArray.map((node) => node.id));
        const linkArray = Array.from(links.values());
        const visibleLinkArray = linkArray.filter((link) => {
            const sourceId = resolveNodeId(link.source);
            const targetId = resolveNodeId(link.target);
            return visibleNodeIds.has(sourceId) && visibleNodeIds.has(targetId);
        });

        simulation.nodes(visibleNodeArray);
        simulation.force('link').links(visibleLinkArray);

        const linkSelection = linkLayer.selectAll('line')
            .data(visibleLinkArray, (d) => d.id);

        linkSelection
            .enter()
            .append('line')
            .attr('stroke-width', 1.4)
            .attr('stroke-linecap', 'round');

        linkSelection.exit().remove();

        linkLayer.selectAll('line')
            .attr('class', (d) => `link ${d.type || ''}`);

        const nodeSelection = nodeLayer.selectAll('g.node')
            .data(visibleNodeArray, (d) => d.id);

        const nodeEnter = nodeSelection
            .enter()
            .append('g')
            .attr('class', (d) => `node ${d.type}`)
            .call(dragBehaviour);

        nodeEnter.each(function(d) {
            const g = d3.select(this);
            if (d.type === 'zone') {
                g.append('rect')
                    .attr('class', 'zone-background')
                    .attr('x', -zoneBoxWidth / 2)
                    .attr('y', -zoneBoxHeight / 2)
                    .attr('width', zoneBoxWidth)
                    .attr('height', zoneBoxHeight);
                g.append('text')
                    .attr('class', 'zone-label node-label')
                    .attr('y', zoneLabelOffset)
                    .text(() => d.label || d.id);

                // Add anchor checkbox as foreignObject
                const foreignObject = g.append('foreignObject')
                    .attr('x', zoneBoxWidth / 2 - 25)
                    .attr('y', -zoneBoxHeight / 2 + 5)
                    .attr('width', 20)
                    .attr('height', 20);

                const checkbox = foreignObject.append('xhtml:input')
                    .attr('type', 'checkbox')
                    .attr('class', 'zone-anchor-checkbox')
                    .attr('id', `anchor-${d.id}`)
                    .style('pointer-events', 'all')
                    .on('click', function(event) {
                        event.stopPropagation();
                        const isChecked = this.checked;
                        if (isChecked) {
                            // Anchor this zone
                            anchoredZones.add(d.id);
                            d.fx = d.x;
                            d.fy = d.y;
                        } else {
                            // Unanchor this zone
                            anchoredZones.delete(d.id);
                            d.fx = null;
                            d.fy = null;
                            simulation.alpha(0.3).restart();
                        }
                    });
            } else {
                g.append('circle')
                    .attr('r', nodeRadiusByType[d.type] || 12)
                    .attr('fill', palette[d.type] || '#94a3b8')
                    .attr('opacity', 0.92);
                g.append('text')
                    .attr('class', 'node-label')
                    .attr('dy', 4)
                    .text(() => d.label || d.id);
            }
        });

        nodeSelection.exit().remove();

        nodeLayer.selectAll('g.node text.node-label')
            .text((d) => truncateLabel(d.label || d.id))
            .attr('y', (d) => (d.type === 'zone' ? zoneLabelOffset : 0))
            .attr('dy', (d) => (d.type === 'zone' ? 0 : 4));

        nodeLayer.selectAll('g.node.zone').each(function(d) {
            const g = d3.select(this);
            let sublabel = g.select('text.zone-sublabel');
            if (sublabel.empty()) {
                sublabel = g.append('text')
                    .attr('class', 'zone-sublabel')
                    .attr('fill', '#94a3b8')
                    .attr('font-size', 11);
            }
            sublabel
                .attr('y', zoneLabelOffset + 22)
                .attr('dy', 0)
                .text(() => `zone ${d.zone}`);
        });

        simulation.alpha(0.9).restart();

        simulation.on('tick', () => {
            visibleNodeArray.forEach((node) => {
                if (node.type !== 'zone' && node.parent && nodes.has(node.parent)) {
                    const parent = nodes.get(node.parent);
                    if (parent) {
                        const parentX = parent.x ?? targetX(parent);
                        const minX = parentX - zoneBoxWidth / 2 + 24;
                        const maxX = parentX + zoneBoxWidth / 2 - 24;
                        node.x = Math.min(Math.max(node.x, minX), maxX);
                        const parentY = parent.y ?? targetY(parent);
                        const minY = parentY - zoneBoxHeight / 2 + 70;
                        const maxY = parentY + zoneBoxHeight / 2 - 50;
                        node.y = Math.min(Math.max(node.y, minY), maxY);
                    }
                }
            });
            linkLayer.selectAll('line')
                .attr('x1', (d) => d.source.x)
                .attr('y1', (d) => d.source.y)
                .attr('x2', (d) => d.target.x)
                .attr('y2', (d) => d.target.y);

            nodeLayer.selectAll('g.node')
                .attr('transform', (d) => `translate(${d.x},${d.y})`);
        });
    }

    function truncateLabel(label) {
        if (!label) {
            return '';
        }
        return label.length > 18 ? `${label.slice(0, 15)}…` : label;
    }
})();
)JS";
}

namespace rpc
{
    namespace
    {
        constexpr const char* kTitlePrefix = "RPC++ Telemetry Animation";
    }

    bool animation_telemetry_service::create(std::shared_ptr<rpc::i_telemetry_service>& service,
        const std::string& test_suite_name,
        const std::string& name,
        const std::filesystem::path& directory)
    {
        auto fixed_suite = sanitize_name(test_suite_name);
        std::filesystem::path output_directory = directory / fixed_suite;
        std::error_code ec;
        std::filesystem::create_directories(output_directory, ec);

        auto output_path = output_directory / (name + ".html");

        service = std::shared_ptr<rpc::i_telemetry_service>(
            new animation_telemetry_service(output_path, test_suite_name, name));
        return true;
    }

    animation_telemetry_service::animation_telemetry_service(std::filesystem::path output_path,
        std::string test_suite_name,
        std::string test_name)
        : output_path_(std::move(output_path))
        , suite_name_(std::move(test_suite_name))
        , test_name_(std::move(test_name))
        , start_time_(std::chrono::steady_clock::now())
    {
    }

    animation_telemetry_service::~animation_telemetry_service()
    {
        write_output();
    }

    std::string animation_telemetry_service::sanitize_name(const std::string& name)
    {
        std::string sanitized = name;
        std::replace_if(sanitized.begin(), sanitized.end(), [](char ch) {
            return ch == '/' || ch == '\\' || ch == ':' || ch == '*';
        }, '#');
        return sanitized;
    }

    std::string animation_telemetry_service::escape_json(const std::string& input)
    {
        std::string escaped;
        escaped.reserve(input.size() + input.size() / 4 + 4);
        for (char ch : input)
        {
            switch (ch)
            {
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20)
                {
                    std::ostringstream oss;
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(ch));
                    escaped += oss.str();
                }
                else
                {
                    escaped += ch;
                }
            }
        }
        return escaped;
    }

    animation_telemetry_service::event_field animation_telemetry_service::make_string_field(
        const std::string& key,
        const std::string& value)
    {
        return event_field{key, value, field_kind::string};
    }

    animation_telemetry_service::event_field animation_telemetry_service::make_number_field(
        const std::string& key,
        uint64_t value)
    {
        return event_field{key, std::to_string(value), field_kind::number};
    }

    animation_telemetry_service::event_field animation_telemetry_service::make_signed_field(
        const std::string& key,
        int64_t value)
    {
        return event_field{key, std::to_string(value), field_kind::number};
    }

    animation_telemetry_service::event_field animation_telemetry_service::make_boolean_field(
        const std::string& key,
        bool value)
    {
        return event_field{key, value ? "true" : "false", field_kind::boolean};
    }

    animation_telemetry_service::event_field animation_telemetry_service::make_floating_field(
        const std::string& key,
        double value)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << value;
        return event_field{key, oss.str(), field_kind::floating};
    }

    void animation_telemetry_service::record_event(const std::string& type, std::initializer_list<event_field> fields) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        event_record record;
        record.timestamp = timestamp_now();
        record.type = type;
        record.fields.assign(fields.begin(), fields.end());
        events_.push_back(std::move(record));
    }

    void animation_telemetry_service::record_event(const std::string& type, std::vector<event_field>&& fields) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        event_record record;
        record.timestamp = timestamp_now();
        record.type = type;
        record.fields = std::move(fields);
        events_.push_back(std::move(record));
    }

    void animation_telemetry_service::on_service_creation(
        const char* name, rpc::zone zone_id, rpc::destination_zone parent_zone_id) const
    {
        std::string service_name = name ? name : "";
        double ts = timestamp_now();
        std::vector<event_field> fields;
        fields.reserve(3);
        fields.push_back(make_string_field("serviceName", service_name));
        fields.push_back(make_number_field("zone", zone_id.get_val()));
        fields.push_back(make_number_field("parentZone", parent_zone_id.get_val()));

        {
            std::lock_guard<std::mutex> lock(mutex_);
            zone_names_[zone_id.get_val()] = service_name;
            zone_parents_[zone_id.get_val()] = parent_zone_id.get_val();

            event_record record;
            record.timestamp = ts;
            record.type = "service_creation";
            record.fields = std::move(fields);
            events_.push_back(std::move(record));
        }
    }

    void animation_telemetry_service::on_service_deletion(rpc::zone zone_id) const
    {
        std::vector<event_field> fields = {
            make_number_field("zone", zone_id.get_val())
        };
        std::lock_guard<std::mutex> lock(mutex_);
        zone_names_.erase(zone_id.get_val());
        zone_parents_.erase(zone_id.get_val());

        event_record record;
        record.timestamp = timestamp_now();
        record.type = "service_deletion";
        record.fields = std::move(fields);
        events_.push_back(std::move(record));
    }

    void animation_telemetry_service::on_service_try_cast(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id) const
    {
        record_event("service_try_cast",
            {make_number_field("zone", zone_id.get_val()),
                make_number_field("destinationZone", destination_zone_id.get_val()),
                make_number_field("callerZone", caller_zone_id.get_val()),
                make_number_field("object", object_id.get_val()),
                make_number_field("interface", interface_id.get_val())});
    }

    void animation_telemetry_service::on_service_add_ref(rpc::zone zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::caller_channel_zone caller_channel_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::add_ref_options options) const
    {
        record_event("service_add_ref",
            {make_number_field("zone", zone_id.get_val()),
                make_number_field("destinationChannelZone", destination_channel_zone_id.get_val()),
                make_number_field("destinationZone", destination_zone_id.get_val()),
                make_number_field("object", object_id.get_val()),
                make_number_field("callerChannelZone", caller_channel_zone_id.get_val()),
                make_number_field("callerZone", caller_zone_id.get_val()),
                make_number_field("options", static_cast<uint64_t>(options))});
    }

    void animation_telemetry_service::on_service_release(rpc::zone zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::caller_zone caller_zone_id) const
    {
        record_event("service_release",
            {make_number_field("zone", zone_id.get_val()),
                make_number_field("destinationChannelZone", destination_channel_zone_id.get_val()),
                make_number_field("destinationZone", destination_zone_id.get_val()),
                make_number_field("object", object_id.get_val()),
                make_number_field("callerZone", caller_zone_id.get_val())});
    }

    void animation_telemetry_service::on_service_proxy_creation(const char* service_name,
        const char* service_proxy_name,
        rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id) const
    {
        std::vector<event_field> fields = {
            make_string_field("serviceName", service_name ? service_name : ""),
            make_string_field("serviceProxyName", service_proxy_name ? service_proxy_name : ""),
            make_number_field("zone", zone_id.get_val()),
            make_number_field("destinationZone", destination_zone_id.get_val()),
            make_number_field("callerZone", caller_zone_id.get_val())};
        record_event("service_proxy_creation", std::move(fields));
    }

    void animation_telemetry_service::on_cloned_service_proxy_creation(const char* service_name,
        const char* service_proxy_name,
        rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id) const
    {
        std::vector<event_field> fields = {
            make_string_field("serviceName", service_name ? service_name : ""),
            make_string_field("serviceProxyName", service_proxy_name ? service_proxy_name : ""),
            make_number_field("zone", zone_id.get_val()),
            make_number_field("destinationZone", destination_zone_id.get_val()),
            make_number_field("callerZone", caller_zone_id.get_val())};
        record_event("cloned_service_proxy_creation", std::move(fields));
    }

    void animation_telemetry_service::on_service_proxy_deletion(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id) const
    {
        record_event("service_proxy_deletion",
            {make_number_field("zone", zone_id.get_val()),
                make_number_field("destinationZone", destination_zone_id.get_val()),
                make_number_field("callerZone", caller_zone_id.get_val())});
    }

    void animation_telemetry_service::on_service_proxy_try_cast(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id) const
    {
        record_event("service_proxy_try_cast",
            {make_number_field("zone", zone_id.get_val()),
                make_number_field("destinationZone", destination_zone_id.get_val()),
                make_number_field("callerZone", caller_zone_id.get_val()),
                make_number_field("object", object_id.get_val()),
                make_number_field("interface", interface_id.get_val())});
    }

    void animation_telemetry_service::on_service_proxy_add_ref(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::object object_id,
        rpc::add_ref_options options) const
    {
        record_event("service_proxy_add_ref",
            {make_number_field("zone", zone_id.get_val()),
                make_number_field("destinationZone", destination_zone_id.get_val()),
                make_number_field("destinationChannelZone", destination_channel_zone_id.get_val()),
                make_number_field("callerZone", caller_zone_id.get_val()),
                make_number_field("object", object_id.get_val()),
                make_number_field("options", static_cast<uint64_t>(options))});
    }

    void animation_telemetry_service::on_service_proxy_release(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::object object_id) const
    {
        record_event("service_proxy_release",
            {make_number_field("zone", zone_id.get_val()),
                make_number_field("destinationZone", destination_zone_id.get_val()),
                make_number_field("destinationChannelZone", destination_channel_zone_id.get_val()),
                make_number_field("callerZone", caller_zone_id.get_val()),
                make_number_field("object", object_id.get_val())});
    }

    void animation_telemetry_service::on_service_proxy_add_external_ref(rpc::zone zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id,
        int ref_count) const
    {
        record_event("service_proxy_add_external_ref",
            {make_number_field("zone", zone_id.get_val()),
                make_number_field("destinationChannelZone", destination_channel_zone_id.get_val()),
                make_number_field("destinationZone", destination_zone_id.get_val()),
                make_number_field("callerZone", caller_zone_id.get_val()),
                make_signed_field("refCount", ref_count)});
    }

    void animation_telemetry_service::on_service_proxy_release_external_ref(rpc::zone zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id,
        int ref_count) const
    {
        record_event("service_proxy_release_external_ref",
            {make_number_field("zone", zone_id.get_val()),
                make_number_field("destinationChannelZone", destination_channel_zone_id.get_val()),
                make_number_field("destinationZone", destination_zone_id.get_val()),
                make_number_field("callerZone", caller_zone_id.get_val()),
                make_signed_field("refCount", ref_count)});
    }

    void animation_telemetry_service::on_impl_creation(const char* name, uint64_t address, rpc::zone zone_id) const
    {
        std::vector<event_field> fields = {
            make_string_field("name", name ? name : ""),
            make_number_field("address", address),
            make_number_field("zone", zone_id.get_val())};
        record_event("impl_creation", std::move(fields));
    }

    void animation_telemetry_service::on_impl_deletion(uint64_t address, rpc::zone zone_id) const
    {
        record_event("impl_deletion",
            {make_number_field("address", address), make_number_field("zone", zone_id.get_val())});
    }

    void animation_telemetry_service::on_stub_creation(rpc::zone zone_id, rpc::object object_id, uint64_t address) const
    {
        record_event("stub_creation",
            {make_number_field("zone", zone_id.get_val()),
                make_number_field("object", object_id.get_val()),
                make_number_field("address", address)});
    }

    void animation_telemetry_service::on_stub_deletion(rpc::zone zone_id, rpc::object object_id) const
    {
        record_event("stub_deletion",
            {make_number_field("zone", zone_id.get_val()), make_number_field("object", object_id.get_val())});
    }

    void animation_telemetry_service::on_stub_send(rpc::zone zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        rpc::method method_id) const
    {
        record_event("stub_send",
            {make_number_field("zone", zone_id.get_val()),
                make_number_field("object", object_id.get_val()),
                make_number_field("interface", interface_id.get_val()),
                make_number_field("method", method_id.get_val())});
    }

    void animation_telemetry_service::on_stub_add_ref(rpc::zone zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        uint64_t count,
        rpc::caller_zone caller_zone_id) const
    {
        record_event("stub_add_ref",
            {make_number_field("zone", zone_id.get_val()),
                make_number_field("object", object_id.get_val()),
                make_number_field("interface", interface_id.get_val()),
                make_number_field("count", count),
                make_number_field("callerZone", caller_zone_id.get_val())});
    }

    void animation_telemetry_service::on_stub_release(rpc::zone zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        uint64_t count,
        rpc::caller_zone caller_zone_id) const
    {
        record_event("stub_release",
            {make_number_field("zone", zone_id.get_val()),
                make_number_field("object", object_id.get_val()),
                make_number_field("interface", interface_id.get_val()),
                make_number_field("count", count),
                make_number_field("callerZone", caller_zone_id.get_val())});
    }

    void animation_telemetry_service::on_object_proxy_creation(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        bool add_ref_done) const
    {
        record_event("object_proxy_creation",
            {make_number_field("zone", zone_id.get_val()),
                make_number_field("destinationZone", destination_zone_id.get_val()),
                make_number_field("object", object_id.get_val()),
                make_boolean_field("addRefDone", add_ref_done)});
    }

    void animation_telemetry_service::on_object_proxy_deletion(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id) const
    {
        record_event("object_proxy_deletion",
            {make_number_field("zone", zone_id.get_val()),
                make_number_field("destinationZone", destination_zone_id.get_val()),
                make_number_field("object", object_id.get_val())});
    }

    void animation_telemetry_service::on_interface_proxy_creation(const char* name,
        rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id) const
    {
        std::vector<event_field> fields = {
            make_string_field("name", name ? name : ""),
            make_number_field("zone", zone_id.get_val()),
            make_number_field("destinationZone", destination_zone_id.get_val()),
            make_number_field("object", object_id.get_val()),
            make_number_field("interface", interface_id.get_val())};
        record_event("interface_proxy_creation", std::move(fields));
    }

    void animation_telemetry_service::on_interface_proxy_deletion(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id) const
    {
        record_event("interface_proxy_deletion",
            {make_number_field("zone", zone_id.get_val()),
                make_number_field("destinationZone", destination_zone_id.get_val()),
                make_number_field("object", object_id.get_val()),
                make_number_field("interface", interface_id.get_val())});
    }

    void animation_telemetry_service::on_interface_proxy_send(const char* method_name,
        rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        rpc::method method_id) const
    {
        std::vector<event_field> fields = {
            make_string_field("methodName", method_name ? method_name : ""),
            make_number_field("zone", zone_id.get_val()),
            make_number_field("destinationZone", destination_zone_id.get_val()),
            make_number_field("object", object_id.get_val()),
            make_number_field("interface", interface_id.get_val()),
            make_number_field("method", method_id.get_val())};
        record_event("interface_proxy_send", std::move(fields));
    }

    void animation_telemetry_service::message(level_enum level, const char* message) const
    {
        std::vector<event_field> fields = {
            make_number_field("level", static_cast<uint64_t>(level)),
            make_string_field("message", message ? message : "")};
        record_event("message", std::move(fields));
    }

    void animation_telemetry_service::write_output() const
    {
        std::vector<event_record> events_copy;
        std::unordered_map<uint64_t, std::string> zone_names_copy;
        std::unordered_map<uint64_t, uint64_t> zone_parents_copy;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            events_copy = events_;
            zone_names_copy = zone_names_;
            zone_parents_copy = zone_parents_;
        }

        std::error_code ec;
        std::filesystem::create_directories(output_path_.parent_path(), ec);

        std::ofstream output(output_path_, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!output)
        {
            return;
        }

        double total_duration = 0.0;
        if (!events_copy.empty())
        {
            total_duration = events_copy.back().timestamp;
            for (const auto& evt : events_copy)
            {
                if (evt.timestamp > total_duration)
                {
                    total_duration = evt.timestamp;
                }
            }
        }

        output << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\" />\n";
        output << "<title>" << kTitlePrefix << " - " << escape_json(suite_name_) << "." << escape_json(test_name_) << "</title>\n";
        output << "<style>\n" << kAnimationStyles << "\n</style>\n";
        output << "</head>\n<body>\n";
        output << "<div class=\"header\">\n";
        output << "  <h1>" << kTitlePrefix << "</h1>\n";
        output << "  <div class=\"subtitle\">" << escape_json(suite_name_) << " / " << escape_json(test_name_) << "</div>\n";
        output << "</div>\n";
        output << "<div class=\"controls\">\n";
        output << "  <button id=\"start-button\">Start</button>\n";
        output << "  <button id=\"stop-button\" disabled>Stop</button>\n";
        output << "  <button id=\"reset-button\">Reset</button>\n";
        output << "  <input type=\"range\" id=\"timeline-slider\" min=\"0\" value=\"0\" />\n";
        output << "  <span class=\"time-label\" id=\"time-display\">0.000s</span>\n";
        output << "  <label class=\"speed-control\">Speed\n";
        output << "    <select id=\"speed-select\">\n";
        output << "      <option value=\"0.25\">0.25×</option>\n";
        output << "      <option value=\"0.5\">0.5×</option>\n";
        output << "      <option value=\"1\">1×</option>\n";
        output << "      <option value=\"1.5\">1.5×</option>\n";
        output << "      <option value=\"2\">2×</option>\n";
        output << "      <option value=\"5\" selected>5×</option>\n";
        output << "      <option value=\"10\">10×</option>\n";
        output << "      <option value=\"20\">20×</option>\n";
        output << "    </select>\n";
        output << "  </label>\n";
        output << "  <div class=\"type-filters\" id=\"type-filters\">\n";
        output << "    <span class=\"filter-title\">Show</span>\n";
        output << "  </div>\n";
        output << "  <label class=\"filter-item\">\n";
        output << "    <input type=\"checkbox\" id=\"show-log-checkbox\" checked>\n";
        output << "    <span>Show Log</span>\n";
        output << "  </label>\n";
        output << "</div>\n";
        output << "<div id=\"main-layout\">\n";
        output << "  <div id=\"viz-container\"></div>\n";
        output << "  <aside id=\"event-panel\">\n";
        output << "    <div class=\"event-log-header\">\n";
        output << "      <h2>Event Log</h2>\n";
        output << "    </div>\n";
        output << "    <div id=\"event-log-body\">\n";
        output << "      <ul id=\"event-log\"></ul>\n";
        output << "    </div>\n";
        output << "  </aside>\n";
        output << "</div>\n";

        output << "<script>\n";
        output << "const telemetryMeta = { suite: \"" << escape_json(suite_name_) << "\", test: \"" << escape_json(test_name_) << "\" };\n";
        output << "const zoneMetadata = {\n";
        bool first_zone = true;
        for (const auto& entry : zone_names_copy)
        {
            if (!first_zone)
            {
                output << ",\n";
            }
            first_zone = false;
            const auto zone_id = entry.first;
            const auto parent_it = zone_parents_copy.find(zone_id);
            uint64_t parent_zone = parent_it != zone_parents_copy.end() ? parent_it->second : 0;
            output << "  \"" << zone_id << "\": { name: \"" << escape_json(entry.second) << "\", parent: "
                   << parent_zone << " }";
        }
        if (!first_zone)
        {
            output << "\n";
        }
        output << "};\n";

        output << "const events = [\n";
        for (size_t idx = 0; idx < events_copy.size(); ++idx)
        {
            const auto& evt = events_copy[idx];
            output << "  { type: \"" << escape_json(evt.type) << "\", timestamp: " << std::fixed
                   << std::setprecision(6) << evt.timestamp << ", data: {";
            for (size_t field_idx = 0; field_idx < evt.fields.size(); ++field_idx)
            {
                const auto& field = evt.fields[field_idx];
                if (field_idx > 0)
                {
                    output << ", ";
                }
                output << "\"" << field.key << "\": ";
                switch (field.type)
                {
                case field_kind::string:
                    output << "\"" << escape_json(field.value) << "\"";
                    break;
                case field_kind::number:
                    output << field.value;
                    break;
                case field_kind::boolean:
                    output << (field.value == "true" ? "true" : "false");
                    break;
                case field_kind::floating:
                    output << field.value;
                    break;
                }
            }
            output << " } }";
            if (idx + 1 < events_copy.size())
            {
                output << ",";
            }
            output << "\n";
        }
        output << "];\n";
        output << "const totalDuration = " << std::fixed << std::setprecision(6) << total_duration << ";\n";
        output << "</script>\n";

        output << "<script src=\"https://d3js.org/d3.v7.min.js\"></script>\n";
        output << "<script>\n" << kAnimationScript << "\n</script>\n";
        output << "</body>\n</html>\n";
    }
}
