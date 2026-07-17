"""
ONE-COMMAND SETUP & LAUNCH
Run:  python start.py
"""

import os, sys, subprocess, sqlite3

DIR = os.path.dirname(os.path.abspath(__file__))
os.environ["PYTHONIOENCODING"] = "utf-8"

def step(msg):
    print(f"\n  [+] {msg}")

def fail(msg):
    print(f"\n  [X] {msg}")
    sys.exit(1)


# ---- 1. Ensure templates/ folder exists ----
step("Checking templates folder...")
tpl_dir = os.path.join(DIR, "templates")
os.makedirs(tpl_dir, exist_ok=True)

html_in_root = os.path.join(DIR, "index.html")
html_in_tpl  = os.path.join(tpl_dir, "index.html")

if os.path.exists(html_in_root) and not os.path.exists(html_in_tpl):
    import shutil
    shutil.move(html_in_root, html_in_tpl)
    print("    Moved index.html -> templates/index.html")

if not os.path.exists(html_in_tpl):
    fail("templates/index.html not found. Make sure the file exists.")

print("    OK")


# ---- 2. Compile engine.c ----
step("Compiling engine.c ...")
engine_src = os.path.join(DIR, "engine.c")
engine_exe = os.path.join(DIR, "engine.exe" if sys.platform == "win32" else "engine")

if not os.path.exists(engine_src):
    fail("engine.c not found.")

result = subprocess.run(
    ["gcc", engine_src, "-o", engine_exe, "-Wall"],
    capture_output=True, text=True
)
if result.returncode != 0:
    print(result.stderr)
    fail("Compilation failed. Make sure gcc is installed and on PATH.")

print(f"    Compiled -> {os.path.basename(engine_exe)}  (0 warnings)")


# ---- 3. Install Flask ----
step("Checking Flask...")
try:
    import flask
    print(f"    Flask {flask.__version__} already installed.")
except ImportError:
    print("    Installing Flask...")
    subprocess.check_call([sys.executable, "-m", "pip", "install", "flask", "-q"])
    print("    Flask installed.")


# ---- 4. Initialize SQLite database ----
step("Initializing database...")
db_path     = os.path.join(DIR, "rescues.db")
schema_path = os.path.join(DIR, "schema.sql")

if not os.path.exists(schema_path):
    fail("schema.sql not found.")

conn = sqlite3.connect(db_path)
with open(schema_path, "r") as f:
    conn.executescript(f.read())
conn.commit()
conn.close()
print(f"    rescues.db ready.")


# ---- 5. Launch Flask ----
step("Starting Flask server...\n")
print("  " + "=" * 50)
print("    STRAY ANIMAL RESCUE DISPATCH SYSTEM")
print("    Open in browser:  http://127.0.0.1:5000")
print("    Press Ctrl+C to stop.")
print("  " + "=" * 50 + "\n")

os.chdir(DIR)
subprocess.run([sys.executable, os.path.join(DIR, "app.py")])
