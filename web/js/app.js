const HISTORY_LEN = 60;
const POLL_MS = 2000;

let cpuHistory = [];
let memHistory = [];
let loadHistory = [];
let timestamps = [];
let cpuChart, memChart, loadChart;

function FmtBytes(b) {
    if (b > 1073741824) {
        return (b / 1073741824).toFixed(1) + ' GB';
    } if (b > 1048576) {
        return (b / 1048576).toFixed(0) + ' MB';
    } if (b > 1024) {
        return (b / 1024).toFixed(0) + ' KB';
    }
    return b + ' B';
}

function FmtUptime(s) {
    const d = Math.floor(s / 86400);
    const h = Math.floor((s % 86400) / 3600);
    const m = Math.floor((s % 3600) / 60);
    if (d > 0) {
        return d + 'd ' + h + 'h ' + m + 'm';
    } if (h > 0) {
        return h + 'h ' + m + 'm';
    }
    return m + 'm';
}

function BarColor(pct) {
    if (pct > 90) {
        return 'red';
    } if (pct > 70) {
        return 'yellow';
    }
    return 'green';
}

function TsMs(ts) {
    return [ts / 1000];
}

function SevStr(sev) {
    const map = { 1: 'INFO', 2: 'WARN', 3: 'ERR', 4: 'CRIT' };
    return map[sev] || '---';
}

function CatStr(cat) {
    const map = { 1: 'system', 2: 'service', 3: 'network', 4: 'ai', 5: 'security', 6: 'disk', 7: 'temp', 8: 'dns' };
    return map[cat] || 'other';
}

function CreateCharts() {
    const opts = (color, max) => ({
        width: 280,
        height: 110,
        axes: [
            { show: false },
            { show: false, min: 0, max: max }
        ],
        series: [
            {},
            { stroke: color, width: 2, fill: color + '33' }
        ],
        scales: {
            x: { time: false },
            y: { range: [0, max] }
        },
        legend: { show: false }
    });

    cpuChart = new uPlot(opts('#58a6ff', 100), [[], []], document.getElementById('cpu-chart'));
    memChart = new uPlot(opts('#3fb950', 100), [[], []], document.getElementById('mem-chart'));
    loadChart = new uPlot(opts('#d29922', 4), [[], []], document.getElementById('load-chart'));
}

function UpdateCharts(snap) {
    const ts = snap.timestamp_ms;
    timestamps.push(ts);
    cpuHistory.push(snap.cpu.overall_percent);
    memHistory.push(snap.memory.percent);
    loadHistory.push(snap.load.load_1m);

    if (timestamps.length > HISTORY_LEN) {
        timestamps.shift();
        cpuHistory.shift();
        memHistory.shift();
        loadHistory.shift();
    }

    const x = timestamps.map(t => [t / 1000]);
    cpuChart.setData([x, cpuHistory]);
    memChart.setData([x, memHistory]);

    // Auto-scale load
    const loadMax = Math.max(2, Math.max(...loadHistory) * 1.2);
    loadChart.scales.y.max = loadMax;
    loadChart.setData([x, loadHistory]);
}

function UpdateDetails(snap) {
    // CPU cores
    let coreHtml = '';
    for (const c of snap.cpu.cores) {
        coreHtml += `<div class="detail-row"><span class="label">core ${c.core_id}</span><span class="value">${c.percent.toFixed(0)}%</span></div>`;
        coreHtml += `<div class="bar-container"><div class="bar-fill ${BarColor(c.percent)}" style="width:${c.percent}%"></div></div>`;
    }
    document.getElementById('cpu-cores').innerHTML = coreHtml;

    // Memory
    const m = snap.memory;
    let memHtml = `<div class="detail-row"><span class="label">RAM</span><span class="value">${FmtBytes(m.used_bytes)} / ${FmtBytes(m.total_bytes)} (${m.percent.toFixed(1)}%)</span></div>`;
    memHtml += `<div class="bar-container"><div class="bar-fill ${BarColor(m.percent)}" style="width:${m.percent}%"></div></div>`;
    if (m.swap_total_bytes > 0) {
        memHtml += `<div class="detail-row"><span class="label">Swap</span><span class="value">${FmtBytes(m.swap_used_bytes)} / ${FmtBytes(m.swap_total_bytes)} (${m.swap_percent.toFixed(1)}%)</span></div>`;
    }
    document.getElementById('mem-detail').innerHTML = memHtml;

    // Network
    let netHtml = '';
    for (const iface of snap.network.interfaces) {
        netHtml += `<div class="detail-row"><span class="label">${iface.name} rx</span><span class="value">${FmtBytes(iface.rx_bytes)}</span></div>`;
        netHtml += `<div class="detail-row"><span class="label">${iface.name} tx</span><span class="value">${FmtBytes(iface.tx_bytes)}</span></div>`;
    }
    netHtml += `<div class="detail-row"><span class="label">TCP</span><span class="value">${snap.network.tcp_established} est, ${snap.network.tcp_time_wait} tw</span></div>`;
    netHtml += `<div class="detail-row"><span class="label">UDP</span><span class="value">${snap.network.udp_connections}</span></div>`;
    document.getElementById('net-detail').innerHTML = netHtml;

    // Disk
    let diskHtml = '';
    for (const fs of snap.disk.filesystems) {
        diskHtml += `<div class="detail-row"><span class="label">${fs.mount_point}</span><span class="value">${FmtBytes(fs.used_bytes)} / ${FmtBytes(fs.total_bytes)} (${fs.percent.toFixed(0)}%)</span></div>`;
        diskHtml += `<div class="bar-container"><div class="bar-fill ${BarColor(fs.percent)}" style="width:${fs.percent}%"></div></div>`;
    }
    document.getElementById('disk-detail').innerHTML = diskHtml;

    // Temperature
    let tempHtml = '';
    for (const z of snap.temperature.zones) {
        const cls = z.temp_celsius > 80 ? 'red' : z.temp_celsius > 60 ? 'yellow' : 'green';
        tempHtml += `<div class="detail-row"><span class="label">${z.name}</span><span class="value">${z.temp_celsius.toFixed(1)} C</span></div>`;
        tempHtml += `<div class="bar-container"><div class="bar-fill ${cls}" style="width:${Math.min(100, z.temp_celsius)}%"></div></div>`;
    }
    if (snap.temperature.zones.length === 0) {
        tempHtml = '<div class="detail-row"><span class="label">No sensors</span></div>';
    }
    document.getElementById('temp-detail').innerHTML = tempHtml;
}

