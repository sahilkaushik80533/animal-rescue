"""
Stray Animal Rescue Dispatch System — Flask Backend (Expanded)
Sahil Kaushik

Endpoints:
  GET  /                  → Frontend
  GET  /api/rescues       → List calls (?status=pending|dispatched|all)
  POST /api/rescues       → Log a new call (with C engine validation)
  POST /api/dispatch      → Dispatch highest priority (C engine)
  GET  /api/search        → Search calls (?type=animal|location|description&q=...)
  GET  /api/sorted        → Pending calls sorted by urgency (C engine)
  GET  /api/stats         → Dashboard counts + metrics from C engine
  POST /api/validate      → Validate a single field via C engine
  GET  /api/export        → Export report via C engine and download
"""

from flask import Flask, request, jsonify, render_template, send_file
import sqlite3, subprocess, os, sys, io, time

app = Flask(__name__)

DATABASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'rescues.db')
SCHEMA   = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'schema.sql')
ENGINE   = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        'engine.exe' if sys.platform == 'win32' else 'engine')


def get_db():
    conn = sqlite3.connect(DATABASE)
    conn.row_factory = sqlite3.Row
    return conn


def init_db():
    conn = get_db()
    with open(SCHEMA, 'r') as f:
        conn.executescript(f.read())
    conn.commit()
    conn.close()


def sanitize(text):
    return str(text).replace('|', '-').replace('\n', ' ').replace('\r', '')


def call_engine(command_args, calls_data):
    """Pipe call data to the C engine via subprocess and return stdout."""
    input_data = '\n'.join(
        f"{c['id']}|{sanitize(c['location'])}|{sanitize(c['animal_type'])}|"
        f"{sanitize(c['description'])}|{c['urgency']}"
        for c in calls_data
    )
    try:
        result = subprocess.run(
            [ENGINE] + command_args,
            input=input_data, capture_output=True, text=True, timeout=10
        )
        if result.returncode != 0:
            print(f"[ENGINE ERROR] {result.stderr}")
            return None
        return result.stdout.strip()
    except FileNotFoundError:
        print("[ERROR] C engine not found. Compile with: gcc engine.c -o engine")
        return None
    except Exception as e:
        print(f"[ERROR] {e}")
        return None


def call_engine_no_input(command_args):
    """Call C engine with no stdin data (for validate, etc.)."""
    try:
        result = subprocess.run(
            [ENGINE] + command_args,
            input='', capture_output=True, text=True, timeout=10
        )
        if result.returncode != 0:
            print(f"[ENGINE ERROR] {result.stderr}")
            return None
        return result.stdout.strip()
    except FileNotFoundError:
        return None
    except Exception as e:
        print(f"[ERROR] {e}")
        return None


@app.route('/')
def index():
    return render_template('index.html')


@app.route('/api/rescues', methods=['GET'])
def get_rescues():
    status = request.args.get('status', 'all')
    conn = get_db()
    if status in ('pending', 'dispatched'):
        rows = conn.execute(
            'SELECT * FROM rescues WHERE status=? ORDER BY created_at DESC', (status,)
        ).fetchall()
    else:
        rows = conn.execute('SELECT * FROM rescues ORDER BY created_at DESC').fetchall()
    conn.close()
    return jsonify([dict(r) for r in rows])


@app.route('/api/rescues', methods=['POST'])
def add_rescue():
    data = request.get_json()
    errors = {}

    # Server-side field validation
    for field in ('location', 'animal_type', 'description', 'urgency'):
        if not data.get(field):
            errors[field] = f'{field.replace("_", " ").title()} is required.'

    if errors:
        return jsonify({'errors': errors}), 400

    # Validate individual fields via C engine
    loc_output = call_engine_no_input(['validate', 'location', data['location']])
    if loc_output and 'VALID|0' in loc_output:
        reason = loc_output.split('|', 2)[2] if loc_output.count('|') >= 2 else 'Invalid location'
        errors['location'] = reason

    animal_output = call_engine_no_input(['validate', 'animal', data['animal_type']])
    if animal_output and 'VALID|0' in animal_output:
        reason = animal_output.split('|', 2)[2] if animal_output.count('|') >= 2 else 'Invalid animal type'
        errors['animal_type'] = reason

    desc_output = call_engine_no_input(['validate', 'description', data['description']])
    if desc_output and 'VALID|0' in desc_output:
        reason = desc_output.split('|', 2)[2] if desc_output.count('|') >= 2 else 'Invalid description'
        errors['description'] = reason

    urg_output = call_engine_no_input(['validate', 'urgency', str(data['urgency'])])
    if urg_output and 'VALID|0' in urg_output:
        reason = urg_output.split('|', 2)[2] if urg_output.count('|') >= 2 else 'Invalid urgency'
        errors['urgency'] = reason

    if errors:
        return jsonify({'errors': errors}), 400

    urgency = max(1, min(10, int(data['urgency'])))
    conn = get_db()
    cur = conn.execute(
        'INSERT INTO rescues (location, animal_type, description, urgency) VALUES (?,?,?,?)',
        (data['location'], data['animal_type'], data['description'], urgency)
    )
    conn.commit()
    row = conn.execute('SELECT * FROM rescues WHERE id=?', (cur.lastrowid,)).fetchone()
    conn.close()
    return jsonify(dict(row)), 201


