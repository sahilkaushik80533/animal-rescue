"""
Stray Animal Rescue Dispatch System — Flask Backend
Sahil Kaushik

Endpoints:
  GET  /              → Frontend
  GET  /api/rescues   → List calls (?status=pending|dispatched|all)
  POST /api/rescues   → Log a new call
  POST /api/dispatch  → Dispatch highest priority (C engine)
  GET  /api/search    → Search calls (?type=animal|location&q=...)
  GET  /api/sorted    → Pending calls sorted by urgency (C engine)
  GET  /api/stats     → Dashboard counts
"""

from flask import Flask, request, jsonify, render_template
import sqlite3, subprocess, os, sys

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
    for field in ('location', 'animal_type', 'description', 'urgency'):
        if not data.get(field):
            return jsonify({'error': f'Missing: {field}'}), 400

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
    conn.close()
    return jsonify({'total': total, 'pending': pend, 'dispatched': disp})

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