async function PollEvents() {
    try {
        const resp = await fetch('/api/events?limit=30');
        if (!resp.ok) {
            return;
        }

        const events = await resp.json();

        let html = '';
        for (const ev of events.reverse()) {
            const d = new Date(ev.timestamp_ms);
            const ts = d.toTimeString().slice(0, 8);
            html += `<div class="event-row">`;
            html += `<span class="ts">${ts}</span>`;
            html += `<span class="sev ${SevStr(ev.severity)}">${SevStr(ev.severity)}</span>`;
            html += `<span class="cat">${CatStr(ev.category)}</span>`;
            html += `<span class="msg">${ev.source}: ${ev.message}</span>`;
            html += `</div>`;
        }
        document.getElementById('events-list').innerHTML = html;
    } catch (e) { }
}

async function PollServices() {
    try {
        const resp = await fetch('/api/services');
        if (!resp.ok) {
            return;
        }

        const services = await resp.json();

        let running = 0;
        let html = '';
        for (const svc of services) {
            const state = ['unknown', 'stopped', 'starting', 'running', 'stopping', 'failed'][svc.state] || 'unknown';
            if (state === 'running') running++;
            html += `<div class="svc-row">`;
            html += `<span class="name">${svc.name}</span>`;
            html += `<span class="state ${state}">${state}</span>`;
            html += `<span class="enabled">${svc.enabled ? 'on boot' : ''}</span>`;
            html += `</div>`;
        }
        document.getElementById('services-list').innerHTML = html;
        document.getElementById('svc-count').textContent = running + '/' + services.length + ' services';
    } catch (e) { }
}

// ---- Main loop -------------------------------------------------------------
async function Poll() {
    try {
        const resp = await fetch('/api/metrics');
        if (!resp.ok) {
            return;
        }

        const snap = await resp.json();
        UpdateCharts(snap);
        UpdateDetails(snap);
    } catch (e) { }
}

async function PollStatus() {
    try {
        const resp = await fetch('/api/status');
        if (!resp.ok) return;
        const status = await resp.json();
        document.getElementById('svc-count').textContent = status.services_running + '/' + status.services_total + ' services';
        document.getElementById('uptime').textContent = 'up ' + FmtUptime(status.uptime_seconds);

        // WireGuard state from status
        let wgHtml = '';
        const wgUp = status.wireguard_active;
        wgHtml += `<div class="detail-row"><span class="label">State</span><span class="value" style="color:var(--${wgUp ? 'green' : 'red'})">${wgUp ? 'UP' : 'DOWN'}</span></div>`;
        document.getElementById('wg-detail').innerHTML = wgHtml;
    } catch (e) { }
}

async function PollDNS() {
    let dnsHtml = '<div class="detail-row"><span class="label">Status</span><span class="value" style="color:var(--green)">active</span></div>';
    dnsHtml += '<div class="detail-row"><span class="label">Cache</span><span class="value">-</span></div>';
    document.getElementById('dns-detail').innerHTML = dnsHtml;
}

document.addEventListener('DOMContentLoaded', () => {
    CreateCharts();
    Poll();
    PollEvents();
    PollServices();
    PollStatus();
    PollDNS();
    setInterval(Poll, POLL_MS);
    setInterval(PollEvents, 5000);
    setInterval(PollServices, 10000);
    setInterval(PollStatus, 5000);
    setInterval(PollDNS, 10000);
});