@app.route('/api/dispatch', methods=['POST'])
def dispatch():
    conn = get_db()
    pending = conn.execute('SELECT * FROM rescues WHERE status=?', ('pending',)).fetchall()
    if not pending:
        conn.close()
        return jsonify({'error': 'No pending calls to dispatch.'}), 400

    output = call_engine(['dispatch'], [dict(r) for r in pending])
    if not output:
        conn.close()
        return jsonify({'error': 'C engine error. Is engine.exe compiled?'}), 500

    for line in output.split('\n'):
        if line.strip().startswith('DISPATCH_ID'):
            did = int(line.strip().split('|')[1])
            conn.execute(
                'UPDATE rescues SET status=?, dispatched_at=CURRENT_TIMESTAMP WHERE id=?',
                ('dispatched', did)
            )
            conn.commit()
            row = conn.execute('SELECT * FROM rescues WHERE id=?', (did,)).fetchone()
            conn.close()
            return jsonify(dict(row))
        if line.strip().startswith('ERROR'):
            conn.close()
            return jsonify({'error': line.strip().split('|', 1)[1]}), 400

    conn.close()
    return jsonify({'error': 'Unexpected engine output'}), 500


@app.route('/api/search', methods=['GET'])
def search():
    stype = request.args.get('type', 'animal')
    query = request.args.get('q', '').strip()
    if not query:
        return jsonify({'error': 'Query required.'}), 400

    # Allow description search (new C engine feature)
    if stype not in ('animal', 'location', 'description'):
        stype = 'animal'

    conn = get_db()
    all_rows = conn.execute('SELECT * FROM rescues').fetchall()
    if not all_rows:
        conn.close()
        return jsonify([])

    output = call_engine(['search', stype, query], [dict(r) for r in all_rows])
    if not output:
        conn.close()
        return jsonify([])

    ids = [int(l.strip().split('|')[1]) for l in output.split('\n')
           if l.strip().startswith('MATCH_ID')]

    if ids:
        ph = ','.join('?' * len(ids))
        rows = conn.execute(
            f'SELECT * FROM rescues WHERE id IN ({ph}) ORDER BY urgency DESC', ids
        ).fetchall()
        conn.close()
        return jsonify([dict(r) for r in rows])

    conn.close()
    return jsonify([])


@app.route('/api/sorted', methods=['GET'])
def sorted_pending():
    conn = get_db()
    pending = conn.execute('SELECT * FROM rescues WHERE status=?', ('pending',)).fetchall()
    if not pending:
        conn.close()
        return jsonify([])

    output = call_engine(['sort'], [dict(r) for r in pending])
    if not output:
        conn.close()
        return jsonify([dict(r) for r in pending])

    sorted_ids = [int(l.strip().split('|')[1]) for l in output.split('\n')
                  if l.strip().startswith('SORTED')]

    results = []
    for sid in sorted_ids:
        row = conn.execute('SELECT * FROM rescues WHERE id=?', (sid,)).fetchone()
        if row:
            results.append(dict(row))
    conn.close()
    return jsonify(results)


@app.route('/api/stats', methods=['GET'])
def stats():
    conn = get_db()
    total = conn.execute('SELECT COUNT(*) FROM rescues').fetchone()[0]
    pend  = conn.execute("SELECT COUNT(*) FROM rescues WHERE status='pending'").fetchone()[0]
    disp  = conn.execute("SELECT COUNT(*) FROM rescues WHERE status='dispatched'").fetchone()[0]

    # Compute extended stats via C engine
    engine_stats = {}
    if pend > 0:
        pending_rows = conn.execute('SELECT * FROM rescues WHERE status=?', ('pending',)).fetchall()
        output = call_engine(['stats'], [dict(r) for r in pending_rows])
        if output:
            for line in output.split('\n'):
                line = line.strip()
                if line.startswith('STAT|'):
                    parts = line.split('|')
                    if len(parts) >= 3:
                        engine_stats[parts[1]] = parts[2]

    # Compute animal type breakdown from DB (all calls)
    animal_rows = conn.execute(
        "SELECT animal_type, COUNT(*) as cnt FROM rescues GROUP BY animal_type ORDER BY cnt DESC"
    ).fetchall()
    animal_breakdown = {r['animal_type']: r['cnt'] for r in animal_rows}

    conn.close()

    return jsonify({
        'total': total,
        'pending': pend,
        'dispatched': disp,
        'highest_urgency': engine_stats.get('highest_urgency', '—'),
        'lowest_urgency': engine_stats.get('lowest_urgency', '—'),
        'average_urgency': engine_stats.get('average_urgency', '—'),
        'animal_breakdown': animal_breakdown
    })


