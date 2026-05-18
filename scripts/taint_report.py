#!/usr/bin/env python3
"""Generate HTML taint analysis report from scout S-Expr output."""

import re
import sys
import json
import subprocess
from pathlib import Path


def run_scout(apk_path: str, method: str, depth: int = 2) -> str:
    """Run scout and return raw S-Expr output."""
    cmd = [
        str(Path(__file__).parent.parent / "build/scout"),
        "-p", apk_path,
        "--track-var", method,
        "--track-depth", str(depth),
    ]
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    return result.stdout


def _find_matching_paren(text: str, start: int) -> int:
    """Find closing paren matching the opening paren at text[start]."""
    depth = 0
    in_str = False
    i = start
    while i < len(text):
        # SExpr uses \" as quote delimiter
        if text[i:i+2] == '\\"':
            in_str = not in_str
            i += 2
            continue
        if not in_str:
            if text[i] == '(':
                depth += 1
            elif text[i] == ')':
                depth -= 1
                if depth == 0:
                    return i
        i += 1
    return -1


def _parse_sexpr_kv(text: str) -> dict:
    """Parse key-value pairs from (:ev :k1 "v1" :k2 123 ...) body.
    Skips the initial :type keyword. Values can be \"quoted\" or bare tokens.
    """
    data = {}
    # Find first bare keyword and then parse all :key value pairs after it
    # Remove leading :type keyword (e.g. :ev)
    rest = re.sub(r'^:\S+\s*', '', text, count=1)
    for m in re.finditer(r':([a-zA-Z_-]+)\s+(?:\\"((?:(?!\\").)*?)\\"|(\S+))', rest):
        if m.group(2) is not None:
            data[m.group(1)] = m.group(2)
        elif m.group(3) is not None:
            data[m.group(1)] = m.group(3)
    return data


def parse_sexpr_events(text: str):
    """Parse scout S-Expr output into event dicts."""
    events = []
    idx = text.find(':aero-taint-report')
    if idx == -1:
        return events, "unknown"

    # Find events by scanning for (:ev blocks
    pos = 0
    while True:
        ev_start = text.find('(:ev', pos)
        if ev_start == -1:
            break
        ev_end = _find_matching_paren(text, ev_start)
        if ev_end == -1:
            break
        block = text[ev_start + 1:ev_end]  # strip outer parens
        kv = _parse_sexpr_kv(block)
        if 'm' in kv and 'l' in kv and 'r' in kv and 'a' in kv:
            events.append({
                "method": kv.get("m", ""),
                "line": int(kv.get("l", 0)),
                "reg": kv.get("r", ""),
                "action": kv.get("a", ""),
                "target": kv.get("t", ""),
                "extra": kv.get("e", ""),
            })
        pos = ev_end + 1

    # Extract start method
    start_m = re.search(r':start\s+"([^"]+)"', text)
    start_method = start_m.group(1) if start_m else "unknown"

    return events, start_method


ACTION_COLORS = {
    "CALL": "#4a90d9",
    "LOAD": "#50b86c",
    "STORE": "#e67e22",
    "STORE_STATIC": "#e67e22",
    "STORE_ARRAY": "#e67e22",
    "SINK_LEAK": "#e74c3c",
    "TAINT_PROP": "#9b59b6",
    "MOVE_RESULT": "#95a5a6",
    "CONST": "#f39c12",
    "CONST_ASSIGN": "#f39c12",
    "MOVE": "#1abc9c",
    "SANITY": "#34495e",
    "EES_OPAQUE_ENTRY": "#e91e63",
}

ACTION_ICONS = {
    "CALL": "→",
    "LOAD": "↑",
    "STORE": "↓",
    "STORE_STATIC": "↓",
    "STORE_ARRAY": "↓",
    "SINK_LEAK": "⚠",
    "TAINT_PROP": "◆",
    "MOVE_RESULT": "●",
    "CONST": "⊕",
    "CONST_ASSIGN": "⊕",
    "MOVE": "↔",
    "SANITY": "✓",
    "EES_OPAQUE_ENTRY": "◈",
}

SINK_ACTIONS = {"SINK_LEAK"}


def generate_html(events, start_method):
    if not events:
        return "<html><body><h1>No taint events found</h1></body></html>"

    # Stats
    total = len(events)
    methods = set(e["method"] for e in events)
    sinks = [e for e in events if e["action"] in SINK_ACTIONS]
    by_action = {}
    for e in events:
        by_action.setdefault(e["action"], 0)
        by_action[e["action"]] += 1

    event_rows = ""
    for i, ev in enumerate(events):
        color = ACTION_COLORS.get(ev["action"], "#7f8c8d")
        icon = ACTION_ICONS.get(ev["action"], "•")
        is_sink = ev["action"] in SINK_ACTIONS
        row_class = "sink-row" if is_sink else ""
        extra_html = f'<span class="extra">{ev["extra"]}</span>' if ev["extra"] else ""
        event_rows += f"""
        <tr class="{row_class}">
            <td>{i + 1}</td>
            <td><span class="badge" style="background:{color}">{icon} {ev["action"]}</span></td>
            <td class="method-cell">{ev["method"]}</td>
            <td>{ev["line"]}</td>
            <td class="reg-cell">{ev["reg"]}</td>
            <td class="target-cell">{ev["target"]}</td>
            <td>{extra_html}</td>
        </tr>"""

    action_summary = "".join(
        f'<span class="stat-chip" style="background:{ACTION_COLORS.get(a, "#7f8c8d")}">{a}: {c}</span>'
        for a, c in sorted(by_action.items(), key=lambda x: -x[1])
    )

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Taint Analysis Report</title>
<style>
* {{ margin: 0; padding: 0; box-sizing: border-box; }}
body {{ font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, monospace; background: #1a1a2e; color: #e0e0e0; padding: 20px; }}
h1 {{ color: #fff; font-size: 1.5em; margin-bottom: 5px; }}
h2 {{ color: #aaa; font-size: 1em; font-weight: normal; margin-bottom: 20px; }}
.stats {{ display: flex; gap: 15px; flex-wrap: wrap; margin-bottom: 20px; }}
.stat-card {{ background: #16213e; padding: 12px 18px; border-radius: 8px; min-width: 100px; }}
.stat-card .num {{ font-size: 1.5em; font-weight: bold; color: #fff; }}
.stat-card .label {{ font-size: 0.8em; color: #888; }}
.action-summary {{ display: flex; gap: 8px; flex-wrap: wrap; margin-bottom: 20px; }}
.stat-chip {{ padding: 4px 10px; border-radius: 12px; font-size: 0.8em; color: #fff; }}
.filters {{ margin-bottom: 15px; display: flex; gap: 10px; flex-wrap: wrap; align-items: center; }}
.filters input, .filters select {{ background: #16213e; border: 1px solid #333; color: #e0e0e0; padding: 6px 10px; border-radius: 4px; font-size: 0.9em; }}
.filters label {{ font-size: 0.85em; color: #aaa; }}
table {{ width: 100%; border-collapse: collapse; font-size: 0.85em; }}
th {{ background: #16213e; color: #888; padding: 8px 10px; text-align: left; position: sticky; top: 0; }}
td {{ padding: 6px 10px; border-bottom: 1px solid #1a1a3e; }}
tr:hover {{ background: #1a1a3e; }}
tr.sink-row {{ background: #3d1a1a !important; }}
tr.sink-row:hover {{ background: #4d2222 !important; }}
.badge {{ padding: 2px 8px; border-radius: 10px; font-size: 0.8em; color: #fff; white-space: nowrap; }}
.method-cell {{ max-width: 400px; overflow: hidden; text-overflow: ellipsis; }}
.target-cell {{ max-width: 350px; overflow: hidden; text-overflow: ellipsis; color: #aaa; }}
.reg-cell {{ color: #f1c40f; font-weight: bold; }}
.extra {{ color: #7f8c8d; font-style: italic; font-size: 0.9em; }}
.controls {{ margin: 10px 0; }}
.controls button {{ background: #16213e; border: 1px solid #333; color: #e0e0e0; padding: 6px 14px; border-radius: 4px; cursor: pointer; margin-right: 6px; }}
.controls button:hover {{ background: #1a1a3e; }}
#event-count {{ color: #888; font-size: 0.9em; margin-left: 10px; }}
</style>
</head>
<body>
<h1>Taint Analysis Report</h1>
<h2>Start: {start_method}</h2>

<div class="stats">
    <div class="stat-card"><div class="num">{total}</div><div class="label">Events</div></div>
    <div class="stat-card"><div class="num">{len(methods)}</div><div class="label">Methods</div></div>
    <div class="stat-card"><div class="num">{len(sinks)}</div><div class="label">Sinks</div></div>
</div>

<div class="action-summary">{action_summary}</div>

<div class="filters">
    <label>Filter:</label>
    <input type="text" id="filterInput" placeholder="method, reg, target..." oninput="filterTable()">
    <select id="actionFilter" onchange="filterTable()">
        <option value="">All actions</option>
        {''.join(f'<option value="{a}">{a}</option>' for a in sorted(by_action))}
    </select>
    <label><input type="checkbox" id="sinkOnly" onchange="filterTable()"> Sinks only</label>
    <span id="event-count"></span>
</div>

<div class="controls">
    <button onclick="expandAll()">Expand All</button>
    <button onclick="collapseAll()">Collapse All</button>
    <button onclick="scrollToSinks()">Scroll to Sinks</button>
    <button onclick="document.getElementById('filterInput').value=''; document.getElementById('actionFilter').value=''; document.getElementById('sinkOnly').checked=false; filterTable();">Reset</button>
</div>

<table id="event-table">
<thead><tr>
    <th>#</th><th>Action</th><th>Method</th><th>Line</th><th>Reg</th><th>Target</th><th>Extra</th>
</tr></thead>
<tbody>
{event_rows}
</tbody>
</table>

<script>
function filterTable() {{
    const q = document.getElementById('filterInput').value.toLowerCase();
    const act = document.getElementById('actionFilter').value;
    const sinkOnly = document.getElementById('sinkOnly').checked;
    const rows = document.querySelectorAll('#event-table tbody tr');
    let visible = 0;
    rows.forEach(r => {{
        const text = r.textContent.toLowerCase();
        const action = r.querySelector('.badge')?.textContent.trim().split(' ')[1] || '';
        const match = (!q || text.includes(q)) && (!act || action === act) && (!sinkOnly || r.classList.contains('sink-row'));
        r.style.display = match ? '' : 'none';
        if (match) visible++;
    }});
    document.getElementById('event-count').textContent = visible + ' / ' + rows.length + ' events';
}}
function expandAll() {{ document.querySelectorAll('.method-cell').forEach(c => c.style.whiteSpace = 'normal'); }}
function collapseAll() {{ document.querySelectorAll('.method-cell').forEach(c => c.style.whiteSpace = 'nowrap'); }}
function scrollToSinks() {{
    const first = document.querySelector('tr.sink-row');
    if (first) first.scrollIntoView({{ behavior: 'smooth', block: 'center' }});
}}
filterTable();
</script>
</body>
</html>"""
    return html


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Taint analysis HTML report generator")
    parser.add_argument("-p", "--path", help="APK path (runs scout automatically)")
    parser.add_argument("-m", "--method", help="Method signature for taint tracking")
    parser.add_argument("-d", "--depth", type=int, default=2, help="Tracking depth")
    parser.add_argument("-i", "--input", help="Input file with S-Expr data (instead of running scout)")
    parser.add_argument("-o", "--output", default="taint_report.html", help="Output HTML file")

    args = parser.parse_args()

    if args.input:
        text = Path(args.input).read_text()
    elif args.path and args.method:
        print("Running scout...", file=sys.stderr)
        text = run_scout(args.path, args.method, args.depth)
    else:
        parser.print_help()
        sys.exit(1)

    result = parse_sexpr_events(text)
    if not result:
        print("No events found in output", file=sys.stderr)
        sys.exit(1)

    events, start_method = result
    html = generate_html(events, start_method)
    Path(args.output).write_text(html)
    print(f"Report written to {args.output} ({len(events)} events)")


if __name__ == "__main__":
    main()