@app.route('/api/undo', methods=['POST'])
def undo_dispatch():
    conn = get_db()
    row = conn.execute(
        "SELECT * FROM rescues WHERE status='dispatched' ORDER BY dispatched_at DESC LIMIT 1"
    ).fetchone()
    if not row:
        conn.close()
        return jsonify({'error': 'No dispatched calls to undo.'}), 400

    conn.execute(
        'UPDATE rescues SET status=?, dispatched_at=NULL WHERE id=?',
        ('pending', row['id'])
    )
    conn.commit()
    updated = conn.execute('SELECT * FROM rescues WHERE id=?', (row['id'],)).fetchone()
    conn.close()
    return jsonify(dict(updated))


@app.route('/api/validate', methods=['POST'])
def validate_field():
    """Validate a single field using the C engine validate command."""
    data = request.get_json()
    field = data.get('field', '')
    value = data.get('value', '')

    if not field:
        return jsonify({'valid': False, 'reason': 'Field name required.'}), 400

    output = call_engine_no_input(['validate', field, value])
    if not output:
        return jsonify({'valid': True, 'reason': ''})

    if 'VALID|1' in output:
        return jsonify({'valid': True, 'reason': ''})
    elif 'VALID|0' in output:
        parts = output.split('|', 2)
        reason = parts[2] if len(parts) >= 3 else 'Validation failed'
        return jsonify({'valid': False, 'reason': reason})

    return jsonify({'valid': True, 'reason': ''})


@app.route('/api/export', methods=['GET'])
def export_report():
    """Generate a text report of all calls and send as downloadable file."""
    conn = get_db()
    pending = conn.execute('SELECT * FROM rescues WHERE status=? ORDER BY urgency DESC',
                           ('pending',)).fetchall()
    dispatched = conn.execute('SELECT * FROM rescues WHERE status=? ORDER BY dispatched_at DESC',
                              ('dispatched',)).fetchall()
    conn.close()

    # Build report text
    lines = []
    lines.append("=" * 56)
    lines.append("  STRAY ANIMAL RESCUE DISPATCH - STATUS REPORT")
    lines.append("  Author: Sahil Kaushik")
    lines.append("  Generated by C DSA Engine + Flask Backend")
    lines.append("=" * 56)
    lines.append("")

    lines.append(f"--- PENDING CALLS ({len(pending)}) ---")
    if pending:
        for r in pending:
            lines.append(f"  Call #{r['id']} | {r['location']} | {r['animal_type']} "
                         f"| {r['description']} | Urgency: {r['urgency']}/10")
    else:
        lines.append("  (None)")
    lines.append("")

    lines.append(f"--- DISPATCHED CALLS ({len(dispatched)}) ---")
    if dispatched:
        for r in dispatched:
            lines.append(f"  Call #{r['id']} | {r['location']} | {r['animal_type']} "
                         f"| Urgency: {r['urgency']}/10 | Dispatched: {r['dispatched_at']}")
    else:
        lines.append("  (None)")
    lines.append("")

    lines.append("--- SUMMARY ---")
    lines.append(f"  Total calls logged    : {len(pending) + len(dispatched)}")
    lines.append(f"  Pending calls         : {len(pending)}")
    lines.append(f"  Dispatched calls      : {len(dispatched)}")
    lines.append("=" * 56)

    report_text = '\n'.join(lines)
    buf = io.BytesIO(report_text.encode('utf-8'))
    buf.seek(0)

    return send_file(
        buf,
        mimetype='text/plain',
        as_attachment=True,
        download_name='rescue_report.txt'
    )


# Initialize DB at module level (gunicorn doesn't run __main__)
init_db()

if __name__ == '__main__':
    if not os.path.exists(ENGINE):
        print(f"[WARNING] C engine not found at {ENGINE}")
        print("  Compile with:  gcc engine.c -o engine\n")
    port = int(os.environ.get('PORT', 5000))
    print("=" * 50)
    print("  STRAY ANIMAL RESCUE DISPATCH SYSTEM")
    print(f"  http://127.0.0.1:{port}")
    print("=" * 50)
    app.run(debug=True, host='0.0.0.0', port=port)
